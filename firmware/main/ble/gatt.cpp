#include "gatt.h"

#include <deque>
#include <esp_log.h>
#include <map>
#include <memory>

static const char* TAG = "gatt";

namespace ble::gatt {

const Characteristic* Service::find(const ble_uuid_t* id) const {
	for (const auto& c : chars) {
		if (ble_uuid_cmp(&c.uuid.u, id) == 0) return &c;
	}
	return nullptr;
}

namespace {

uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
// Bumped by reset(). Completion callbacks of an older link only free their context.
uint32_t generation = 0;
std::deque<std::function<int()>> queue;  // each entry starts one request, nonzero = nothing in flight
bool busy = false;
std::map<uint16_t, Listener> listeners;

void pump() {
	while (!busy && !queue.empty()) {
		auto start = std::move(queue.front());
		queue.pop_front();
		busy = true;
		if (start() != 0) busy = false;
	}
}

void enqueue(std::function<int()> start) {
	queue.push_back(std::move(start));
	pump();
}

// Every NimBLE completion ends here: hands the result over, then starts the next request.
template <typename F>
void complete(uint32_t gen, F&& deliver) {
	if (gen != generation) return;
	busy = false;
	deliver();
	pump();
}

struct Ctx {
	uint32_t gen;
	Done done;
};

struct ReadCtx {
	uint32_t gen;
	ReadDone done;
};

int onWrite(uint16_t, const ble_gatt_error* error, ble_gatt_attr*, void* arg) {
	std::unique_ptr<Ctx> ctx(static_cast<Ctx*>(arg));
	complete(ctx->gen, [&] {
		if (ctx->done) ctx->done(error->status);
	});
	return 0;
}

int onRead(uint16_t, const ble_gatt_error* error, ble_gatt_attr* attr, void* arg) {
	std::unique_ptr<ReadCtx> ctx(static_cast<ReadCtx*>(arg));
	std::vector<uint8_t> value;
	if (error->status == 0 && attr && attr->om) {
		value.resize(OS_MBUF_PKTLEN(attr->om));
		os_mbuf_copydata(attr->om, 0, value.size(), value.data());
	}
	complete(ctx->gen, [&] { ctx->done(error->status, value); });
	return 0;
}

// Discovery runs as one queue entry that chains NimBLE procedures itself:
// services, then characteristics per service, then descriptors per notifying
// characteristic.
struct Discovery {
	uint32_t gen;
	std::vector<const ble_uuid_t*> wanted;
	std::function<void(int, std::vector<Service>&)> done;
	std::vector<Service> found;
	size_t svc = 0;
	size_t chr = 0;
};

void discoverChars(Discovery* d);
void discoverDescriptors(Discovery* d);

void discoveryDone(Discovery* d, int status) {
	std::unique_ptr<Discovery> own(d);
	complete(d->gen, [&] { d->done(status, d->found); });
}

int onService(uint16_t, const ble_gatt_error* error, const ble_gatt_svc* svc, void* arg) {
	auto* d = static_cast<Discovery*>(arg);
	if (d->gen != generation) {
		if (error->status != 0) delete d;
		return 0;
	}
	if (error->status == 0) {
		for (const ble_uuid_t* w : d->wanted) {
			if (ble_uuid_cmp(&svc->uuid.u, w) == 0) {
				Service s;
				s.uuid = svc->uuid;
				s.start = svc->start_handle;
				s.end = svc->end_handle;
				d->found.push_back(std::move(s));
			}
		}
		return 0;
	}
	if (error->status != BLE_HS_EDONE) {
		discoveryDone(d, error->status);
		return 0;
	}
	d->svc = 0;
	discoverChars(d);
	return 0;
}

int onChar(uint16_t, const ble_gatt_error* error, const ble_gatt_chr* chr, void* arg) {
	auto* d = static_cast<Discovery*>(arg);
	if (d->gen != generation) {
		if (error->status != 0) delete d;
		return 0;
	}
	Service& s = d->found[d->svc];
	if (error->status == 0) {
		Characteristic c;
		c.uuid = chr->uuid;
		c.def = chr->def_handle;
		c.value = chr->val_handle;
		c.props = chr->properties;
		s.chars.push_back(c);
		return 0;
	}
	if (error->status != BLE_HS_EDONE) {
		discoveryDone(d, error->status);
		return 0;
	}
	for (size_t i = 0; i < s.chars.size(); i++) {
		s.chars[i].end = i + 1 < s.chars.size() ? s.chars[i + 1].def - 1 : s.end;
	}
	d->svc++;
	discoverChars(d);
	return 0;
}

int onDescriptor(uint16_t, const ble_gatt_error* error, uint16_t, const ble_gatt_dsc* dsc, void* arg) {
	auto* d = static_cast<Discovery*>(arg);
	if (d->gen != generation) {
		if (error->status != 0) delete d;
		return 0;
	}
	Characteristic& c = d->found[d->svc].chars[d->chr];
	if (error->status == 0) {
		static const ble_uuid16_t cccd = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);
		if (ble_uuid_cmp(&dsc->uuid.u, &cccd.u) == 0) c.cccd = dsc->handle;
		return 0;
	}
	if (error->status != BLE_HS_EDONE) ESP_LOGW(TAG, "descriptors of 0x%04x: %d", c.value, error->status);
	d->chr++;
	discoverDescriptors(d);
	return 0;
}

void discoverChars(Discovery* d) {
	if (d->svc >= d->found.size()) {
		d->svc = 0;
		d->chr = 0;
		discoverDescriptors(d);
		return;
	}
	const Service& s = d->found[d->svc];
	const int rc = ble_gattc_disc_all_chrs(conn, s.start, s.end, onChar, d);
	if (rc != 0) discoveryDone(d, rc);
}

void discoverDescriptors(Discovery* d) {
	for (; d->svc < d->found.size(); d->svc++, d->chr = 0) {
		auto& chars = d->found[d->svc].chars;
		for (; d->chr < chars.size(); d->chr++) {
			const Characteristic& c = chars[d->chr];
			if (!(c.props & (BLE_GATT_CHR_PROP_NOTIFY | BLE_GATT_CHR_PROP_INDICATE)) || c.end <= c.value) continue;
			const int rc = ble_gattc_disc_all_dscs(conn, c.value, c.end, onDescriptor, d);
			if (rc != 0) discoveryDone(d, rc);
			return;
		}
	}
	discoveryDone(d, 0);
}

}  // namespace

