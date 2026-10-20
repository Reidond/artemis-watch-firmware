#include "i18n.h"

#include <cstddef>

namespace {

Lang current = Lang::Uk;

struct Entry {
	const char* uk;
	const char* en;
};

// Order must match enum Str.
constexpr Entry Strings[] = {
	{ "Артеміда", "Artemis" },
	// Menu
	{ "Сповіщення", "Notifications" },
	{ "Секундомір", "Stopwatch" },
	{ "Музика", "Music" },
	{ "Налаштування", "Settings" },
	{ "Про годинник", "About" },
	// Settings
	{ "Мова", "Language" },
	{ "Яскравість", "Brightness" },
	{ "Згасання екрана", "Screen timeout" },
	{ "Формат часу", "Time format" },
	{ "Перевернути екран", "Flip screen" },
	{ "Звуки", "Sounds" },
	{ "Телефон", "Phone" },
	{ "Вимкнути годинник", "Power off" },
	{ "Українська", "English" },
	{ "Увімк.", "On" },
	{ "Вимк.", "Off" },
	{ "24 год", "24 h" },
	{ "12 год", "12 h" },
	{ "с", "s" },
	{ "хв", "min" },
	// Power off confirmation
	{ "Вимкнути?", "Power off?" },
	// Phone link
	{ "Bluetooth вимкнено", "Bluetooth off" },
	{ "Не підʼєднано", "Not connected" },
	{ "Підʼєднується…", "Connecting…" },
	{ "Підʼєднано", "Connected" },
	{ "Не сполучено", "Not paired" },
	{ "iPhone: Параметри > Bluetooth > Artemis", "iPhone: Settings > Bluetooth > Artemis" },
	{ "Забути iPhone?", "Forget iPhone?" },
	// Notifications
	{ "Немає сповіщень", "No notifications" },
	{ "Телефон не підʼєднано", "Phone not connected" },
	{ "Дії", "Actions" },
	{ "Так", "OK" },
	{ "Очистити", "Clear" },
	// Calls
	{ "Вхідний дзвінок", "Incoming call" },
	{ "Невідомий", "Unknown" },
	// Music
	{ "Нічого не грає", "Nothing playing" },
	{ "Увімкніть музику на iPhone", "Start music on the iPhone" },
	{ "утримати — гучність", "hold: volume" },
	// Pairing
	{ "Сполучення", "Pairing" },
	{ "Код такий самий, як на iPhone?", "Same code as on the iPhone?" },
	{ "Введіть цей код на iPhone", "Enter this code on the iPhone" },
	{ "Зачекайте…", "Please wait…" },
	{ "Сполучено", "Paired" },
	{ "Не вдалося", "Failed" },
	{ "iPhone: Bluetooth > Artemis > Забути цей пристрій", "iPhone: Bluetooth > Artemis > Forget This Device" },
	// About
	{ "Прошивка", "Firmware" },
	{ "Плата", "Board" },
	{ "Батарея", "Battery" },
	{ "заряджається", "charging" },
	{ "заряджено", "charged" },
	{ "Час роботи", "Uptime" },
	{ "Памʼять", "Memory" },
	// Clock
	{ "Час не встановлено", "Time not set" },
	{ "дп", "AM" },
	{ "пп", "PM" },
	// Weather, firmware update, find watch (companion app)
	{ "Погода", "Weather" },
	{ "Ще немає погоди", "No weather yet" },
	{ "Відкрийте застосунок Artemis на iPhone", "Open the Artemis app on your iPhone" },
	{ "Оновлено", "Updated" },
	{ "Ясно", "Clear" },
	{ "Переважно ясно", "Mainly clear" },
	{ "Мінлива хмарність", "Partly cloudy" },
	{ "Хмарно", "Overcast" },
	{ "Туман", "Fog" },
	{ "Мряка", "Drizzle" },
	{ "Дощ", "Rain" },
	{ "Крижаний дощ", "Freezing rain" },
	{ "Сніг", "Snow" },
	{ "Злива", "Showers" },
	{ "Гроза", "Thunderstorm" },
	{ "Оновлення", "Update" },
	{ "Отримую прошивку", "Receiving firmware" },
	{ "Перевіряю", "Verifying" },
	{ "Перезапуск…", "Restarting…" },
	{ "Оновлення не вдалося", "Update failed" },
	{ "Я тут!", "Here I am!" },
};
static_assert(sizeof(Strings) / sizeof(Strings[0]) == size_t(Str::Count), "translation table out of sync with Str");

constexpr Entry WeekdayShort[] = {
	{ "Нд", "Sun" }, { "Пн", "Mon" }, { "Вт", "Tue" }, { "Ср", "Wed" },
	{ "Чт", "Thu" }, { "Пт", "Fri" }, { "Сб", "Sat" },
};

constexpr Entry WeekdayLong[] = {
	{ "Неділя", "Sunday" }, { "Понеділок", "Monday" }, { "Вівторок", "Tuesday" },
	{ "Середа", "Wednesday" }, { "Четвер", "Thursday" }, { "Пʼятниця", "Friday" },
	{ "Субота", "Saturday" },
};

constexpr Entry MonthGenitive[] = {
	{ "січня", "January" }, { "лютого", "February" }, { "березня", "March" },
	{ "квітня", "April" }, { "травня", "May" }, { "червня", "June" },
	{ "липня", "July" }, { "серпня", "August" }, { "вересня", "September" },
	{ "жовтня", "October" }, { "листопада", "November" }, { "грудня", "December" },
};

const char* pick(const Entry& e) {
	return current == Lang::Uk ? e.uk : e.en;
}

}  // namespace

namespace i18n {

void setLanguage(Lang lang) { current = lang; }
Lang language() { return current; }

const char* tr(Str s) {
	return pick(Strings[size_t(s)]);
}

const char* weekdayShort(int wday) { return pick(WeekdayShort[wday % 7]); }
const char* weekdayLong(int wday) { return pick(WeekdayLong[wday % 7]); }
const char* monthGenitive(int mon) { return pick(MonthGenitive[mon % 12]); }

}  // namespace i18n
