#pragma once
#include <cstdint>

namespace CoreShared {

    inline constexpr uint32_t bgr555_to_argb8888(uint16_t bgr) {
        uint8_t r5 = bgr & 0x1F;
        uint8_t g5 = (bgr >> 5) & 0x1F;
        uint8_t b5 = (bgr >> 10) & 0x1F;

        uint8_t r8 = (r5 << 3) | (r5 >> 2);
        uint8_t g8 = (g5 << 3) | (g5 >> 2);
        uint8_t b8 = (b5 << 3) | (b5 >> 2);

        return 0xFF000000U | (static_cast<uint32_t>(r8) << 16) |
                             (static_cast<uint32_t>(g8) << 8)  |
                              static_cast<uint32_t>(b8);
    }

    inline constexpr uint32_t bgr555_bytes_to_argb8888(uint8_t low, uint8_t high) {
        return bgr555_to_argb8888(static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8));
    }

} // namespace CoreShared