void reset(uint16_t c) {
	conn = c;
	generation++;
	queue.clear();
	busy = false;
	listeners.clear();
}

uint16_t connection() {
	return conn;
}

void write(uint16_t handle, std::vector<uint8_t> data, Done done) {
	enqueue([handle, data = std::move(data), done = std::move(done)]() mutable {
		auto* ctx = new Ctx{ generation, std::move(done) };
		const int rc = ble_gattc_write_flat(conn, handle, data.data(), data.size(), onWrite, ctx);
		if (rc != 0) {
			ESP_LOGW(TAG, "write 0x%04x: %d", handle, rc);
			Done d = std::move(ctx->done);
			delete ctx;
			if (d) d(rc);
		}
		return rc;
	});
}

void read(uint16_t handle, ReadDone done) {
	enqueue([handle, done = std::move(done)]() mutable {
		auto* ctx = new ReadCtx{ generation, std::move(done) };
		const int rc = ble_gattc_read(conn, handle, onRead, ctx);
		if (rc != 0) {
			ESP_LOGW(TAG, "read 0x%04x: %d", handle, rc);
			ReadDone d = std::move(ctx->done);
			delete ctx;
			d(rc, {});
		}
		return rc;
	});
}

void subscribe(uint16_t cccd, uint16_t mode, Done done) {
	write(cccd, { uint8_t(mode), uint8_t(mode >> 8) }, std::move(done));
}

void barrier(std::function<void()> fn) {
	enqueue([fn = std::move(fn)] {
		fn();
		return 1;  // nothing in flight, carry on with the queue
	});
}

void discover(std::vector<const ble_uuid_t*> wanted, std::function<void(int, std::vector<Service>&)> done) {
	enqueue([wanted = std::move(wanted), done = std::move(done)]() mutable {
		auto* d = new Discovery{ generation, std::move(wanted), std::move(done) };
		const int rc = ble_gattc_disc_all_svcs(conn, onService, d);
		if (rc != 0) {
			std::unique_ptr<Discovery> own(d);
			d->done(rc, d->found);
		}
		return rc;
	});
}

void listen(uint16_t handle, Listener fn) {
	listeners[handle] = std::move(fn);
}

void dispatch(uint16_t handle, const os_mbuf* om) {
	auto it = listeners.find(handle);
	if (it == listeners.end() || !om) return;
	std::vector<uint8_t> data(OS_MBUF_PKTLEN(om));
	os_mbuf_copydata(om, 0, data.size(), data.data());
	it->second(data.data(), data.size());
}

}  // namespace ble::gatt
