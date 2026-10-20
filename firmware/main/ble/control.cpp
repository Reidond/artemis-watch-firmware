#include "control.h"
#include "ble.h"

#include "board/board.h"
#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/buzzer.h"
#include "i18n/i18n.h"
#include "services/clock.h"
#include "services/mission.h"
#include "services/ota.h"
#include "services/settings.h"
#include "services/weather.h"
#include "ui/screens/screens.h"
#include "ui/ui.h"

#include <cJSON.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <host/ble_hs.h>
#include <nvs.h>
#include <services/gatt/ble_svc_gatt.h>
#include <nimble/nimble_port.h>
#include <optional>
#include <string>

static const char* TAG = "control";

namespace ble::control {
namespace {

// 9B2A000N-6E1D-4F3C-B8A5-2C7D1E0F4A93, little-endian as NimBLE wants it.
#define ARTEMIS_UUID(n) \
	BLE_UUID128_INIT(0x93, 0x4A, 0x0F, 0x1E, 0x7D, 0x2C, 0xA5, 0xB8, 0x3C, 0x4F, 0x1D, 0x6E, n, 0x00, 0x2A, 0x9B)

const ble_uuid128_t ServiceUuid = ARTEMIS_UUID(0x00);
const ble_uuid128_t InfoUuid = ARTEMIS_UUID(0x01);
const ble_uuid128_t SettingsUuid = ARTEMIS_UUID(0x02);
const ble_uuid128_t WeatherUuid = ARTEMIS_UUID(0x03);
const ble_uuid128_t CommandUuid = ARTEMIS_UUID(0x04);
const ble_uuid128_t OtaControlUuid = ARTEMIS_UUID(0x05);
const ble_uuid128_t OtaDataUuid = ARTEMIS_UUID(0x06);

enum Chr : uintptr_t { Info, SettingsChr, WeatherChr, Command, OtaControl, OtaData, ChrCount };

uint16_t handles[ChrCount] = {};
bool subscribed[ChrCount] = {};
uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
ble_npl_callout infoTimer;
constexpr uint32_t InfoEveryMs = 30 * 60 * 1000;

int access(uint16_t connHandle, uint16_t attr, ble_gatt_access_ctxt* ctxt, void* arg);

const ble_gatt_chr_def Characteristics[] = {
	{ .uuid = &InfoUuid.u, .access_cb = access, .arg = (void*) Info,
		.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY, .val_handle = &handles[Info] },
	{ .uuid = &SettingsUuid.u, .access_cb = access, .arg = (void*) SettingsChr,
		.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
			BLE_GATT_CHR_F_NOTIFY,
		.val_handle = &handles[SettingsChr] },
	{ .uuid = &WeatherUuid.u, .access_cb = access, .arg = (void*) WeatherChr,
		.flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC, .val_handle = &handles[WeatherChr] },
	{ .uuid = &CommandUuid.u, .access_cb = access, .arg = (void*) Command,
		.flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC, .val_handle = &handles[Command] },
	{ .uuid = &OtaControlUuid.u, .access_cb = access, .arg = (void*) OtaControl,
		.flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_NOTIFY,
		.val_handle = &handles[OtaControl] },
	{ .uuid = &OtaDataUuid.u, .access_cb = access, .arg = (void*) OtaData,
		.flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC, .val_handle = &handles[OtaData] },
	{},
};

const ble_gatt_svc_def Services[] = {
	{ .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &ServiceUuid.u, .characteristics = Characteristics },
	{},
};

// --- JSON ---------------------------------------------------------------

std::string infoJson() {
	auto& dev = ui::devices();
	char buf[200];
	snprintf(buf, sizeof(buf),
		"{\"fw\":\"%s\",\"hw\":%u,\"bat\":%d,\"mv\":%d,\"chg\":%d,\"t\":%lld,\"wx\":%lld,\"day\":%d}",
		esp_app_get_description()->version, board::identity().revision, dev.battery.percent(), dev.battery.millivolts(),
		int(dev.battery.charge()), (long long) time(nullptr), (long long) weather::lastUpdate(),
		mission::day(time(nullptr)));
	return buf;
}

std::string settingsJson() {
	SettingsData d;
	if (ui::lock(200)) {
		d = ui::devices().settings.get();
		ui::unlock();
	}
	char buf[240];
	snprintf(buf, sizeof(buf),
		"{\"lang\":\"%s\",\"bright\":%u,\"timeout\":%u,\"h24\":%s,\"flip\":%s,\"sound\":%s,\"tz\":\"%s\",\"epoch\":%lld}",
		d.language == Lang::Uk ? "uk" : "en", d.brightness, Settings::ScreenTimeouts[d.screenTimeoutIdx],
		d.use24h ? "true" : "false", d.flipped ? "true" : "false", d.sounds ? "true" : "false", d.timezone,
		(long long) mission::epoch());
	return buf;
}

struct SettingsPatch {
	std::optional<Lang> lang;
	std::optional<int> bright, timeout;
	std::optional<bool> h24, flip, sound;
	std::optional<std::string> tz;
	std::optional<int64_t> epoch;
};

std::optional<bool> boolean(const cJSON* root, const char* key) {
	const cJSON* v = cJSON_GetObjectItem(root, key);
	if (!cJSON_IsBool(v)) return std::nullopt;
	return cJSON_IsTrue(v);
}

std::optional<double> number(const cJSON* root, const char* key) {
	const cJSON* v = cJSON_GetObjectItem(root, key);
	if (!cJSON_IsNumber(v)) return std::nullopt;
	return v->valuedouble;
}

bool parseSettings(const char* json, size_t len, SettingsPatch& p) {
	cJSON* root = cJSON_ParseWithLength(json, len);
	if (!cJSON_IsObject(root)) {
		cJSON_Delete(root);
		return false;
	}
	if (const cJSON* v = cJSON_GetObjectItem(root, "lang"); cJSON_IsString(v)) {
		if (!strcmp(v->valuestring, "uk")) p.lang = Lang::Uk;
		if (!strcmp(v->valuestring, "en")) p.lang = Lang::En;
	}
	if (auto v = number(root, "bright")) p.bright = int(*v);
	if (auto v = number(root, "timeout")) p.timeout = int(*v);
	p.h24 = boolean(root, "h24");
	p.flip = boolean(root, "flip");
	p.sound = boolean(root, "sound");
	if (const cJSON* v = cJSON_GetObjectItem(root, "tz"); cJSON_IsString(v)) p.tz = v->valuestring;
	if (auto v = number(root, "epoch")) p.epoch = int64_t(*v);
	cJSON_Delete(root);
	return true;
}

// Runs on the UI task.
void applySettings(const SettingsPatch& p) {
	auto& dev = ui::devices();
	auto& d = dev.settings.get();
	bool rebuild = false;
	if (p.lang && *p.lang != d.language) {
		d.language = *p.lang;
		i18n::setLanguage(d.language);
		rebuild = true;
	}
	if (p.bright) {
		constexpr int Steps[] = { 20, 40, 60, 80, 100 };
		int best = Steps[0];
		for (int s : Steps) {
			if (std::abs(s - *p.bright) < std::abs(best - *p.bright)) best = s;
		}
		d.brightness = uint8_t(best);
		if (ui::isScreenOn()) dev.backlight.set(d.brightness);
	}
	if (p.timeout) {
		for (int i = 0; i < Settings::ScreenTimeoutCount; i++) {
			if (Settings::ScreenTimeouts[i] == *p.timeout) d.screenTimeoutIdx = uint8_t(i);
		}
	}
	if (p.h24 && *p.h24 != d.use24h) {
		d.use24h = *p.h24;
		rebuild = true;
	}
	if (p.flip && *p.flip != d.flipped) {
		d.flipped = *p.flip;
		ui::setFlipped(d.flipped);
	}
	if (p.sound) {
		d.sounds = *p.sound;
		dev.buzzer.setEnabled(d.sounds);
	}
	if (p.tz && !p.tz->empty() && *p.tz != d.timezone) {
		strlcpy(d.timezone, p.tz->c_str(), sizeof(d.timezone));
		clock_svc::setTimezone(d.timezone);
		rebuild = true;
	}
	if (p.epoch && *p.epoch > 0 && *p.epoch != mission::epoch()) {
		mission::setEpoch(time_t(*p.epoch));
		rebuild = true;
	}
	dev.settings.save();  // notifies the app through Settings::onSave
	if (rebuild) ui::rebuildAll();
}

// --- GATT ---------------------------------------------------------------

void send(Chr chr, const std::string& json) {
	if (conn == BLE_HS_CONN_HANDLE_NONE || !subscribed[chr]) return;
	os_mbuf* om = ble_hs_mbuf_from_flat(json.data(), json.size());
	if (!om) return;
	const int rc = ble_gatts_notify_custom(conn, handles[chr], om);
	if (rc != 0) ESP_LOGW(TAG, "notify %u: %d", unsigned(chr), rc);
}

void onOtaReport(const char* json) {
	const std::string copy(json);
	ble::post([copy] { send(OtaControl, copy); });
	if (strstr(json, "\"op\":\"begin\"") && strstr(json, "\"ok\":true")) {
		ui::post([] { ui::push(makeUpdate()); });
	}
}

void handleOtaControl(const char* json, size_t len) {
	cJSON* root = cJSON_ParseWithLength(json, len);
	const cJSON* op = cJSON_GetObjectItem(root, "op");
	if (!cJSON_IsString(op)) {
		cJSON_Delete(root);
		return;
	}
	if (!strcmp(op->valuestring, "begin")) {
		const cJSON* size = cJSON_GetObjectItem(root, "size");
		const cJSON* sha = cJSON_GetObjectItem(root, "sha256");
		uint8_t digest[32] = {};
		bool ok = cJSON_IsNumber(size) && cJSON_IsString(sha) && strlen(sha->valuestring) == 64;
		for (int i = 0; ok && i < 32; i++) {
			unsigned v = 0;
			ok = sscanf(sha->valuestring + 2 * i, "%2x", &v) == 1;
			digest[i] = uint8_t(v);
		}
		if (ok) ota::begin(size_t(size->valuedouble), digest);
		else send(OtaControl, "{\"ok\":false,\"err\":\"bad begin\"}");
	} else if (!strcmp(op->valuestring, "end")) {
		ota::end();
	} else if (!strcmp(op->valuestring, "abort")) {
		ota::abort();
	}
	cJSON_Delete(root);
}

void handleCommand(const char* json, size_t len) {
	cJSON* root = cJSON_ParseWithLength(json, len);
	const cJSON* op = cJSON_GetObjectItem(root, "op");
	if (cJSON_IsString(op) && !strcmp(op->valuestring, "find")) {
		ui::post([] { ui::push(makeFind()); });
		ui::wake();
	}
	cJSON_Delete(root);
}

int access(uint16_t connHandle, uint16_t, ble_gatt_access_ctxt* ctxt, void* arg) {
	const auto chr = Chr(reinterpret_cast<uintptr_t>(arg));
	if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
		const std::string json = chr == Info ? infoJson() : settingsJson();
		return os_mbuf_append(ctxt->om, json.data(), json.size()) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}
	if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

