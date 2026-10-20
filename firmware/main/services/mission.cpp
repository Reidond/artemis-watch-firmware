#include "mission.h"

#include <cmath>
#include <nvs.h>

namespace {
constexpr const char* Namespace = "artemis";
constexpr const char* Key = "epoch";
time_t cached = 0;
}  // namespace

namespace mission {

time_t epoch() {
	if (cached) return cached;
	cached = ArtemisIILaunch;
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READONLY, &h) == ESP_OK) {
		int64_t v = 0;
		if (nvs_get_i64(h, Key, &v) == ESP_OK && v > 0) cached = time_t(v);
		nvs_close(h);
	}
	return cached;
}

void setEpoch(time_t utc) {
	cached = utc;
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READWRITE, &h) != ESP_OK) return;
	nvs_set_i64(h, Key, int64_t(utc));
	nvs_commit(h);
	nvs_close(h);
}

int day(time_t utc) {
	return int(std::floor(double(utc - epoch()) / 86400.0));
}

}  // namespace mission
