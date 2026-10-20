#include "ble.h"
#include "apple.h"
#include "control.h"
#include "gatt.h"

#include "services/phone.h"
#include "ui/ui.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <host/ble_hs.h>
#include <mutex>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

extern "C" void ble_store_config_init(void);

static const char* TAG = "ble";

namespace ble {
namespace {

constexpr uint16_t AppearanceWatch = 0x00C0;
const ble_uuid16_t GattService = BLE_UUID16_INIT(0x1801);
const ble_uuid16_t ServiceChanged = BLE_UUID16_INIT(0x2A05);
// Apple's recommended advertising intervals: 20 ms for the first 30 s, then a slower step.
constexpr uint16_t AdvFast = 32;   // 20 ms in 0.625 ms units
constexpr uint16_t AdvSlow = 668;  // 417.5 ms
constexpr int32_t AdvFastMs = 30000;

bool started = false;
uint8_t ownAddrType = BLE_OWN_ADDR_RANDOM;
uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
uint16_t mtu = 23;
bool encrypted = false;
bool pairing = false;  // a passkey was shown for the current link
phone::Link linkState = phone::Link::Off;

// A bonded iPhone encrypts on its own right after connecting; a Security
// Request on top of that never completes and times out after 30 s. So the
// watch only asks when nothing happened for a moment (first pairing).
constexpr uint32_t SecurityDelayMs = 1500;
ble_npl_callout securityTimer;

std::mutex postedMutex;
std::deque<std::function<void()>> posted;
ble_npl_event postEvent;

int gapEvent(ble_gap_event* event, void* arg);

int bondCount() {
	int n = 0;
	ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &n);
	return n;
}

void report(phone::Link link) {
	linkState = link;
	ui::post([link, bonded = bondCount() > 0] { phone::feed::link(link, bonded); });
}

void reportPairing(phone::PairResult result) {
	ui::post([result] { phone::feed::pairingDone(result); });
}

// A static random address derived from the Bluetooth MAC. Stable across
// reboots, but not the stock firmware's public address: an iPhone that bonded
// with the stock firmware sees a new accessory instead of failing on old keys.
void setAddress() {
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_BT);
	uint8_t addr[6];
	for (int i = 0; i < 6; i++) addr[i] = mac[5 - i] ^ 0x5A;
	addr[5] |= 0xC0;
	const int rc = ble_hs_id_set_rnd(addr);
	if (rc != 0) {
		ESP_LOGW(TAG, "random address: %d, using public", rc);
		ownAddrType = BLE_OWN_ADDR_PUBLIC;
		return;
	}
	ESP_LOGI(TAG, "address %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

void advertise(bool fast) {
	if (conn != BLE_HS_CONN_HANDLE_NONE) return;
	if (ble_gap_adv_active()) ble_gap_adv_stop();

	// Advertising data: flags, appearance and the ANCS solicitation, which
	// makes iOS list the watch in Settings > Bluetooth. The name goes in the
	// scan response for lack of room.
	ble_hs_adv_fields fields = {};
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
	fields.appearance = AppearanceWatch;
	fields.appearance_is_present = 1;
	fields.sol_uuids128 = reinterpret_cast<const ble_uuid128_t*>(ancs::service());
	fields.sol_num_uuids128 = 1;
	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) ESP_LOGE(TAG, "advertising data: %d", rc);

	ble_hs_adv_fields rsp = {};
	const char* name = ble_svc_gap_device_name();
	rsp.name = reinterpret_cast<const uint8_t*>(name);
	rsp.name_len = strlen(name);
	rsp.name_is_complete = 1;
	rsp.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
	rsp.tx_pwr_lvl_is_present = 1;
	// The companion app's service, so it can find the watch by scanning.
	rsp.uuids128 = control::service();
	rsp.num_uuids128 = 1;
	rsp.uuids128_is_complete = 0;
	rc = ble_gap_adv_rsp_set_fields(&rsp);
	if (rc != 0) ESP_LOGE(TAG, "scan response: %d", rc);

	ble_gap_adv_params params = {};
	params.conn_mode = BLE_GAP_CONN_MODE_UND;
	params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	params.itvl_min = params.itvl_max = fast ? AdvFast : AdvSlow;
	rc = ble_gap_adv_start(ownAddrType, nullptr, fast ? AdvFastMs : BLE_HS_FOREVER, &params, gapEvent, nullptr);
	if (rc != 0) {
		ESP_LOGE(TAG, "advertising: %d", rc);
		return;
	}
	if (linkState != phone::Link::Advertising) report(phone::Link::Advertising);
}

// After setup, a calmer connection interval within Apple's rules (min a
// multiple of 15 ms, max >= min + 15 ms, timeout 2..6 s). Battery tuning
// proper is milestone 7.
void relaxConnection() {
	if (conn == BLE_HS_CONN_HANDLE_NONE) return;
	ble_gap_upd_params params = {};
	params.itvl_min = 48;  // 60 ms in 1.25 ms units
	params.itvl_max = 60;  // 75 ms
	params.latency = 0;
	params.supervision_timeout = 400;  // 4 s in 10 ms units
	const int rc = ble_gap_update_params(conn, &params);
	if (rc != 0) ESP_LOGW(TAG, "connection update: %d", rc);
}

void detachAll() {
	ancs::detach();
	ams::detach();
	cts::detach();
}

void startServices();

void onServiceChanged(const uint8_t*, size_t) {
	ESP_LOGI(TAG, "phone services changed, rediscovering");
	gatt::barrier([] {
		detachAll();
		gatt::reset(conn);
		startServices();
	});
}

void startServices() {
	gatt::discover({ ancs::service(), ams::service(), cts::service(), &GattService.u },
		[](int status, std::vector<gatt::Service>& found) {
			if (status != 0) {
				ESP_LOGW(TAG, "discovery failed: 0x%x", status);
				return;
			}
			const auto byUuid = [&](const ble_uuid_t* id) -> const gatt::Service* {
				for (const auto& s : found) {
					if (ble_uuid_cmp(&s.uuid.u, id) == 0) return &s;
				}
				return nullptr;
			};
			if (const auto* s = byUuid(&GattService.u)) {
				const auto* sc = s->find(&ServiceChanged.u);
				if (sc && sc->cccd) {
					gatt::listen(sc->value, onServiceChanged);
					gatt::subscribe(sc->cccd, 2);
				}
			}
			// Time first (one quick read), then notifications, then media.
			const auto* cts = byUuid(cts::service());
			const auto* ancs = byUuid(ancs::service());
			const auto* ams = byUuid(ams::service());
			ESP_LOGI(TAG, "services: ancs=%d ams=%d cts=%d", ancs != nullptr, ams != nullptr, cts != nullptr);
			if (cts) cts::attach(*cts);
			if (ancs) ancs::attach(*ancs);
			if (ams) ams::attach(*ams);
			gatt::barrier([] {
				report(phone::Link::Ready);
				relaxConnection();
			});
		});
}

int gapEvent(ble_gap_event* event, void*) {
	switch (event->type) {
		case BLE_GAP_EVENT_CONNECT:
			if (event->connect.status != 0) {
				ESP_LOGW(TAG, "connect failed: %d", event->connect.status);
				advertise(true);
				return 0;
			}
			conn = event->connect.conn_handle;
			encrypted = false;
			pairing = false;
			gatt::reset(conn);
			control::onConnect(conn);
			ESP_LOGI(TAG, "connected");
			report(phone::Link::Connected);
			ble_npl_callout_reset(&securityTimer, ble_npl_time_ms_to_ticks32(SecurityDelayMs));
			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			ESP_LOGI(TAG, "disconnected: 0x%x", event->disconnect.reason);
			if (pairing && !encrypted) reportPairing(phone::PairResult::Failed);
			conn = BLE_HS_CONN_HANDLE_NONE;
			encrypted = false;
			pairing = false;
			mtu = 23;
			control::onDisconnect();
			ble_npl_callout_stop(&securityTimer);
			gatt::reset(BLE_HS_CONN_HANDLE_NONE);
			detachAll();
			advertise(true);
			return 0;

		case BLE_GAP_EVENT_ENC_CHANGE: {
			const int status = event->enc_change.status;
			if (status != 0) {
				ESP_LOGW(TAG, "encryption failed: 0x%x", status);
				// The phone has keys we lost (watch erased or re-flashed): it
				// has to forget the watch before pairing again.
				if (status == BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING) {
					reportPairing(phone::PairResult::KeyMissing);
				} else if (pairing) {
					reportPairing(phone::PairResult::Failed);
				}
				pairing = false;
				return 0;
			}
			ble_gap_conn_desc desc;
			if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
				ESP_LOGI(TAG, "encrypted, bonded=%d authenticated=%d key=%u", desc.sec_state.bonded,
					desc.sec_state.authenticated, desc.sec_state.key_size);
			}
			if (encrypted) return 0;  // key refresh
			encrypted = true;
			control::onEncrypted();
			if (pairing) reportPairing(phone::PairResult::Ok);
			pairing = false;
			startServices();
			return 0;
		}

		case BLE_GAP_EVENT_PASSKEY_ACTION: {
			const auto& p = event->passkey.params;
			if (p.action == BLE_SM_IOACT_NUMCMP) {
				pairing = true;
				const uint32_t code = p.numcmp;
				ESP_LOGI(TAG, "pairing, code %06lu", (unsigned long) code);
				ui::post([code] { phone::feed::pairing(code, true); });
			} else if (p.action == BLE_SM_IOACT_DISP) {
				ble_sm_io io = {};
				io.action = BLE_SM_IOACT_DISP;
				io.passkey = esp_random() % 1000000;
				ble_sm_inject_io(event->passkey.conn_handle, &io);
				pairing = true;
				const uint32_t code = io.passkey;
				ESP_LOGI(TAG, "pairing, passkey %06lu", (unsigned long) code);
				ui::post([code] { phone::feed::pairing(code, false); });
			} else {
				ESP_LOGW(TAG, "unsupported pairing action %u", p.action);
			}
			return 0;
		}

		case BLE_GAP_EVENT_REPEAT_PAIRING: {
			// The phone forgot the watch and pairs again: drop the old bond.
			ble_gap_conn_desc desc;
			if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
				ble_store_util_delete_peer(&desc.peer_id_addr);
			}
			ESP_LOGI(TAG, "repeat pairing, old bond removed");
			return BLE_GAP_REPEAT_PAIRING_RETRY;
		}

