#pragma once

#include "ui/dither.h"

#include <ctime>

namespace art {

// Moon age as a fraction of the synodic month: 0 = new, 0.25 = first quarter,
// 0.5 = full, 0.75 = last quarter.
float moonPhase(time_t utc);

// Near side of the Moon as seen from the northern hemisphere (maria in their
// real places), lit for `phase`, dithered through `ramp` onto the canvas.
void drawMoon(dither::Canvas& canvas, int cx, int cy, int radius, float phase, const dither::Ramp& ramp);

}  // namespace art
