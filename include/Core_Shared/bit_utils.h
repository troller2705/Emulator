#pragma once
#include <cstdint>

namespace CoreShared {

    inline uint16_t read_le16(const uint8_t* buf, uint32_t offset = 0) {
        return static_cast<uint16_t>(buf[offset]) |
               (static_cast<uint16_t>(buf[offset + 1]) << 8);
    }

    inline uint32_t read_le32(const uint8_t* buf, uint32_t offset = 0) {
        return static_cast<uint32_t>(buf[offset]) |
               (static_cast<uint32_t>(buf[offset + 1]) << 8) |
               (static_cast<uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<uint32_t>(buf[offset + 3]) << 24);
    }

    inline void write_le16(uint8_t* buf, uint32_t offset, uint16_t val) {
        buf[offset]     = static_cast<uint8_t>(val & 0xFF);
        buf[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    }

    inline void write_le32(uint8_t* buf, uint32_t offset, uint32_t val) {
        buf[offset]     = static_cast<uint8_t>(val & 0xFF);
        buf[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        buf[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        buf[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    }

    inline constexpr uint32_t rotr32(uint32_t val, uint32_t shift) {
        shift &= 31U;
        return (shift == 0) ? val : ((val >> shift) | (val << (32U - shift)));
    }

} // namespace CoreShared