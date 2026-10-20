// AMS client. Protocol: Apple "Apple Media Service Specification". The phone
// pushes one Entity Update per attribute; values are UTF-8 strings.
#include "apple.h"

#include "services/phone.h"
#include "ui/ui.h"

#include <cstdlib>
#include <esp_log.h>
#include <string>

static const char* TAG = "ams";

namespace ble::ams {
namespace {

// 89D3502B-0F36-433A-8EF4-C502AD55F8DC
const ble_uuid128_t Service = BLE_UUID128_INIT(0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F,
	0x2B, 0x50, 0xD3, 0x89);
// 9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2
const ble_uuid128_t RemoteCommand = BLE_UUID128_INIT(0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8, 0x8A, 0x4A, 0xB1,
	0x57, 0xD8, 0x81, 0x3C, 0x9B);
// 2F7CABCE-808D-411F-9A0C-BB92BA96C102
const ble_uuid128_t EntityUpdate = BLE_UUID128_INIT(0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D,
	0x80, 0xCE, 0xAB, 0x7C, 0x2F);

enum Entity : uint8_t { Player = 0, Queue = 1, Track = 2 };
enum PlayerAttr : uint8_t { PlayerName = 0, PlaybackInfo = 1, Volume = 2 };
enum TrackAttr : uint8_t { Artist = 0, Album = 1, Title = 2, Duration = 3 };
enum PlaybackState : uint8_t { Paused = 0, Playing = 1, Rewinding = 2, FastForwarding = 3 };

uint16_t remoteCommand = 0;
bool ready = false;

void onEntityUpdate(const uint8_t* data, size_t len) {
	if (len < 3) return;
	const uint8_t entity = data[0];
	const uint8_t attr = data[1];
	// data[2] bit 0: truncated. Long titles scroll anyway, so the full value is not fetched.
	const std::string value(reinterpret_cast<const char*>(data + 3), len - 3);

	if (entity == Player && attr == PlayerName) {
		ui::post([value] { phone::feed::mediaPlayer(value); });
	} else if (entity == Player && attr == PlaybackInfo) {
		// "state,rate,elapsed", empty when nothing is loaded
		phone::MediaState state = phone::MediaState::Stopped;
		float rate = 0, elapsed = 0;
		if (!value.empty()) {
			char* end = nullptr;
			const long s = strtol(value.c_str(), &end, 10);
			if (end && *end == ',') rate = strtof(end + 1, &end);
			if (end && *end == ',') elapsed = strtof(end + 1, &end);
			state = s == Paused ? phone::MediaState::Paused : phone::MediaState::Playing;
		}
		ui::post([state, rate, elapsed] { phone::feed::mediaPlayback(state, rate, elapsed); });
	} else if (entity == Player && attr == Volume) {
		const float v = value.empty() ? -1 : strtof(value.c_str(), nullptr);
		ui::post([v] { phone::feed::mediaVolume(v); });
	} else if (entity == Track && attr <= Title) {
		const auto field = attr == Artist ? phone::feed::TrackField::Artist
			: attr == Album               ? phone::feed::TrackField::Album
										  : phone::feed::TrackField::Title;
		ui::post([field, value] { phone::feed::mediaTrack(field, value); });
	} else if (entity == Track && attr == Duration) {
		const float d = value.empty() ? 0 : strtof(value.c_str(), nullptr);
		ui::post([d] { phone::feed::mediaDuration(d); });
	}
}

void onRemoteCommands(const uint8_t* data, size_t len) {
	// The commands the current player accepts. All the watch sends is common to every player.
	ESP_LOGD(TAG, "%u commands available", unsigned(len));
}

}  // namespace

const ble_uuid_t* service() {
	return &Service.u;
}

void attach(const gatt::Service& svc) {
	const auto* rc = svc.find(&RemoteCommand.u);
	const auto* eu = svc.find(&EntityUpdate.u);
	if (!rc || !eu || !eu->cccd) {
		ESP_LOGW(TAG, "service incomplete");
		return;
	}
	remoteCommand = rc->value;
	if (rc->cccd) {
		gatt::listen(rc->value, onRemoteCommands);
		gatt::subscribe(rc->cccd, 1);
	}
	gatt::listen(eu->value, onEntityUpdate);
	gatt::subscribe(eu->cccd, 1, [](int status) {
		if (status != 0) ESP_LOGW(TAG, "entity update subscribe: 0x%x", status);
	});
	gatt::write(eu->value, { Player, PlayerName, PlaybackInfo, Volume });
	gatt::write(eu->value, { Track, Artist, Album, Title, Duration }, [](int status) {
		if (status != 0) {
			ESP_LOGW(TAG, "register: 0x%x", status);
			return;
		}
		ESP_LOGI(TAG, "subscribed");
		ready = true;
		ui::post([] { phone::feed::mediaAvailable(true); });
	});
}

void detach() {
	if (ready) ui::post([] { phone::feed::mediaAvailable(false); });
	ready = false;
	remoteCommand = 0;
}

bool attached() {
	return ready;
}

void command(uint8_t cmd) {
	if (!ready) return;
	gatt::write(remoteCommand, { cmd }, [cmd](int status) {
		if (status != 0) ESP_LOGW(TAG, "command %u: 0x%x", cmd, status);
	});
}

}  // namespace ble::ams
