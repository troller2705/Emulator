#pragma once
#include <array>
#include <cstdint>

class GBAMMU;
class GBAAPU;

class GBATimer {
public:
    explicit GBATimer(GBAMMU& mmu);

    void reset();
    void link_apu(GBAAPU* apu) { m_apu = apu; }
    void tick(int cycles);

    uint16_t read_register(uint32_t offset) const;
    void     write_register(uint32_t offset, uint16_t val);

private:
    struct Channel {
        uint16_t counter = 0;
        uint16_t reload  = 0;
        uint16_t control = 0;
        int      sub_cycles = 0;
    };

    std::array<Channel, 4> m_channels{};
    GBAMMU& m_mmu;
    GBAAPU* m_apu = nullptr;

    void overflow_timer(int idx);
};