		case BLE_GAP_EVENT_SUBSCRIBE:
			control::onSubscribe(*event);
			return 0;

		case BLE_GAP_EVENT_NOTIFY_RX:
			gatt::dispatch(event->notify_rx.attr_handle, event->notify_rx.om);
			return 0;

		case BLE_GAP_EVENT_MTU:
			mtu = event->mtu.value;
			ESP_LOGI(TAG, "mtu %u", mtu);
			return 0;

		case BLE_GAP_EVENT_CONN_UPDATE: {
			ble_gap_conn_desc desc;
			if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
				ESP_LOGI(TAG, "interval %u.%02u ms, latency %u, timeout %u ms", desc.conn_itvl * 5 / 4,
					desc.conn_itvl * 125 % 100, desc.conn_latency, desc.supervision_timeout * 10);
			}
			return 0;
		}

		case BLE_GAP_EVENT_ADV_COMPLETE:
			if (conn == BLE_HS_CONN_HANDLE_NONE) advertise(false);
			return 0;

		default:
			return 0;
	}
}

// Apple's services refuse unencrypted links, so ask for encryption (pairing
// the first time) if the phone did not start it.
void onSecurityTimer(ble_npl_event*) {
	if (conn == BLE_HS_CONN_HANDLE_NONE || encrypted) return;
	const int rc = ble_gap_security_initiate(conn);
	if (rc != 0) ESP_LOGW(TAG, "security request: %d", rc);
}

