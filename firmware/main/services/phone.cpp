#include "phone.h"

#include "ble/ble.h"
#include "services/clock.h"
#include "services/settings.h"
#include "ui/text.h"
#include "ui/ui.h"

#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <esp_timer.h>
#include <map>

static const char* TAG = "phone";

namespace phone {
namespace {

constexpr size_t MaxNotifications = 30;

Link currentLink = Link::Off;
bool isBonded = false;
uint32_t rev = 1;
std::deque<Notification> list;
std::map<std::string, std::string> appNames;
bool mediaOk = false;
Media current;
Listener* listener = nullptr;

void changed() {
	rev++;
}

uint32_t nowMs() {
	return uint32_t(esp_timer_get_time() / 1000);
}

// Until the phone answers GetAppAttributes: a few well-known ids, else the
// last part of the bundle id.
std::string fallbackName(const std::string& id) {
	static const std::map<std::string, const char*> known = {
		{ "com.apple.MobileSMS", "Messages" }, { "com.apple.mobilephone", "Phone" },
		{ "com.apple.mobilemail", "Mail" }, { "com.apple.mobilecal", "Calendar" },
		{ "ph.telegra.Telegraph", "Telegram" }, { "net.whatsapp.WhatsApp", "WhatsApp" },
		{ "com.viber", "Viber" }, { "com.facebook.Messenger", "Messenger" },
		{ "com.burbn.instagram", "Instagram" }, { "com.google.Gmail", "Gmail" },
	};
	if (auto it = known.find(id); it != known.end()) return it->second;
	const size_t dot = id.rfind('.');
	return dot == std::string::npos ? id : id.substr(dot + 1);
}

void clean(Notification& n) {
	n.title = text::display(n.title, true);
	n.subtitle = text::display(n.subtitle, true);
	n.message = text::display(n.message);
	n.positive = text::display(n.positive, true);
	n.negative = text::display(n.negative, true);
}

}  // namespace

float Media::position() const {
	float p = elapsed;
	if (state == MediaState::Playing) p += rate * float(nowMs() - elapsedAtMs) / 1000.0f;
	if (duration > 0 && p > duration) p = duration;
	return p < 0 ? 0 : p;
}

Link link() { return currentLink; }
bool bonded() { return isBonded; }
uint32_t revision() { return rev; }
const std::deque<Notification>& notifications() { return list; }

const Notification* find(uint32_t uid) {
	for (const auto& n : list) {
		if (n.uid == uid) return &n;
	}
	return nullptr;
}

std::string appName(const std::string& appId) {
	if (auto it = appNames.find(appId); it != appNames.end() && !it->second.empty()) return it->second;
	return fallbackName(appId);
}

bool hasMedia() { return mediaOk; }
const Media& media() { return current; }

void perform(uint32_t uid, bool positive) {
	ble::notificationAction(uid, positive);
}

void command(MediaCommand cmd) {
	ble::mediaCommand(uint8_t(cmd));
}

void answerPairing(bool accept) {
	ble::answerPairing(accept);
}

void forget() {
	ble::forgetPhone();
	list.clear();
	changed();
}

void setListener(Listener* l) {
	listener = l;
}

namespace feed {

void link(Link l, bool bonded) {
	if (l == currentLink && bonded == isBonded) return;
	currentLink = l;
	isBonded = bonded;
	changed();
}

void notification(Notification n, bool isNew, bool alert) {
	clean(n);
	bool replaced = false;
	for (auto& existing : list) {
		if (existing.uid == n.uid) {
			existing = n;
			replaced = true;
			break;
		}
	}
	if (!replaced) {
		list.push_front(n);
		while (list.size() > MaxNotifications) list.pop_back();
	}
	changed();
	if (!listener) return;
	if (n.category == Category::IncomingCall) {
		if (isNew) listener->callRinging(n);
	} else if (alert) {
		listener->notificationArrived(n);
	}
}

void removed(uint32_t uid) {
	for (auto it = list.begin(); it != list.end(); ++it) {
		if (it->uid == uid) {
			list.erase(it);
			changed();
			return;
		}
	}
}

void clearNotifications() {
	list.clear();
	changed();
}

void appName(const std::string& appId, const std::string& name) {
	appNames[appId] = text::display(name, true);
	changed();
}

void mediaAvailable(bool available) {
	mediaOk = available;
	if (!available) current = Media{};
	changed();
}

void mediaPlayer(const std::string& name) {
	current.player = text::display(name, true);
	changed();
}

void mediaPlayback(MediaState state, float rate, float elapsed) {
	current.state = state;
	current.rate = rate;
	current.elapsed = elapsed;
	current.elapsedAtMs = nowMs();
	changed();
}

void mediaVolume(float volume) {
	current.volume = volume;
	changed();
}

void mediaTrack(TrackField field, const std::string& value) {
	std::string v = text::display(value, true);
	switch (field) {
		case TrackField::Artist: current.artist = std::move(v); break;
		case TrackField::Album: current.album = std::move(v); break;
		case TrackField::Title: current.title = std::move(v); break;
	}
	changed();
}

void mediaDuration(float seconds) {
	current.duration = seconds;
	changed();
}

void pairing(uint32_t code, bool confirm) {
	if (listener) listener->pairingRequested(code, confirm);
}

void pairingDone(PairResult result) {
	if (listener) listener->pairingFinished(result);
}

void time(const tm& local, bool offsetKnown, int offset) {
	auto& settings = ui::devices().settings;
	if (!offsetKnown) {
		// No zone from the phone: read its wall clock in the configured zone.
		tm copy = local;
		copy.tm_isdst = -1;
		clock_svc::setUtc(mktime(&copy));
		return;
	}
	const time_t utc = clock_svc::fromUtcFields(local) - offset;
	clock_svc::setUtc(utc);
	if (clock_svc::utcOffset(utc) == offset) return;
	// The configured zone disagrees with the phone (travel, or never set):
	// switch to the phone's fixed offset. POSIX TZ offsets are west-positive.
	const int west = -offset;
	const int h = (west < 0 ? -west : west) / 3600;
	const int m = ((west < 0 ? -west : west) % 3600) / 60;
	char tz[sizeof(SettingsData::timezone)];
	if (m) snprintf(tz, sizeof(tz), "UTC%s%d:%02d", west < 0 ? "-" : "", h, m);
	else snprintf(tz, sizeof(tz), "UTC%s%d", west < 0 ? "-" : "", h);
	ESP_LOGI(TAG, "time zone from phone: %s (was %s)", tz, settings.get().timezone);
	strlcpy(settings.get().timezone, tz, sizeof(settings.get().timezone));
	settings.save();
	clock_svc::setTimezone(tz);
}

}  // namespace feed
}  // namespace phone
