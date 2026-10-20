#pragma once

#include <string>
#include <string_view>

namespace text {

// Makes UTF-8 from the phone renderable with our fonts: common emoji become
// icons (ui/fonts/icons.h), other emoji and invisible characters are dropped,
// Latin Extended-A letters lose their accents, whitespace is folded.
// singleLine also turns line breaks into spaces. Call on the UI task.
std::string display(std::string_view utf8, bool singleLine = false);

}  // namespace text
