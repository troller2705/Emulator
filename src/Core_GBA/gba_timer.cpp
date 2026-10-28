#include "../../include/Core_GBA/gba_timer.h"
#include "../../include/Core_GBA/gba_mmu.h"
#include "../../include/Core_GBA/gba_apu.h"

GBATimer::GBATimer(GBAMMU& mmu) : m_mmu(mmu) {
    reset();
}

void GBATimer::reset() {
    m_channels = {};
}

uint16_t GBATimer::read_register(uint32_t offset) const {
    int idx = (offset - 0x100) / 4;
    if (idx < 0 || idx > 3) return 0;
    if ((offset & 2) == 0) return m_channels[idx].counter;
    return m_channels[idx].control;
}

void GBATimer::write_register(uint32_t offset, uint16_t val) {
    int idx = (offset - 0x100) / 4;
    if (idx < 0 || idx > 3) return;

    if ((offset & 2) == 0) {
        m_channels[idx].reload = val;
    } else {
        bool was_enabled = (m_channels[idx].control >> 7) & 1;
        bool now_enabled = (val >> 7) & 1;
        m_channels[idx].control = val;
        if (!was_enabled && now_enabled) {
            m_channels[idx].counter    = m_channels[idx].reload;
            m_channels[idx].sub_cycles = 0;
        }
    }
}

void GBATimer::overflow_timer(int idx) {
    m_channels[idx].counter = m_channels[idx].reload;

    if (m_channels[idx].control & (1 << 6)) {
        m_mmu.request_irq(1 << (3 + idx)); // IRQ_TIMER0..3
    }

    if ((idx == 0 || idx == 1) && m_apu) {
        m_apu->on_timer_overflow(idx);
    }

    // Cascade mode: Timer idx+1 increments when Timer idx overflows
    if (idx < 3) {
        auto& next = m_channels[idx + 1];
        if ((next.control & 0x84) == 0x84) { // Enabled + Count-up bit set
            if (next.counter == 0xFFFF) {
                overflow_timer(idx + 1);
            } else {
                next.counter++;
            }
        }
    }
}

void GBATimer::tick(int cycles) {
    static const int PRESCALERS[4] = { 1, 64, 256, 1024 };

    for (int i = 0; i < 4; ++i) {
        auto& ch = m_channels[i];
        if (!(ch.control & 0x80)) continue;     // Disabled
        if (i > 0 && (ch.control & 0x04)) continue; // Cascade timer

        int period = PRESCALERS[ch.control & 3];
        ch.sub_cycles += cycles;

        while (ch.sub_cycles >= period) {
            ch.sub_cycles -= period;
            if (ch.counter == 0xFFFF) {
                overflow_timer(i);
            } else {
                ch.counter++;
            }
        }
    }
}