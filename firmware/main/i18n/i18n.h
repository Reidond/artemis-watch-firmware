#pragma once

#include "services/settings.h"

// UI strings. Add the enum value here and both translations in i18n.cpp.
enum class Str : uint16_t {
	AppName,
	// Menu
	MenuNotifications,
	MenuStopwatch,
	MenuMusic,
	MenuSettings,
	MenuAbout,
	// Settings
	SetLanguage,
	SetBrightness,
	SetScreenTimeout,
	SetTimeFormat,
	SetFlipScreen,
	SetSounds,
	SetPhone,
	SetPowerOff,
	LanguageName,  // name of the current language, in that language
	On,
	Off,
	Hours24,
	Hours12,
	Seconds,
	Minutes,
	// Power off confirmation
	PowerOffConfirm,
	// Phone link
	LinkOff,
	LinkSearching,
	LinkConnecting,
	LinkConnected,
	NotPaired,
	PairHint,
	ForgetPhoneConfirm,
	// Notifications
	NoNotifications,
	PhoneNotConnected,
	Actions,
	ActionPositive,
	ActionNegative,
	// Calls
	IncomingCall,
	UnknownCaller,
	// Music
	NothingPlaying,
	PlayOnPhone,
	HoldForVolume,
	// Pairing
	Pairing,
	PairConfirm,
	PairEnter,
	PairWaiting,
	Paired,
	PairFailed,
	PairForgetOnPhone,
	// About
	AboutFirmware,
	AboutHardware,
	AboutBattery,
	AboutCharging,
	AboutCharged,
	AboutUptime,
	AboutMemory,
	// Clock
	TimeNotSet,
	Am,
	Pm,
	// Weather, firmware update, find watch (companion app)
	MenuWeather,
	WeatherNone,
	WeatherHint,
	WeatherUpdated,
	WxClear,
	WxMainlyClear,
	WxPartlyCloudy,
	WxOvercast,
	WxFog,
	WxDrizzle,
	WxRain,
	WxFreezingRain,
	WxSnow,
	WxShowers,
	WxThunder,
	OtaTitle,
	OtaReceiving,
	OtaVerifying,
	OtaRestarting,
	OtaFailed,
	FindHere,
	Count
};

namespace i18n {

void setLanguage(Lang lang);
Lang language();
const char* tr(Str s);

const char* weekdayShort(int wday);  // 0 = Sunday
const char* weekdayLong(int wday);
const char* monthGenitive(int mon);  // 0 = January; "19 жовтня" / "19 October"

}  // namespace i18n

using i18n::tr;