	static uint8_t buf[520];
	uint16_t len = 0;
	if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len) != 0) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	buf[len] = 0;
	const char* text = reinterpret_cast<const char*>(buf);

	switch (chr) {
		case OtaData:
			ota::data(buf, len);
			return 0;
		case SettingsChr: {
			SettingsPatch patch;
			if (!parseSettings(text, len, patch)) return BLE_ATT_ERR_UNLIKELY;
			ui::post([patch] { applySettings(patch); });
			return 0;
		}
		case WeatherChr: {
			weather::Data data;
			if (!weather::parse(text, len, data)) return BLE_ATT_ERR_UNLIKELY;
			ESP_LOGI(TAG, "weather %.1f°C code %u, %s", data.temp, data.code, data.place);
			ui::post([data] { weather::set(data); });
			return 0;
		}
		case Command:
			handleCommand(text, len);
			return 0;
		case OtaControl:
			handleOtaControl(text, len);
			return 0;
		default:
			return BLE_ATT_ERR_UNLIKELY;
	}
}

void onInfoTimer(ble_npl_event*) {
	send(Info, infoJson());
	ble_npl_callout_reset(&infoTimer, ble_npl_time_ms_to_ticks32(InfoEveryMs));
}

}  // namespace

const ble_uuid128_t* service() {
	return &ServiceUuid;
}

