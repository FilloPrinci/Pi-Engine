#pragma once

#include <cstdint>

namespace engine::core {

// Drawable surface size in pixels (as opposed to window size in points on HiDPI displays).
struct Extent2D {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

} // namespace engine::core
