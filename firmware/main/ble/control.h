#pragma once

#include <cstdint>
#include <host/ble_gap.h>
#include <host/ble_uuid.h>

// "Artemis Control": the GATT service the companion app talks to
// (docs/BLE_PROTOCOL.md). Lives on the NimBLE host task like the rest of ble/.
namespace ble::control {

const ble_uuid128_t* service();

// Before the host syncs (from ble::start).
void registerService();

void onConnect(uint16_t conn);
// First encryption of a link: tells a bonded iPhone to rediscover services
// when this firmware's GATT layout differs from the one it cached.
void onEncrypted();
void onDisconnect();
void onSubscribe(const ble_gap_event& event);

// Safe from any task.
void notifyInfo();
void notifySettings();

}  // namespace ble::control
