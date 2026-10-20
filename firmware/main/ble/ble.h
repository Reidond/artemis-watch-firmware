#pragma once

#include <cstdint>
#include <functional>

// Bluetooth LE link to the iPhone (NimBLE). The watch advertises as "Artemis",
// soliciting ANCS so iOS offers it in Settings > Bluetooth, bonds with LE
// Secure Connections (numeric comparison on the watch) and then acts as a GATT
// client of Apple's ANCS (notifications), AMS (media) and CTS (time).
//
// All BLE state lives on the NimBLE host task. Results reach the UI through
// ui::post into services/phone; the functions below are safe from any task.
namespace ble {

void start();  // once, after nvs_flash_init()

// Runs fn on the NimBLE host task.
void post(std::function<void()> fn);

void notificationAction(uint32_t uid, bool positive);
void mediaCommand(uint8_t command);  // AMS RemoteCommandID
void answerPairing(bool accept);     // numeric comparison: the codes match
void forgetPhone();                  // drop all bonds and the current link

// One-line state summary for the debug console.
void describe(char* buf, size_t len);

}  // namespace ble
