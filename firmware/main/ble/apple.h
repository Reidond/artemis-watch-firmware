#pragma once

#include "gatt.h"

#include <cstdint>

// Clients for the services every iPhone exposes to a bonded accessory. Host
// task only; ble.cpp attaches them after discovery and detaches on disconnect.

// Apple Notification Center Service: notifications, incoming calls, actions.
namespace ble::ancs {
const ble_uuid_t* service();
void attach(const gatt::Service& svc);
void detach();
bool attached();
void perform(uint32_t uid, bool positive);
}  // namespace ble::ancs

// Apple Media Service: now playing, remote control.
namespace ble::ams {
const ble_uuid_t* service();
void attach(const gatt::Service& svc);
void detach();
bool attached();
void command(uint8_t command);
}  // namespace ble::ams

// Current Time Service: wall-clock time and the phone's UTC offset.
namespace ble::cts {
const ble_uuid_t* service();
void attach(const gatt::Service& svc);
void detach();
bool attached();
}  // namespace ble::cts
