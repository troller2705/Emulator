#pragma once
#include <cstdint>
#include <vector>

class ICartridge {
public:
    virtual ~ICartridge() = default;

    virtual uint8_t read(uint16_t address) = 0;
    virtual void write(uint16_t address, uint8_t value) = 0;

    virtual uint8_t* get_sram_ptr() = 0;
    virtual size_t get_sram_size() = 0;
};