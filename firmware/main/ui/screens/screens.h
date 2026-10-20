#pragma once

#include "services/phone.h"
#include "ui/ui.h"

#include <cstdint>
#include <memory>

std::unique_ptr<Screen> makeWatchFace();
std::unique_ptr<Screen> makeMenu();
std::unique_ptr<Screen> makeSettings();
std::unique_ptr<Screen> makeNotifications();
std::unique_ptr<Screen> makeStopwatch();
std::unique_ptr<Screen> makeAbout();
std::unique_ptr<Screen> makePowerOff();
std::unique_ptr<Screen> makeMusic();
std::unique_ptr<Screen> makeForgetPhone();
std::unique_ptr<Screen> makeArtTest();
std::unique_ptr<Screen> makeWeather();
std::unique_ptr<Screen> makeUpdate();  // firmware update progress (OTA from the app)
std::unique_ptr<Screen> makeFind();    // "find my watch": flash and ring  // dither engine check, console `art`

// Phone-driven screens. installPhoneEvents() makes services/phone open them.
void installPhoneEvents();
std::unique_ptr<Screen> makeNotificationDetail(uint32_t uid, bool popup);
bool showInPopup(uint32_t uid);  // false if no popup is open
std::unique_ptr<Screen> makeCall(uint32_t uid);
bool callScreenOpen();
std::unique_ptr<Screen> makePairing(uint32_t code, bool confirm);
bool pairingFinished(phone::PairResult result);  // false if no pairing screen is open
bool pairingScreenOpen();
std::unique_ptr<Screen> makePairingResult(phone::PairResult result);
const char* categoryIcon(phone::Category c);
