#include "../../include/Launcher/input_mapper.h"
#include <algorithm>

using namespace EmulatorInput;

InputMapper::InputMapper() {
    open_all_controllers();
}

InputMapper::~InputMapper() {
    close_all_controllers();
}

void InputMapper::open_all_controllers() {
    close_all_controllers();
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            if (SDL_GameController* pad = SDL_GameControllerOpen(i)) {
                m_controllers.push_back(pad);
            }
        }
    }
}

void InputMapper::close_all_controllers() {
    for (SDL_GameController* pad : m_controllers) {
        if (pad) SDL_GameControllerClose(pad);
    }
    m_controllers.clear();
}

uint32_t InputMapper::map_key_to_mask(SDL_Keycode key) {
    switch (key) {
        // D-Pad (Arrow Keys)
        case SDLK_UP:        return BTN_UP;
        case SDLK_DOWN:      return BTN_DOWN;
        case SDLK_LEFT:      return BTN_LEFT;
        case SDLK_RIGHT:     return BTN_RIGHT;

        // Primary Action Buttons
        case SDLK_z:         return BTN_A;
        case SDLK_x:         return BTN_B;
        case SDLK_c:         return BTN_X;
        case SDLK_v:         return BTN_Y;
        case SDLK_RETURN:    return BTN_START;
        case SDLK_RSHIFT:
        case SDLK_BACKSPACE: return BTN_SELECT;

        // Shoulder Buttons L / R (A/S and Q/E)
        case SDLK_q:         return BTN_L;
        case SDLK_e:         return BTN_R;

        // Triggers L2 / R2 (1 / 2)
        case SDLK_1:         return BTN_L2;
        case SDLK_2:         return BTN_R2;

        // Left Stick Click / Right Stick Click (3 / 4)
        case SDLK_3:         return BTN_L3;
        case SDLK_4:         return BTN_R3;

        // Left Stick Keyboard Emulation (Numpad 8/2/4/6)
        case SDLK_w:      return BTN_LS_UP;
        case SDLK_s:      return BTN_LS_DOWN;
        case SDLK_a:      return BTN_LS_LEFT;
        case SDLK_d:      return BTN_LS_RIGHT;

        // Right Stick Keyboard Emulation (I/K/J/L)
        case SDLK_i:         return BTN_RS_UP;
        case SDLK_k:         return BTN_RS_DOWN;
        case SDLK_j:         return BTN_RS_LEFT;
        case SDLK_l:         return BTN_RS_RIGHT;

        default:             return 0;
    }
}

uint32_t InputMapper::map_pad_button_to_mask(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:       return BTN_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return BTN_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return BTN_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return BTN_RIGHT;
        case SDL_CONTROLLER_BUTTON_START:         return BTN_START;
        case SDL_CONTROLLER_BUTTON_BACK:          return BTN_SELECT;
        case SDL_CONTROLLER_BUTTON_B:             return BTN_A; // Nintendo-style East = A
        case SDL_CONTROLLER_BUTTON_A:             return BTN_B; // Nintendo-style South = B
        case SDL_CONTROLLER_BUTTON_Y:             return BTN_X;
        case SDL_CONTROLLER_BUTTON_X:             return BTN_Y;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return BTN_L;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_R;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return BTN_L3;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return BTN_R3;
        default:                                  return 0;
    }
}

bool InputMapper::process_event(const SDL_Event& event, bool ignore_keyboard) {
    if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED) {
        open_all_controllers();
        return false;
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        if (ignore_keyboard) return false;

        bool is_pressed = (event.type == SDL_KEYDOWN);
        if (event.key.keysym.sym == SDLK_ESCAPE && is_pressed && event.key.repeat == 0) {
            return true; // Toggle launcher menu
        }

        uint32_t mask = map_key_to_mask(event.key.keysym.sym);
        if (mask != 0) {
            if (is_pressed) m_key_state |= mask;
            else            m_key_state &= ~mask;
        }
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        bool is_pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
        uint32_t mask = map_pad_button_to_mask(event.cbutton.button);
        if (mask != 0) {
            if (is_pressed) m_pad_buttons |= mask;
            else            m_pad_buttons &= ~mask;
        }
    }

    return false;
}

void InputMapper::update_analog_axes() {
    m_pad_axes = 0;

    for (SDL_GameController* pad : m_controllers) {
        if (!pad || !SDL_GameControllerGetAttached(pad)) continue;

        int16_t lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        int16_t ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        int16_t rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        int16_t ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
        int16_t l2 = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        int16_t r2 = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        // Left Stick
        if (ly < -STICK_DEADZONE) m_pad_axes |= BTN_LS_UP;
        if (ly >  STICK_DEADZONE) m_pad_axes |= BTN_LS_DOWN;
        if (lx < -STICK_DEADZONE) m_pad_axes |= BTN_LS_LEFT;
        if (lx >  STICK_DEADZONE) m_pad_axes |= BTN_LS_RIGHT;

        // Right Stick
        if (ry < -STICK_DEADZONE) m_pad_axes |= BTN_RS_UP;
        if (ry >  STICK_DEADZONE) m_pad_axes |= BTN_RS_DOWN;
        if (rx < -STICK_DEADZONE) m_pad_axes |= BTN_RS_LEFT;
        if (rx >  STICK_DEADZONE) m_pad_axes |= BTN_RS_RIGHT;

        // Analog Triggers (L2 / R2)
        if (l2 > TRIGGER_DEADZONE) m_pad_axes |= BTN_L2;
        if (r2 > TRIGGER_DEADZONE) m_pad_axes |= BTN_R2;
    }
}

uint32_t InputMapper::get_input_state() {
    update_analog_axes();
    uint32_t combined = m_key_state | m_pad_buttons | m_pad_axes;

    // Also route Left Stick and Right Stick directions to standard D-Pad bits
    // so GB, GBC, and GBA games can be played smoothly with analog sticks
    if (combined & (BTN_LS_UP    | BTN_RS_UP))    combined |= BTN_UP;
    if (combined & (BTN_LS_DOWN  | BTN_RS_DOWN))  combined |= BTN_DOWN;
    if (combined & (BTN_LS_LEFT  | BTN_RS_LEFT))  combined |= BTN_LEFT;
    if (combined & (BTN_LS_RIGHT | BTN_RS_RIGHT)) combined |= BTN_RIGHT;

    return combined;
}