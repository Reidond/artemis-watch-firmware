#include "settings.h"

#include <esp_log.h>
#include <nvs.h>

static const char* TAG = "settings";

namespace {
constexpr const char* Namespace = "artemis";
constexpr const char* Key = "settings";
constexpr uint32_t Version = 1;

struct Stored {
	uint32_t version;
	SettingsData data;
};
}  // namespace

void Settings::load() {
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READONLY, &h) != ESP_OK) {
		ESP_LOGI(TAG, "no saved settings, using defaults");
		return;
	}
	Stored stored{};
	size_t len = sizeof(stored);
	const esp_err_t err = nvs_get_blob(h, Key, &stored, &len);
	nvs_close(h);
	if (err != ESP_OK || len != sizeof(stored) || stored.version != Version) {
		ESP_LOGW(TAG, "settings missing or from another version, using defaults");
		return;
	}
	data = stored.data;
	data.timezone[sizeof(data.timezone) - 1] = 0;
	if (data.screenTimeoutIdx >= ScreenTimeoutCount) data.screenTimeoutIdx = 2;
	if (data.language != Lang::Uk && data.language != Lang::En) data.language = Lang::Uk;
}

void Settings::save() {
	nvs_handle_t h;
	if (nvs_open(Namespace, NVS_READWRITE, &h) != ESP_OK) return;
	const Stored stored{ Version, data };
	nvs_set_blob(h, Key, &stored, sizeof(stored));
	nvs_commit(h);
	nvs_close(h);
	if (onSave) onSave();
}
