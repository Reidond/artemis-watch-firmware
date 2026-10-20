// ANCS client. Protocol: Apple "ANCS Specification" (Notification Source,
// Control Point, Data Source). Request handling follows the stock firmware's
// main/src/Notifs/ANCS/Client.cpp: one GetNotificationAttributes in flight,
// a timeout for UIDs the phone no longer knows, newest notifications first.
#include "apple.h"

#include "services/phone.h"
#include "ui/ui.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <esp_log.h>
#include <map>
#include <nimble/nimble_port.h>
#include <optional>
#include <set>
#include <string>

static const char* TAG = "ancs";

namespace ble::ancs {
namespace {

// 7905F431-B5CE-4E99-A40F-4B1E122D00D0
const ble_uuid128_t Service = BLE_UUID128_INIT(0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5,
	0x31, 0xF4, 0x05, 0x79);
// 9FBF120D-6301-42D9-8C58-25E699A21DBD
const ble_uuid128_t NotificationSource = BLE_UUID128_INIT(0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42,
	0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F);
// 69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9
const ble_uuid128_t ControlPoint = BLE_UUID128_INIT(0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1,
	0x45, 0xF3, 0xD8, 0xD1, 0x69);
// 22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB
const ble_uuid128_t DataSource = BLE_UUID128_INIT(0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6,
	0x24, 0xE9, 0xC6, 0xEA, 0x22);

enum Command : uint8_t { GetNotificationAttributes = 0, GetAppAttributes = 1, PerformNotificationAction = 2 };
enum Event : uint8_t { Added = 0, Modified = 1, Removed = 2 };
enum Attribute : uint8_t {
	AppIdentifier = 0, Title = 1, Subtitle = 2, Message = 3, MessageSize = 4, Date = 5, PositiveLabel = 6,
	NegativeLabel = 7, AttributeCount
};
constexpr uint8_t AppDisplayName = 0;

struct Wanted {
	Attribute id;
	uint16_t maxLen;  // 0: the attribute takes no length parameter
};
// Lengths are bytes of UTF-8: Cyrillic needs two per letter.
constexpr Wanted NotificationAttributes[] = {
	{ AppIdentifier, 0 }, { Title, 96 }, { Subtitle, 64 }, { Message, 400 },
	{ Date, 0 }, { PositiveLabel, 0 }, { NegativeLabel, 0 },
};
constexpr size_t WantedCount = sizeof(NotificationAttributes) / sizeof(NotificationAttributes[0]);

struct Request {
	bool app = false;  // GetAppAttributes for appId, else GetNotificationAttributes for uid
	uint32_t uid = 0;
	uint8_t category = 0;
	uint8_t flags = 0;
	bool isNew = false;  // Added (incl. pre-existing), as opposed to Modified
	std::string appId;
};

constexpr size_t MaxQueued = 32;
constexpr uint32_t TimeoutMs = 3000;
constexpr size_t MaxResponse = 2048;

uint16_t controlPoint = 0;
bool ready = false;
std::deque<Request> queue;
std::optional<Request> inFlight;
uint32_t sequence = 0;   // identifies the in-flight request in its write callback
bool abandoned = false;  // the phone removed the in-flight notification meanwhile
std::vector<uint8_t> response;
ble_npl_callout timeout;
bool timeoutReady = false;
// Display names learned from the phone, kept across reconnects.
std::map<std::string, std::string> appNames;
std::set<std::string> appsAsked;

void pump();

void finish() {
	ble_npl_callout_stop(&timeout);
	inFlight.reset();
	response.clear();
	abandoned = false;
	pump();
}

void onTimeout(ble_npl_event*) {
	if (!inFlight) return;
	ESP_LOGW(TAG, "no answer for %s", inFlight->app ? inFlight->appId.c_str() : "notification");
	finish();
}

void putUid(std::vector<uint8_t>& out, uint32_t uid) {
	for (int i = 0; i < 4; i++) out.push_back(uint8_t(uid >> (8 * i)));
}

uint32_t getUid(const uint8_t* p) {
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

void pump() {
	if (!ready || inFlight || queue.empty()) return;
	inFlight = std::move(queue.front());
	queue.pop_front();
	response.clear();
	abandoned = false;
	const uint32_t seq = ++sequence;

	std::vector<uint8_t> cmd;
	if (inFlight->app) {
		cmd.push_back(GetAppAttributes);
		cmd.insert(cmd.end(), inFlight->appId.begin(), inFlight->appId.end());
		cmd.push_back(0);
		cmd.push_back(AppDisplayName);
	} else {
		cmd.push_back(GetNotificationAttributes);
		putUid(cmd, inFlight->uid);
		for (const auto& a : NotificationAttributes) {
			cmd.push_back(a.id);
			if (a.maxLen) {
				cmd.push_back(uint8_t(a.maxLen));
				cmd.push_back(uint8_t(a.maxLen >> 8));
			}
		}
	}
	gatt::write(controlPoint, std::move(cmd), [seq](int status) {
		if (!inFlight || seq != sequence) return;
		if (status == 0) {
			ble_npl_callout_reset(&timeout, ble_npl_time_ms_to_ticks32(TimeoutMs));
			return;
		}
		// 0x1A2 (Invalid parameter): the UID is gone already.
		ESP_LOGW(TAG, "request refused: 0x%x", status);
		finish();
	});
}

void parseDate(const std::string& s, phone::Notification& n) {
	// yyyyMMdd'T'HHmmSS, local time
	if (s.size() < 13 || s[8] != 'T') return;
	const auto num = [&](size_t at, size_t len) { return atoi(s.substr(at, len).c_str()); };
	n.year = uint16_t(num(0, 4));
	n.month = uint8_t(num(4, 2));
	n.day = uint8_t(num(6, 2));
	n.hour = uint8_t(num(9, 2));
	n.minute = uint8_t(num(11, 2));
}

void deliver(const Request& req, std::string (&attr)[AttributeCount]) {
	phone::Notification n;
	n.uid = req.uid;
	n.category = phone::Category(req.category);
	n.flags = req.flags;
	n.appId = attr[AppIdentifier];
	n.title = attr[Title];
	n.subtitle = attr[Subtitle];
	n.message = attr[Message];
	if (req.flags & phone::flag::PositiveAction) n.positive = attr[PositiveLabel];
	if (req.flags & phone::flag::NegativeAction) n.negative = attr[NegativeLabel];
	parseDate(attr[Date], n);

	const bool alert = req.isNew && !(req.flags & (phone::flag::Silent | phone::flag::PreExisting));
	ESP_LOGI(TAG, "%s 0x%08lx cat=%u flags=0x%02x app=%s", req.isNew ? "new" : "changed", (unsigned long) n.uid,
		req.category, req.flags, n.appId.c_str());

	if (!n.appId.empty() && !appNames.count(n.appId) && appsAsked.insert(n.appId).second) {
		Request app;
		app.app = true;
		app.appId = n.appId;
		queue.push_back(std::move(app));
	}
	ui::post([n = std::move(n), isNew = req.isNew, alert]() mutable {
		phone::feed::notification(std::move(n), isNew, alert);
	});
}

// Parses the accumulated Data Source bytes; acts once the response is whole.
void parse() {
	const auto& r = response;
	if (r.empty()) return;
	if (r.size() > MaxResponse) {
		ESP_LOGW(TAG, "response too long, dropped");
		finish();
		return;
	}
	if (inFlight->app) {
		if (r[0] != GetAppAttributes) {
			response.clear();
			return;
		}
		const auto nul = std::find(r.begin() + 1, r.end(), 0);
		if (nul == r.end()) return;
		size_t pos = size_t(nul - r.begin()) + 1;
		if (pos + 3 > r.size()) return;
		const size_t len = r[pos + 1] | r[pos + 2] << 8;
		if (pos + 3 + len > r.size()) return;
		const std::string appId(r.begin() + 1, nul);
		const std::string name(r.begin() + pos + 3, r.begin() + pos + 3 + len);
		appNames[appId] = name;
		ui::post([appId, name] { phone::feed::appName(appId, name); });
		finish();
		return;
	}

	if (r[0] != GetNotificationAttributes || (r.size() >= 5 && getUid(&r[1]) != inFlight->uid)) {
		ESP_LOGW(TAG, "unexpected data source bytes, dropped");
		response.clear();
		return;
	}
	if (r.size() < 5) return;
	std::string attr[AttributeCount];
	size_t pos = 5;
	for (size_t got = 0; got < WantedCount; got++) {
		if (pos + 3 > r.size()) return;
		const uint8_t id = r[pos];
		const size_t len = r[pos + 1] | r[pos + 2] << 8;
		if (pos + 3 + len > r.size()) return;
		if (id < AttributeCount) attr[id].assign(reinterpret_cast<const char*>(&r[pos + 3]), len);
		pos += 3 + len;
	}
	const Request req = *inFlight;
	if (!abandoned) deliver(req, attr);
	finish();
}

void onDataSource(const uint8_t* data, size_t len) {
	if (!inFlight) return;  // tail of a request that already timed out
	response.insert(response.end(), data, data + len);
	parse();
}

void onNotificationSource(const uint8_t* data, size_t len) {
	if (len < 8) return;
	const uint8_t event = data[0];
	const uint8_t flags = data[1];
	const uint8_t category = data[2];
	const uint32_t uid = getUid(data + 4);

	if (event == Removed) {
		for (auto it = queue.begin(); it != queue.end(); ++it) {
			if (!it->app && it->uid == uid) {
				queue.erase(it);
				break;
			}
		}
		if (inFlight && !inFlight->app && inFlight->uid == uid) abandoned = true;
		ui::post([uid] { phone::feed::removed(uid); });
		return;
	}
	if (event != Added && event != Modified) return;

	// iOS repeats Modified for live notifications; fold repeats into the queued request.
	for (auto& q : queue) {
		if (!q.app && q.uid == uid) {
			q.flags = flags;
			q.isNew = q.isNew || event == Added;
			return;
		}
	}
	Request req;
	req.uid = uid;
	req.category = category;
	req.flags = flags;
	req.isNew = event == Added;
	if (category == uint8_t(phone::Category::IncomingCall)) {
		queue.push_front(std::move(req));
	} else {
		queue.push_back(std::move(req));
	}
	// The phone replays existing notifications oldest first: keep the newest.
	while (queue.size() > MaxQueued) {
		auto it = queue.begin();
		while (it != queue.end() && (it->app || it->category == uint8_t(phone::Category::IncomingCall))) ++it;
		if (it == queue.end()) break;
		queue.erase(it);
	}
	pump();
}

}  // namespace

const ble_uuid_t* service() {
	return &Service.u;
}

void attach(const gatt::Service& svc) {
	const auto* ns = svc.find(&NotificationSource.u);
	const auto* cp = svc.find(&ControlPoint.u);
	const auto* ds = svc.find(&DataSource.u);
	if (!ns || !cp || !ds || !ns->cccd || !ds->cccd) {
		ESP_LOGW(TAG, "service incomplete");
		return;
	}
	if (!timeoutReady) {
		ble_npl_callout_init(&timeout, nimble_port_get_dflt_eventq(), onTimeout, nullptr);
		timeoutReady = true;
	}
	detach();
	controlPoint = cp->value;
	gatt::listen(ds->value, onDataSource);
	gatt::listen(ns->value, onNotificationSource);
	ui::post([] { phone::feed::clearNotifications(); });
	// Data Source first: as soon as Notification Source is on, the phone
	// replays every notification it holds.
	gatt::subscribe(ds->cccd, 1, [](int status) {
		if (status != 0) ESP_LOGW(TAG, "data source subscribe: 0x%x", status);
	});
	gatt::subscribe(ns->cccd, 1, [](int status) {
		if (status != 0) {
			ESP_LOGW(TAG, "notification source subscribe: 0x%x", status);
			return;
		}
		ESP_LOGI(TAG, "subscribed");
		ready = true;
		pump();
	});
}

void detach() {
	ready = false;
	controlPoint = 0;
	queue.clear();
	inFlight.reset();
	response.clear();
	abandoned = false;
	if (timeoutReady) ble_npl_callout_stop(&timeout);
}

bool attached() {
	return ready;
}

void perform(uint32_t uid, bool positive) {
	if (!ready) return;
	std::vector<uint8_t> cmd{ PerformNotificationAction };
	putUid(cmd, uid);
	cmd.push_back(positive ? 0 : 1);
	ESP_LOGI(TAG, "%s action on 0x%08lx", positive ? "positive" : "negative", (unsigned long) uid);
	gatt::write(controlPoint, std::move(cmd), [](int status) {
		if (status != 0) ESP_LOGW(TAG, "action failed: 0x%x", status);
	});
}

}  // namespace ble::ancs