void registerService() {
	int rc = ble_gatts_count_cfg(Services);
	if (rc == 0) rc = ble_gatts_add_svcs(Services);
	if (rc != 0) ESP_LOGE(TAG, "register service: %d", rc);
	ble_npl_callout_init(&infoTimer, nimble_port_get_dflt_eventq(), onInfoTimer, nullptr);
	ota::init(onOtaReport);
	if (ui::lock(1000)) {
		ui::devices().settings.onSave = [] { notifySettings(); };
		ui::unlock();
	}
}

void onConnect(uint16_t c) {
	conn = c;
	for (bool& s : subscribed) s = false;
}

void onEncrypted() {
	// Bump when services or characteristics change.
	constexpr uint8_t GattLayout = 2;  // 1: v0.2 (no Artemis Control), 2: v0.4
	nvs_handle_t h;
	if (nvs_open("artemis", NVS_READWRITE, &h) != ESP_OK) return;
	uint8_t seen = 0;
	nvs_get_u8(h, "gattlayout", &seen);
	if (seen != GattLayout) {
		ESP_LOGI(TAG, "GATT layout %u -> %u: service changed", seen, GattLayout);
		ble_svc_gatt_changed(0x0001, 0xFFFF);
		nvs_set_u8(h, "gattlayout", GattLayout);
		nvs_commit(h);
	}
	nvs_close(h);
}

void onDisconnect() {
	conn = BLE_HS_CONN_HANDLE_NONE;
	for (bool& s : subscribed) s = false;
	ble_npl_callout_stop(&infoTimer);
	if (ota::active()) ota::abort();
}

void onSubscribe(const ble_gap_event& event) {
	for (int i = 0; i < ChrCount; i++) {
		if (handles[i] != event.subscribe.attr_handle) continue;
		subscribed[i] = event.subscribe.cur_notify;
		ESP_LOGI(TAG, "chr %d notify %d", i, subscribed[i]);
		// Fresh state as soon as the app listens.
		if (subscribed[i] && i == Info) onInfoTimer(nullptr);
		if (subscribed[i] && i == SettingsChr) send(SettingsChr, settingsJson());
	}
}

void notifyInfo() {
	ble::post([] { send(Info, infoJson()); });
}

void notifySettings() {
	ble::post([] { send(SettingsChr, settingsJson()); });
}

}  // namespace ble::control