void onSync() {
	setAddress();
	ESP_LOGI(TAG, "ready, %d bond(s)", bondCount());
	advertise(true);
}

void onReset(int reason) {
	ESP_LOGE(TAG, "host reset: %d", reason);
}

void hostTask(void*) {
	nimble_port_run();
	nimble_port_freertos_deinit();
}

void runPosted(ble_npl_event*) {
	std::deque<std::function<void()>> work;
	{
		std::lock_guard<std::mutex> guard(postedMutex);
		work.swap(posted);
	}
	for (auto& fn : work) fn();
}

}  // namespace

void start() {
	const esp_err_t err = nimble_port_init();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nimble init: %s", esp_err_to_name(err));
		return;
	}
	ble_hs_cfg.reset_cb = onReset;
	ble_hs_cfg.sync_cb = onSync;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
	// Numeric comparison: the watch shows the code iOS shows, Select confirms.
	ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_mitm = 1;
	ble_hs_cfg.sm_sc = 1;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

	ble_svc_gap_init();
	ble_svc_gatt_init();
	control::registerService();
	ble_svc_gap_device_name_set("Artemis");
	ble_store_config_init();

	ble_npl_event_init(&postEvent, runPosted, nullptr);
	ble_npl_callout_init(&securityTimer, nimble_port_get_dflt_eventq(), onSecurityTimer, nullptr);
	started = true;
	nimble_port_freertos_init(hostTask);
}

void post(std::function<void()> fn) {
	if (!started) return;
	{
		std::lock_guard<std::mutex> guard(postedMutex);
		posted.push_back(std::move(fn));
	}
	ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &postEvent);
}

void notificationAction(uint32_t uid, bool positive) {
	post([uid, positive] { ancs::perform(uid, positive); });
}

void mediaCommand(uint8_t command) {
	post([command] { ams::command(command); });
}

void answerPairing(bool accept) {
	post([accept] {
		if (conn == BLE_HS_CONN_HANDLE_NONE) return;
		ble_sm_io io = {};
		io.action = BLE_SM_IOACT_NUMCMP;
		io.numcmp_accept = accept;
		const int rc = ble_sm_inject_io(conn, &io);
		if (rc != 0) ESP_LOGW(TAG, "pairing answer: %d", rc);
	});
}

void forgetPhone() {
	post([] {
		ESP_LOGI(TAG, "forgetting all bonds");
		if (conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
		ble_store_clear();
		report(linkState);
	});
}

void describe(char* buf, size_t len) {
	static const char* const names[] = { "off", "advertising", "connected", "ready" };
	snprintf(buf, len, "link=%s bonds=%d mtu=%u encrypted=%d ancs=%d ams=%d cts=%d", names[int(linkState)],
		started ? bondCount() : 0, mtu, encrypted, ancs::attached(), ams::attached(), cts::attached());
}

}  // namespace ble
