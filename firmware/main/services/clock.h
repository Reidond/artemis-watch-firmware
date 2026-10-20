#pragma once

#include <ctime>

class Rtc;

// System time is UTC (seeded from the RTC at boot); local time comes from the
// POSIX TZ string in settings. Every time change is written back to the RTC.
namespace clock_svc {

void init(Rtc& rtc, const char* tz);
void setTimezone(const char* tz);
void setUtc(time_t utc);
bool isValid();  // false until the RTC or the phone gave a plausible time
tm local();
// Seconds since the epoch for broken-down time read as UTC (timegm).
time_t fromUtcFields(const tm& t);
// Offset of local time from UTC at `utc`, in seconds (e.g. 10800 for EEST).
int utcOffset(time_t utc);

}  // namespace clock_svc
