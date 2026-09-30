#include <cstring>
#include <SDL2/SDL.h>
#include "IEmulatorCore.h"
#include "gbc_core.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>

// --- IMGUI HEADERS ---
#include <imgui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_memory_editor.h"

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
const int SCALE = 6;

const uint8_t BTN_DOWN   = 1 << 7;
const uint8_t BTN_UP     = 1 << 6;
const uint8_t BTN_LEFT   = 1 << 5;
const uint8_t BTN_RIGHT  = 1 << 4;
const uint8_t BTN_START  = 1 << 3;
const uint8_t BTN_SELECT = 1 << 2;
const uint8_t BTN_B      = 1 << 1;
const uint8_t BTN_A      = 1 << 0;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("Mega Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GB_WIDTH * SCALE, GB_HEIGHT * SCALE, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH, GB_HEIGHT);

    SDL_Texture* tile_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        128, 192); // 16 tiles wide, 24 tiles high

    // --- IMGUI SETUP ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // --- AUDIO SETUP ---
    SDL_AudioSpec audio_spec = {};
    audio_spec.freq = 44100;
    audio_spec.format = AUDIO_F32SYS;
    audio_spec.channels = 1;
    audio_spec.samples = 1024;
    audio_spec.callback = nullptr;

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &audio_spec, nullptr, 0);
    if (audio_device == 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << "\n";
    } else {
        SDL_PauseAudioDevice(audio_device, 0);
    }

    std::string rom_path = "D:/Emulator/ROMs/PokemonRed.gb";
    std::string save_path = rom_path.substr(0, rom_path.find_last_of('.')) + ".sav";

    std::vector<uint8_t> rom_data = load_file(rom_path);

    std::unique_ptr<IEmulatorCore> core = nullptr;
    if (rom_path.find(".gb") != std::string::npos) {
        core = std::make_unique<GBCCore>();
    }

    if (!core || !core->load_rom(rom_data)) {
        std::cerr << "Failed to load ROM or initialize core!\n";
        return -1;
    }

    core->load_battery(save_path);

    bool is_running = true;
    SDL_Event event;

    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;
    uint32_t frame_start;
    int frame_time;

    uint8_t current_input = 0;

    bool is_paused = false;

    // --- MEMORY EDITOR SETUP ---
    static MemoryEditor mem_edit;
    mem_edit.Open = true; // Start open

    // Callback to read from your MMU
    mem_edit.ReadFn = [](const ImU8* data, size_t off, void* user_data) -> ImU8 {
        // Cast the generic data pointer back to our IEmulatorCore
        auto* emulator_core = reinterpret_cast<IEmulatorCore*>(const_cast<ImU8*>(data));
        return emulator_core->debug_read_memory(static_cast<uint16_t>(off));
    };

    // Callback to write to your MMU
    mem_edit.WriteFn = [](ImU8* data, size_t off, ImU8 d, void* user_data) {
        auto* emulator_core = reinterpret_cast<IEmulatorCore*>(data);
        emulator_core->debug_write_memory(static_cast<uint16_t>(off), d);
    };

    while (is_running) {
        frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            // Give ImGui the event first
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                is_running = false;
            }
            else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameControllerOpen(event.cdevice.which);
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                // Only process keyboard for the game if ImGui doesn't want it
                if (!io.WantCaptureKeyboard) {
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
            }
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
                    case SDL_CONTROLLER_BUTTON_B:          mask = BTN_A;      break;
                    case SDL_CONTROLLER_BUTTON_A:          mask = BTN_B;      break;
                }
                if (is_pressed) current_input |= mask;
                else            current_input &= ~mask;
            }
        }

        // --- START IMGUI FRAME ---
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // --- RENDER UI WINDOWS ---
        ImGui::Begin("CPU Debugger");
        ImGui::Text("Emulation Speed: %.1f FPS", io.Framerate);

        ImGui::Separator();

        // Pause/Play Toggle
        if (is_paused) {
            if (ImGui::Button("Resume Execution")) is_paused = false;
            ImGui::SameLine();
            if (ImGui::Button("Step Instruction")) {
                core->step_instruction();
            }
        } else {
            if (ImGui::Button("Pause Execution")) is_paused = true;
        }

        ImGui::Separator();

        // Fetch and display live registers
        CPUState state = core->get_cpu_state();

        // ImGui::Text with standard printf formatting
        ImGui::Text("PC: 0x%04X", state.PC);
        ImGui::Text("SP: 0x%04X", state.SP);
        ImGui::Spacing();
        ImGui::Text("AF: 0x%02X%02X", state.A, state.F);
        ImGui::Text("BC: 0x%02X%02X", state.B, state.C);
        ImGui::Text("DE: 0x%02X%02X", state.D, state.E);
        ImGui::Text("HL: 0x%02X%02X", state.H, state.L);

        ImGui::End();

        if (mem_edit.Open) {
            mem_edit.DrawWindow("Memory Hex Editor", core.get(), 0x10000);
        }

        // --- VRAM TILE VIEWER ---
        ImGui::Begin("VRAM Tile Viewer");

        // Only decode and update if the window is actually visible
        if (ImGui::IsWindowAppearing() || ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) {
            core->debug_update_tile_buffer();
            SDL_UpdateTexture(tile_texture, nullptr, core->debug_get_tile_buffer(), 128 * sizeof(uint32_t));
        }

        // Draw the texture in ImGui. Scale it up by 2 for readability (256x384)
        // With the SDL2_Renderer backend, we cast the SDL_Texture pointer to ImTextureID
        ImGui::Image((ImTextureID)(intptr_t)tile_texture, ImVec2(256.0f, 384.0f));

        ImGui::End();

        // --- CORE EMULATION ---
        core->set_input(current_input);

        // Only run the continuous frame loop if we aren't paused
        if (!is_paused) {
            core->run_frame();
        }

        size_t sample_count = core->get_audio_sample_count();
        if (sample_count > 0 && audio_device != 0) {
            SDL_QueueAudio(audio_device, core->get_audio_buffer(), sample_count * sizeof(float));
        }

        // --- GRAPHICS RENDERING ---
        SDL_UpdateTexture(texture, nullptr, core->get_video_buffer(), GB_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);

        // Draw the Game
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);

        // Draw ImGui over the Game
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);

        frame_time = SDL_GetTicks() - frame_start;
        if (FRAME_DELAY > frame_time) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    core->save_battery(save_path);

    // --- CLEANUP ---
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }

    SDL_DestroyTexture(tile_texture);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}