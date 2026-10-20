#include "console.h"

#include "ble/ble.h"
#include "board/board.h"
#include "drivers/battery.h"
#include "drivers/buttons.h"
#include "i18n/i18n.h"
#include "services/clock.h"
#include "services/mission.h"
#include "services/phone.h"
#include "services/settings.h"
#include "ui/screens/screens.h"
#include "ui/ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_console.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <string>

static const char* TAG = "console";

namespace {

int cmdShot(int, char**) {
	if (!ui::lock(2000)) {
		printf("ERR ui busy\n");
		return 1;
	}
	lv_draw_buf_t* snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
	ui::unlock();
	if (!snap) {
		printf("ERR snapshot failed\n");
		return 1;
	}
	const uint32_t w = snap->header.w, h = snap->header.h, stride = snap->header.stride;
	printf("SHOT BEGIN %lu %lu\n", (unsigned long) w, (unsigned long) h);
	// Encode row by row; 57 raw bytes -> 76 base64 chars per line.
	const size_t rowBytes = w * 2;
	auto* packed = static_cast<uint8_t*>(heap_caps_malloc(rowBytes * h, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	for (uint32_t y = 0; y < h; y++) memcpy(packed + y * rowBytes, snap->data + y * stride, rowBytes);
	lv_draw_buf_destroy(snap);
	const size_t total = rowBytes * h;
	unsigned char line[80];
	for (size_t off = 0; off < total; off += 57) {
		size_t olen = 0;
		const size_t n = total - off < 57 ? total - off : 57;
		mbedtls_base64_encode(line, sizeof(line), &olen, packed + off, n);
		fwrite(line, 1, olen, stdout);
		fputc('\n', stdout);
	}
	heap_caps_free(packed);
	printf("SHOT END\n");
	fflush(stdout);
	return 0;
}

int cmdPress(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: press up|down|select|alt [long]\n");
		return 1;
	}
	static const char* names[] = { "up", "down", "select", "alt" };
	for (int i = 0; i < 4; i++) {
		if (strcmp(argv[1], names[i]) == 0) {
			ui::devices().buttons.inject(Button(i), argc > 2 && strcmp(argv[2], "long") == 0);
			return 0;
		}
	}
	printf("unknown button %s\n", argv[1]);
	return 1;
}

int cmdTime(int argc, char** argv) {
	if (argc >= 2) {
		clock_svc::setUtc(time_t(atoll(argv[1])));
		ui::wake();
	}
	const time_t now = time(nullptr);
	char buf[64];
	const tm t = clock_svc::local();
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &t);
	printf("utc=%lld local=%s valid=%d\n", (long long) now, buf, clock_svc::isValid());
	return 0;
}

int cmdTz(int argc, char** argv) {
	auto& s = ui::devices().settings;
	if (argc >= 2) {
		ui::lock(portMAX_DELAY);
		strlcpy(s.get().timezone, argv[1], sizeof(s.get().timezone));
		s.save();
		clock_svc::setTimezone(s.get().timezone);
		ui::unlock();
	}
	printf("tz=%s\n", s.get().timezone);
	return 0;
}

int cmdLang(int argc, char** argv) {
	if (argc < 2 || (strcmp(argv[1], "uk") && strcmp(argv[1], "en"))) {
		printf("usage: lang uk|en\n");
		return 1;
	}
	auto& s = ui::devices().settings;
	ui::lock(portMAX_DELAY);
	s.get().language = strcmp(argv[1], "uk") == 0 ? Lang::Uk : Lang::En;
	i18n::setLanguage(s.get().language);
	s.save();
	ui::rebuildAll();
	ui::unlock();
	ui::wake();
	return 0;
}

int cmdInfo(int, char**) {
	auto& dev = ui::devices();
	const auto id = board::identity();
	printf("hw: pid=0x%04x rev=%u\n", id.pid, id.revision);
	printf("battery: %d mV, %d%%, charge=%d, cal=%d mV\n", dev.battery.millivolts(), dev.battery.percent(),
		int(dev.battery.charge()), dev.battery.calibrationOffsetMv());
	printf("heap: internal %u KB free (min %u), psram %u KB free\n",
		unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
		unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
		unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
	printf("uptime: %lld s\n", esp_timer_get_time() / 1000000);
	printf("lang: %s\n", i18n::language() == Lang::Uk ? "uk" : "en");
	return 0;
}

int cmdWake(int, char**) {
	ui::wake();
	return 0;
}

int cmdEpoch(int argc, char** argv) {
	if (argc >= 2) {
		mission::setEpoch(time_t(atoll(argv[1])));
		ui::post([] { ui::rebuildAll(); });
		ui::wake();
	}
	printf("epoch=%lld day=T+%d\n", (long long) mission::epoch(), mission::day(time(nullptr)));
	return 0;
}

int cmdArt(int, char**) {
	ui::post([] { ui::push(makeArtTest()); });
	ui::wake();
	return 0;
}

int cmdBle(int argc, char** argv) {
	if (argc >= 2 && strcmp(argv[1], "forget") == 0) {
		ble::forgetPhone();
		printf("bonds cleared\n");
		return 0;
	}
	char buf[160];
	ble::describe(buf, sizeof(buf));
	printf("%s\n", buf);
	return 0;
}

// The line editor drops non-ASCII bytes, so watchctl sends \xNN, \uNNNN and
// \UNNNNNNNN escapes (Python's backslashreplace); this turns them back into UTF-8.
std::string unescape(const char* s) {
	std::string out;
	while (*s) {
		const int digits = s[0] != '\\' ? 0 : s[1] == 'x' ? 2 : s[1] == 'u' ? 4 : s[1] == 'U' ? 8 : 0;
		if (!digits || strlen(s) < size_t(2 + digits)) {
			out += *s++;
			continue;
		}
		const std::string hex(s + 2, digits);
		const uint32_t cp = strtoul(hex.c_str(), nullptr, 16);
		s += 2 + digits;
		if (cp < 0x80) {
			out += char(cp);
		} else if (cp < 0x800) {
			out += char(0xC0 | (cp >> 6));
			out += char(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			out += char(0xE0 | (cp >> 12));
			out += char(0x80 | ((cp >> 6) & 0x3F));
			out += char(0x80 | (cp & 0x3F));
		} else {
			out += char(0xF0 | (cp >> 18));
			out += char(0x80 | ((cp >> 12) & 0x3F));
			out += char(0x80 | ((cp >> 6) & 0x3F));
			out += char(0x80 | (cp & 0x3F));
		}
	}
	return out;
}

// Feeds made-up phone data into the UI, to check screens without an iPhone.
int cmdDemo(int argc, char** argv) {
	static uint32_t nextUid = 0xD0000000;
	static uint32_t lastCall = 0;
	const auto arg = [&](int i, const char* fallback) { return unescape(argc > i ? argv[i] : fallback); };
	const std::string what = arg(1, "");
	if (what == "notif" || what == "call") {
		phone::Notification n;
		n.uid = nextUid++;
		const bool call = what == "call";
		n.category = call ? phone::Category::IncomingCall : phone::Category(atoi(arg(4, "4").c_str()));
		n.appId = call ? "com.apple.mobilephone" : "com.apple.MobileSMS";
		n.title = arg(2, "Demo");
		n.message = call ? "" : arg(3, "");
		n.flags = phone::flag::NegativeAction | (call ? phone::flag::PositiveAction : 0);
		const tm t = clock_svc::local();
		n.year = t.tm_year + 1900;
		n.month = t.tm_mon + 1;
		n.day = t.tm_mday;
		n.hour = t.tm_hour;
		n.minute = t.tm_min;
		if (call) lastCall = n.uid;
		ui::post([n, call]() mutable { phone::feed::notification(std::move(n), true, !call); });
	} else if (what == "end") {
		ui::post([] { phone::feed::removed(lastCall); });
	} else if (what == "clear") {
		ui::post([] { phone::feed::clearNotifications(); });
	} else if (what == "music") {
		const std::string title = arg(2, "Demo"), artist = arg(3, "");
		ui::post([title, artist] {
			phone::feed::mediaAvailable(true);
			phone::feed::mediaTrack(phone::feed::TrackField::Title, title);
			phone::feed::mediaTrack(phone::feed::TrackField::Artist, artist);
			phone::feed::mediaDuration(215);
			phone::feed::mediaVolume(0.6f);
			phone::feed::mediaPlayback(phone::MediaState::Playing, 1, 42);
		});
	} else if (what == "pair") {
		const uint32_t code = uint32_t(atol(arg(2, "123456").c_str()));
		ui::post([code] { phone::feed::pairing(code, true); });
	} else if (what == "paired") {
		const std::string r = arg(2, "ok");
		const auto result = r == "ok" ? phone::PairResult::Ok
			: r == "key"             ? phone::PairResult::KeyMissing
									 : phone::PairResult::Failed;
		ui::post([result] { phone::feed::pairingDone(result); });
	} else if (what == "link") {
		const std::string l = arg(2, "ready");
		const auto link = l == "off" ? phone::Link::Off
			: l == "adv"            ? phone::Link::Advertising
			: l == "conn"           ? phone::Link::Connected
									: phone::Link::Ready;
		const bool bonded = arg(3, "bonded") != "unbonded";
		ui::post([link, bonded] { phone::feed::link(link, bonded); });
	} else {
		printf("usage: demo notif <title> <text> [category] | call <name> | end | clear\n"
			   "       demo music <title> <artist> | pair <code> | paired ok|fail|key\n"
			   "       demo link off|adv|conn|ready [unbonded]\n");
		return 1;
	}
	return 0;
}

int cmdReboot(int, char**) {
	esp_restart();
	return 0;
}

void reg(const char* name, const char* help, esp_console_cmd_func_t fn) {
	esp_console_cmd_t cmd = {};
	cmd.command = name;
	cmd.help = help;
	cmd.func = fn;
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

}  // namespace

namespace console {

void start() {
	esp_console_repl_t* repl = nullptr;
	esp_console_repl_config_t replCfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	replCfg.prompt = "watch>";
	replCfg.max_cmdline_length = 2048;  // demo text arrives \u-escaped, 11 bytes per letter
	replCfg.task_stack_size = 6144;
	esp_console_dev_usb_serial_jtag_config_t hwCfg = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hwCfg, &replCfg, &repl));

	esp_console_register_help_command();
	reg("shot", "Dump the screen as base64 RGB565", cmdShot);
	reg("press", "Inject a button: press up|down|select|alt [long]", cmdPress);
	reg("time", "Show time, or set UTC: time <unix seconds>", cmdTime);
	reg("tz", "Show or set POSIX time zone: tz EET-2EEST,M3.5.0/3,M10.5.0/4", cmdTz);
	reg("lang", "Switch UI language: lang uk|en", cmdLang);
	reg("info", "Hardware, battery, memory", cmdInfo);
	reg("wake", "Turn the screen on", cmdWake);
	reg("art", "Open the dither engine test screen", cmdArt);
	reg("epoch", "Show or set the T+ mission epoch: epoch <unix seconds>", cmdEpoch);
	reg("ble", "Bluetooth state, or ble forget: drop all bonds", cmdBle);
	reg("demo", "Fake phone data for UI checks (demo help)", cmdDemo);
	reg("reboot", "Restart the watch", cmdReboot);

	ESP_ERROR_CHECK(esp_console_start_repl(repl));
	ESP_LOGI(TAG, "console ready");
}

}  // namespace console
