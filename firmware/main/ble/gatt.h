#pragma once

#include <host/ble_hs.h>

#include <cstdint>
#include <functional>
#include <vector>

// GATT client helpers for the one connection, host task only. ATT allows a
// single outstanding request per link and NimBLE does not queue them, so
// every read, write and discovery step goes through one FIFO here.
namespace ble::gatt {

struct Characteristic {
	ble_uuid_any_t uuid;
	uint16_t def = 0;
	uint16_t value = 0;
	uint16_t end = 0;   // last handle that belongs to this characteristic
	uint16_t cccd = 0;  // 0 if none
	uint8_t props = 0;
};

struct Service {
	ble_uuid_any_t uuid;
	uint16_t start = 0;
	uint16_t end = 0;
	std::vector<Characteristic> chars;

	const Characteristic* find(const ble_uuid_t* uuid) const;
};

using Done = std::function<void(int status)>;
using ReadDone = std::function<void(int status, const std::vector<uint8_t>& value)>;
using Listener = std::function<void(const uint8_t* data, size_t len)>;

// A new link (or BLE_HS_CONN_HANDLE_NONE after a disconnect). Drops queued
// requests and listeners from the previous one.
void reset(uint16_t conn);
uint16_t connection();

void write(uint16_t handle, std::vector<uint8_t> data, Done done = nullptr);
void read(uint16_t handle, ReadDone done);
// Writes the CCCD: 1 = notifications, 2 = indications.
void subscribe(uint16_t cccd, uint16_t mode, Done done = nullptr);
// Runs fn once every request queued before it has finished.
void barrier(std::function<void()> fn);

// Finds the given primary services with their characteristics and CCCDs.
void discover(std::vector<const ble_uuid_t*> wanted, std::function<void(int status, std::vector<Service>& found)> done);

// Notifications and indications from the phone, by value handle.
void listen(uint16_t handle, Listener fn);
void dispatch(uint16_t handle, const os_mbuf* om);

}  // namespace ble::gatt
