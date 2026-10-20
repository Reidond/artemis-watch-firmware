#include "weather.h"
#include "ui/art/sprites.h"

#include <cJSON.h>
#include <cmath>
#include <cstring>
#include <nvs.h>

namespace {

constexpr const char* Namespace = "artemis";
constexpr const char* Key = "weather";
constexpr uint32_t Version = 1;
constexpr int64_t MaxAgeSeconds = 12 * 3600;

struct Stored {
	uint32_t version;
	weather::Data data;
};

weather::Data latest;
bool have = false;

}  // namespace

namespace weather {

bool parse(const char* json, size_t len, Data& out) {
	cJSON* root = cJSON_ParseWithLength(json, len);
	if (!root) return false;
	const cJSON* ts = cJSON_GetObjectItem(root, "ts");
	const cJSON* temp = cJSON_GetObjectItem(root, "temp");
	const cJSON* code = cJSON_GetObjectItem(root, "code");
	const bool ok = cJSON_IsNumber(ts) && cJSON_IsNumber(temp) && cJSON_IsNumber(code);
	if (ok) {
		out = Data{};
		out.ts = int64_t(ts->valuedouble);
		out.temp = float(temp->valuedouble);
		out.code = uint8_t(code->valueint);
		if (const cJSON* v = cJSON_GetObjectItem(root, "hi"); cJSON_IsNumber(v)) out.hi = float(v->valuedouble);
		if (const cJSON* v = cJSON_GetObjectItem(root, "lo"); cJSON_IsNumber(v)) out.lo = float(v->valuedouble);
		if (const cJSON* v = cJSON_GetObjectItem(root, "place"); cJSON_IsString(v)) {
			strlcpy(out.place, v->valuestring, sizeof(out.place));
		}
		const cJSON* hours = cJSON_GetObjectItem(root, "hours");
		const cJSON* h = nullptr;
		cJSON_ArrayForEach(h, hours) {
			if (out.hourCount == sizeof(out.hours) / sizeof(out.hours[0])) break;
			if (!cJSON_IsArray(h) || cJSON_GetArraySize(h) < 3) continue;
			auto& slot = out.hours[out.hourCount++];
			slot.hour = int8_t(cJSON_GetArrayItem(h, 0)->valueint);
			slot.temp = int8_t(std::lround(cJSON_GetArrayItem(h, 1)->valuedouble));
			slot.code = uint8_t(cJSON_GetArrayItem(h, 2)->valueint);
		}
	}
	cJSON_Delete(root);
	return ok;
}

void load() {
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READONLY, &h) != ESP_OK) return;
	Stored stored{};
	size_t len = sizeof(stored);
	if (nvs_get_blob(h, Key, &stored, &len) == ESP_OK && len == sizeof(stored) && stored.version == Version) {
		latest = stored.data;
		latest.place[sizeof(latest.place) - 1] = 0;
		have = true;
	}
	nvs_close(h);
}

void set(const Data& data) {
	latest = data;
	have = true;
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READWRITE, &h) != ESP_OK) return;
	const Stored stored{ Version, data };
	nvs_set_blob(h, Key, &stored, sizeof(stored));
	nvs_commit(h);
	nvs_close(h);
}

const Data* current() {
	if (!have || time(nullptr) - latest.ts > MaxAgeSeconds) return nullptr;
	return &latest;
}

int64_t lastUpdate() {
	return have ? latest.ts : 0;
}

// WMO weather interpretation codes, as Open-Meteo reports them.
Str condition(uint8_t code) {
	if (code == 0) return Str::WxClear;
	if (code == 1) return Str::WxMainlyClear;
	if (code == 2) return Str::WxPartlyCloudy;
	if (code == 3) return Str::WxOvercast;
	if (code == 45 || code == 48) return Str::WxFog;
	if (code >= 51 && code <= 55) return Str::WxDrizzle;
	if (code == 56 || code == 57 || code == 66 || code == 67) return Str::WxFreezingRain;
	if (code >= 61 && code <= 65) return Str::WxRain;
	if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Str::WxSnow;
	if (code >= 80 && code <= 82) return Str::WxShowers;
	if (code >= 95) return Str::WxThunder;
	return Str::WxOvercast;
}

const lv_image_dsc_t* icon(uint8_t code, bool night) {
	if (code <= 1) return night ? &sprite_wx_moon : &sprite_wx_sun;
	if (code == 2) return night ? &sprite_wx_cloud : &sprite_wx_partly;
	if (code == 3) return &sprite_wx_cloud;
	if (code == 45 || code == 48) return &sprite_wx_fog;
	if ((code >= 71 && code <= 77) || code == 85 || code == 86) return &sprite_wx_snow;
	if (code >= 95) return &sprite_wx_storm;
	if (code >= 51) return &sprite_wx_rain;
	return &sprite_wx_cloud;
}

}  // namespace weather
