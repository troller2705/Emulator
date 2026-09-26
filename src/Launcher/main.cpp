#include <cstring>
#include <SDL2/SDL.h>
#include "gbc_core.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

std::vector<uint8_t> load_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM: " << filepath << "\n";
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

const int GB_WIDTH = 160;
const int GB_HEIGHT = 144;
const int SCALE = 4;

// Mask constants matching the bit-shifts in our MMU
const uint8_t BTN_DOWN   = 1 << 7;
const uint8_t BTN_UP     = 1 << 6;
const uint8_t BTN_LEFT   = 1 << 5;
const uint8_t BTN_RIGHT  = 1 << 4;
const uint8_t BTN_START  = 1 << 3;
const uint8_t BTN_SELECT = 1 << 2;
const uint8_t BTN_B      = 1 << 1;
const uint8_t BTN_A      = 1 << 0;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("Game Boy Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GB_WIDTH * SCALE, GB_HEIGHT * SCALE, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH, GB_HEIGHT);

    GBCCore core;
    std::vector<uint8_t> rom_data = load_file("D:/Emulator/ROMs/PokemonRed.gb");
    if (rom_data.empty() || !core.load_rom(rom_data)) {
        std::cerr << "Failed to load ROM into core!\n";
        return -1;
    }

    bool is_running = true;
    SDL_Event event;

    // --- TIMING VARIABLES ---
    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;
    uint32_t frame_start;
    int frame_time;

    uint8_t current_input = 0; // The live input state

    while (is_running) {
        frame_start = SDL_GetTicks(); // Record when the frame started

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                is_running = false;
            }
            // Controller Hot-plugging
            else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameControllerOpen(event.cdevice.which);
            }

            // Keyboard Input
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                bool is_pressed = (event.type == SDL_KEYDOWN);
                uint8_t mask = 0;
                switch (event.key.keysym.sym) {
                    case SDLK_UP:     mask = BTN_UP;     break;
                    case SDLK_DOWN:   mask = BTN_DOWN;   break;
                    case SDLK_LEFT:   mask = BTN_LEFT;   break;
                    case SDLK_RIGHT:  mask = BTN_RIGHT;  break;
                    case SDLK_RETURN: mask = BTN_START;  break;
                    case SDLK_RSHIFT: mask = BTN_SELECT; break;
                    case SDLK_z:      mask = BTN_A;      break;
                    case SDLK_x:      mask = BTN_B;      break;
                }
                if (is_pressed) current_input |= mask;
                else            current_input &= ~mask;
            }

            // Controller Input
            else if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
                bool is_pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
                uint8_t mask = 0;
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    mask = BTN_UP;     break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  mask = BTN_DOWN;   break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  mask = BTN_LEFT;   break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: mask = BTN_RIGHT;  break;
                    case SDL_CONTROLLER_BUTTON_START:      mask = BTN_START;  break;
                    case SDL_CONTROLLER_BUTTON_BACK:       mask = BTN_SELECT; break;
                    case SDL_CONTROLLER_BUTTON_B:          mask = BTN_A;      break; // Map Xbox B / Switch A to GB A
                    case SDL_CONTROLLER_BUTTON_A:          mask = BTN_B;      break; // Map Xbox A / Switch B to GB B
                }
                if (is_pressed) current_input |= mask;
                else            current_input &= ~mask;
            }
        }

        // Send live inputs to the core
        core.set_input(current_input);

        core.run_frame();

        SDL_UpdateTexture(texture, nullptr, core.get_ppu()->get_framebuffer(), GB_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // --- FPS THROTTLE ---
        frame_time = SDL_GetTicks() - frame_start;
        if (FRAME_DELAY > frame_time) {
            SDL_Delay(FRAME_DELAY - frame_time); // Sleep to lock at 60 FPS
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}