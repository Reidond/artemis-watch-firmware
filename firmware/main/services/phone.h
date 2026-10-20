#pragma once

#include <cstdint>
#include <deque>
#include <string>

// What the iPhone told us, mirrored for the UI. Everything here runs on the UI
// task: the BLE clients (ble/) deliver updates through ui::post, screens read
// the state directly and poll revision() to notice changes.
namespace phone {

enum class Link : uint8_t {
	Off,          // Bluetooth not running
	Advertising,  // waiting for the iPhone
	Connected,    // link up, securing and discovering services
	Ready,        // encrypted, Apple services subscribed
};

// ANCS CategoryID.
enum class Category : uint8_t {
	Other, IncomingCall, MissedCall, Voicemail, Social, Schedule, Email, News, Health, Business, Location, Entertainment,
};

// ANCS EventFlags.
namespace flag {
constexpr uint8_t Silent = 1 << 0;
constexpr uint8_t Important = 1 << 1;
constexpr uint8_t PreExisting = 1 << 2;
constexpr uint8_t PositiveAction = 1 << 3;
constexpr uint8_t NegativeAction = 1 << 4;
}  // namespace flag

struct Notification {
	uint32_t uid = 0;
	Category category = Category::Other;
	uint8_t flags = 0;
	std::string appId;
	std::string title, subtitle, message;  // display-ready (ui/text.h)
	std::string positive, negative;        // action labels from iOS, in the phone's language
	// Local time the phone stamped it with; year 0 if missing.
	uint16_t year = 0;
	uint8_t month = 0, day = 0, hour = 0, minute = 0;
};

enum class MediaState : uint8_t { Stopped, Paused, Playing };

struct Media {
	MediaState state = MediaState::Stopped;
	std::string player, title, artist, album;
	float duration = 0;  // s, 0 if unknown
	float elapsed = 0;   // s, at elapsedAtMs
	float rate = 0;      // playback speed
	uint32_t elapsedAtMs = 0;
	float volume = -1;   // 0..1, -1 if unknown

	float position() const;  // now, extrapolated from elapsed and rate
};

// AMS RemoteCommandID.
enum class MediaCommand : uint8_t { Play, Pause, Toggle, Next, Previous, VolumeUp, VolumeDown };

Link link();
bool bonded();
uint32_t revision();  // changes whenever anything below changes

const std::deque<Notification>& notifications();  // newest first, calls included
const Notification* find(uint32_t uid);
std::string appName(const std::string& appId);    // the phone's name for the app, or a fallback

bool hasMedia();  // the phone exposes AMS
const Media& media();

// User actions, forwarded to the phone.
void perform(uint32_t uid, bool positive);
void command(MediaCommand cmd);
void answerPairing(bool accept);
void forget();

enum class PairResult : uint8_t { Ok, Failed, KeyMissing };

// UI reactions to phone events, implemented by the screens.
struct Listener {
	virtual void notificationArrived(const Notification& n) = 0;  // new and not silent
	virtual void callRinging(const Notification& n) = 0;
	virtual void pairingRequested(uint32_t code, bool confirm) = 0;
	virtual void pairingFinished(PairResult result) = 0;
};
void setListener(Listener* listener);

// Updates from ble/, already on the UI task. Strings arrive raw (UTF-8 from
// the phone) and are cleaned up here.
namespace feed {
void link(Link link, bool bonded);
void notification(Notification n, bool isNew, bool alert);
void removed(uint32_t uid);
void clearNotifications();
void appName(const std::string& appId, const std::string& name);
void mediaAvailable(bool available);
void mediaPlayer(const std::string& name);
void mediaPlayback(MediaState state, float rate, float elapsed);
void mediaVolume(float volume);
enum class TrackField : uint8_t { Artist, Album, Title };
void mediaTrack(TrackField field, const std::string& value);
void mediaDuration(float seconds);
void pairing(uint32_t code, bool confirm);
void pairingDone(PairResult result);
// Current Time Service: the phone's local wall-clock time and, when known, its
// UTC offset in seconds (time zone + DST).
void time(const struct tm& local, bool offsetKnown, int offsetSeconds);
}  // namespace feed

}  // namespace phone
