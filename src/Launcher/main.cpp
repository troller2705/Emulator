#include <cstring>
#include <SDL2/SDL.h>
#include "IEmulatorCore.h"
#include "gbc_core.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

// --- IMGUI HEADERS ---
#include <imgui.h>

#include "gba_core.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_memory_editor.h"

namespace fs = std::filesystem;

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

// Available Core Types
enum class CoreType {
    AutoDetect = 0,
    GB_GBC,
    GBA
};

const char* CORE_NAMES[] = {
    "Auto-Detect (By Extension)",
    "Game Boy / Game Boy Color (GBCCore)",
    "Game Boy Advance (GBACore)"
};

std::vector<fs::path> scan_rom_directory(const std::string& dir_path) {
    std::vector<fs::path> roms;
    std::error_code ec;
    if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
        return roms;
    }

    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".gb" || ext == ".gbc" || ext == ".gba") {
                roms.push_back(entry.path());
            }
        }
    }
    std::sort(roms.begin(), roms.end());
    return roms;
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
        GB_WIDTH * SCALE, GB_HEIGHT * SCALE, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH, GB_HEIGHT);

    SDL_Texture* tile_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        128, 192);

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

    // --- LAUNCHER & EMULATOR STATE ---
    std::unique_ptr<IEmulatorCore> core = nullptr;
    std::string current_rom_path = "";
    std::string current_save_path = "";
    char rom_dir_buf[256] = "D:/Emulator/ROMs";
    std::vector<fs::path> available_roms = scan_rom_directory(rom_dir_buf);

    int selected_core_idx = static_cast<int>(CoreType::AutoDetect);
    bool show_launcher = true;
    bool show_cpu_debugger = true;
    bool show_vram_viewer = true;
    bool is_paused = false;

    static MemoryEditor mem_edit;
    mem_edit.Open = false; // Toggleable from the Tools menu

    mem_edit.ReadFn = [](const ImU8* data, size_t off, void* user_data) -> ImU8 {
        auto* emulator_core = reinterpret_cast<IEmulatorCore*>(const_cast<ImU8*>(data));
        return emulator_core->debug_read_memory(static_cast<uint16_t>(off));
    };

    mem_edit.WriteFn = [](ImU8* data, size_t off, ImU8 d, void* user_data) {
        auto* emulator_core = reinterpret_cast<IEmulatorCore*>(data);
        emulator_core->debug_write_memory(static_cast<uint16_t>(off), d);
    };

    // Helper Lambda to Instantiate Core & Load ROM
    int current_tex_w = GB_WIDTH;
    int current_tex_h = GB_HEIGHT;

    auto launch_rom = [&](const std::string& path, CoreType core_choice) {
        if (core && !current_save_path.empty()) {
            core->save_battery(current_save_path);
        }

        if (audio_device != 0) {
            SDL_ClearQueuedAudio(audio_device);
        }

        std::vector<uint8_t> rom_data = load_file(path);
        if (rom_data.empty()) return false;

        std::unique_ptr<IEmulatorCore> new_core = nullptr;
        std::string ext = fs::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (core_choice == CoreType::AutoDetect) {
            if (ext == ".gb" || ext == ".gbc") {
                new_core = std::make_unique<GBCCore>();
            } else if (ext == ".gba") {
                new_core = std::make_unique<GBACore>();
            }
        } else if (core_choice == CoreType::GB_GBC) {
            new_core = std::make_unique<GBCCore>();
        } else if (core_choice == CoreType::GBA) {
            new_core = std::make_unique<GBACore>();
        }

        if (!new_core || !new_core->load_rom(rom_data)) {
            std::cerr << "Failed to initialize core for ROM: " << path << "\n";
            return false;
        }

        // Resize the streaming SDL_Texture if the new core has a different resolution
        int new_w = new_core->get_video_width();
        int new_h = new_core->get_video_height();
        if (new_w != current_tex_w || new_h != current_tex_h) {
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer,
                SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                new_w, new_h);
            current_tex_w = new_w;
            current_tex_h = new_h;
        }

        current_rom_path = path;
        current_save_path = path.substr(0, path.find_last_of('.')) + ".sav";
        new_core->load_battery(current_save_path);

        core = std::move(new_core);
        is_paused = false;
        show_launcher = false;
        return true;
    };

    bool is_running = true;
    SDL_Event event;
    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;
    uint32_t frame_start;
    int frame_time;
    uint8_t current_input = 0;

    while (is_running) {
        frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                is_running = false;
            }
            else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameControllerOpen(event.cdevice.which);
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
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
                        case SDLK_ESCAPE:
                            if (is_pressed) show_launcher = !show_launcher;
                            break;
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

        // --- 1. MAIN MENU BAR ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open ROM Launcher", "Esc", show_launcher)) {
                    show_launcher = !show_launcher;
                }
                if (ImGui::MenuItem("Reset ROM", nullptr, false, core != nullptr)) {
                    launch_rom(current_rom_path, static_cast<CoreType>(selected_core_idx));
                }
                if (ImGui::MenuItem("Close / Unload ROM", nullptr, false, core != nullptr)) {
                    if (!current_save_path.empty()) core->save_battery(current_save_path);
                    core.reset();
                    current_rom_path.clear();
                    current_save_path.clear();
                    show_launcher = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    is_running = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools & Debuggers")) {
                ImGui::MenuItem("CPU Debugger", nullptr, &show_cpu_debugger);
                ImGui::MenuItem("Memory Hex Editor", nullptr, &mem_edit.Open);
                ImGui::MenuItem("VRAM Tile Viewer", nullptr, &show_vram_viewer);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (core) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Running: %s",
                    fs::path(current_rom_path).filename().string().c_str());
            } else {
                ImGui::TextDisabled("No ROM Loaded");
            }

            ImGui::EndMainMenuBar();
        }

        // --- 2. ROM LAUNCHER & CORE SELECTOR WINDOW ---
        if (show_launcher) {
            ImGui::SetNextWindowSize(ImVec2(500, 380), ImGuiCond_FirstUseEver);
            ImGui::Begin("Emulator Launcher", &show_launcher);

            ImGui::Text("1. Select Emulation Core:");
            ImGui::Combo("##CoreSelector", &selected_core_idx, CORE_NAMES, IM_ARRAYSIZE(CORE_NAMES));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("2. ROM Directory:");
            ImGui::PushItemWidth(-100.0f);
            ImGui::InputText("##RomDir", rom_dir_buf, sizeof(rom_dir_buf));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(90.0f, 0.0f))) {
                available_roms = scan_rom_directory(rom_dir_buf);
            }

            ImGui::Spacing();
            ImGui::Text("Available ROMs (%zu):", available_roms.size());

            ImGui::BeginChild("ROMList", ImVec2(0, -10), true);
            if (available_roms.empty()) {
                ImGui::TextDisabled("No .gb or .gbc files found in directory.");
            } else {
                for (const auto& rom_path : available_roms) {
                    std::string filename = rom_path.filename().string();
                    std::string ext = rom_path.extension().string();

                    // Tag badge for GB vs GBC
                    ImGui::TextDisabled("[%s]", ext.c_str());
                    ImGui::SameLine();

                    if (ImGui::Selectable(filename.c_str(), current_rom_path == rom_path.string())) {
                        launch_rom(rom_path.string(), static_cast<CoreType>(selected_core_idx));
                    }
                }
            }
            ImGui::EndChild();

            ImGui::End();
        }

        // --- 3. DEBUGGER WINDOWS (Active when Core is loaded) ---
        if (core) {
            if (show_cpu_debugger) {
                ImGui::Begin("CPU Debugger", &show_cpu_debugger);
                ImGui::Text("Emulation Speed: %.1f FPS", io.Framerate);
                ImGui::Separator();

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
                CPUState state = core->get_cpu_state();
                ImGui::Text("PC: 0x%04X", state.PC);
                ImGui::Text("SP: 0x%04X", state.SP);
                ImGui::Spacing();
                ImGui::Text("AF: 0x%02X%02X", state.A, state.F);
                ImGui::Text("BC: 0x%02X%02X", state.B, state.C);
                ImGui::Text("DE: 0x%02X%02X", state.D, state.E);
                ImGui::Text("HL: 0x%02X%02X", state.H, state.L);
                ImGui::End();
            }

            if (mem_edit.Open) {
                mem_edit.DrawWindow("Memory Hex Editor", core.get(), 0x10000);
            }

            if (show_vram_viewer) {
                ImGui::Begin("VRAM Tile Viewer", &show_vram_viewer);
                if (ImGui::IsWindowAppearing() || ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) {
                    core->debug_update_tile_buffer();
                    SDL_UpdateTexture(tile_texture, nullptr, core->debug_get_tile_buffer(), 128 * sizeof(uint32_t));
                }
                ImGui::Image((ImTextureID)(intptr_t)tile_texture, ImVec2(256.0f, 384.0f));
                ImGui::End();
            }

            // --- STEP EMULATION CORE ---
            core->set_input(current_input);
            if (!is_paused) {
                core->run_frame();
            }

            size_t sample_count = core->get_audio_sample_count();
            if (sample_count > 0 && audio_device != 0) {
                SDL_QueueAudio(audio_device, core->get_audio_buffer(), sample_count * sizeof(float));
            }

            SDL_UpdateTexture(texture, nullptr, core->get_video_buffer(), current_tex_w * sizeof(uint32_t));
        }

        // --- GRAPHICS RENDERING ---
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);

        if (core) {
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        frame_time = SDL_GetTicks() - frame_start;
        if (FRAME_DELAY > frame_time) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    if (core && !current_save_path.empty()) {
        core->save_battery(current_save_path);
    }

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