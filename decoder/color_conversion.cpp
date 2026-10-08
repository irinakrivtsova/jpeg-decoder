#include "color_conversion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace color_conversion {

RGB YCbCrToRGB(double y, double cb, double cr) {
    const int r = std::clamp(static_cast<int>(std::lround(y + 1.402 * (cr - 128.0))), 0, 255);

    const int g = std::clamp(
        static_cast<int>(std::lround(y - 0.344136 * (cb - 128.0) - 0.714136 * (cr - 128.0))), 0,
        255);

    const int b = std::clamp(static_cast<int>(std::lround(y + 1.772 * (cb - 128.0))), 0, 255);

    return RGB{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
}

}  // namespace color_conversion
