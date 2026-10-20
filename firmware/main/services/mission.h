#pragma once

#include <ctime>

// The "T+" day counter on the watch face counts days since this moment.
// Defaults to the Artemis II launch; console `epoch <unix>` changes it.
namespace mission {

constexpr time_t ArtemisIILaunch = 1775082900;  // 2026-04-01 22:35 UTC (18:35 EDT)

time_t epoch();
void setEpoch(time_t utc);
// Whole days since the epoch (T+0 on the epoch's day), negative before it.
int day(time_t utc);

}  // namespace mission
