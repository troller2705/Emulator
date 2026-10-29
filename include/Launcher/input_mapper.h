#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

// Unified 32-bit Input Bitmask across all Emulator Cores
namespace EmulatorInput {
    // Standard 8-bit GB / GBC / GBA Base Buttons (Bits 0-7)
    constexpr uint32_t BTN_A        = 1U << 0;
    constexpr uint32_t BTN_B        = 1U << 1;
    constexpr uint32_t BTN_SELECT   = 1U << 2;
    constexpr uint32_t BTN_START    = 1U << 3;
    constexpr uint32_t BTN_RIGHT    = 1U << 4;
    constexpr uint32_t BTN_LEFT     = 1U << 5;
    constexpr uint32_t BTN_UP       = 1U << 6;
    constexpr uint32_t BTN_DOWN     = 1U << 7;

    // Extended Shoulder, Trigger & Face Buttons (Bits 8-15)
    constexpr uint32_t BTN_R        = 1U << 8;  // GBA R / Controller R1
    constexpr uint32_t BTN_L        = 1U << 9;  // GBA L / Controller L1
    constexpr uint32_t BTN_X        = 1U << 10; // Controller X
    constexpr uint32_t BTN_Y        = 1U << 11; // Controller Y
    constexpr uint32_t BTN_L2       = 1U << 12; // Left Trigger (L2)
    constexpr uint32_t BTN_R2       = 1U << 13; // Right Trigger (R2)
    constexpr uint32_t BTN_L3       = 1U << 14; // Left Stick Click (L3)
    constexpr uint32_t BTN_R3       = 1U << 15; // Right Stick Click (R3)

    // Left Analog Stick Digital Directions (Bits 16-19)
    constexpr uint32_t BTN_LS_UP    = 1U << 16;
    constexpr uint32_t BTN_LS_DOWN  = 1U << 17;
    constexpr uint32_t BTN_LS_LEFT  = 1U << 18;
    constexpr uint32_t BTN_LS_RIGHT = 1U << 19;

    // Right Analog Stick Digital Directions (Bits 20-23)
    constexpr uint32_t BTN_RS_UP    = 1U << 20;
    constexpr uint32_t BTN_RS_DOWN  = 1U << 21;
    constexpr uint32_t BTN_RS_LEFT  = 1U << 22;
    constexpr uint32_t BTN_RS_RIGHT = 1U << 23;
}

class InputMapper {
public:
    InputMapper();
    ~InputMapper();

    // Processes SDL keyboard, controller button, and hotplug events.
    // Returns true if Escape was pressed to toggle the launcher UI.
    bool process_event(const SDL_Event& event, bool ignore_keyboard);

    // Polls analog sticks & triggers and returns the combined 32-bit input mask
    uint32_t get_input_state();

private:
    uint32_t m_key_state  = 0;
    uint32_t m_pad_buttons = 0;
    uint32_t m_pad_axes    = 0;

    static constexpr int16_t STICK_DEADZONE   = 8000;
    static constexpr int16_t TRIGGER_DEADZONE = 8000;

    std::vector<SDL_GameController*> m_controllers;

    void open_all_controllers();
    void close_all_controllers();
    void update_analog_axes();

    static uint32_t map_key_to_mask(SDL_Keycode key);
    static uint32_t map_pad_button_to_mask(Uint8 button);
};