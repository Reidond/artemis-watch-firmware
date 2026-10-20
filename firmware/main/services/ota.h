#pragma once

#include <cstddef>
#include <cstdint>

// Firmware update over BLE into the spare OTA slot (docs/BLE_PROTOCOL.md).
// A task of its own writes flash, so erases never stall the BLE host. All
// functions are safe from any task; results arrive through the report callback
// as the JSON the protocol notifies on OTA control.
namespace ota {

using Report = void (*)(const char* json);

void init(Report report);

void begin(size_t size, const uint8_t sha256[32]);
// Bytes of the image, in order. Blocks briefly when the writer is behind,
// which is the BLE flow control.
void data(const uint8_t* bytes, size_t len);
void end();
void abort();

bool active();
bool failed();  // the last update stopped with an error
// Progress 0..1 while active.
float progress();

}  // namespace ota
