#include <SDL2/SDL.h>
#include "gbc_core.h"
#include <iostream>

#include <fstream>
#include <vector>
#include <string>

std::vector<uint8_t> load_file(const std::string& filepath) {
    // Open the file in binary mode, starting at the end so we can easily check the file size
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM: " << filepath << "\n";
        return {};
    }

    // Get the size, allocate the vector, and seek back to the beginning
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }

    return {};
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL failed to initialize: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "All-in-One Launcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* gb_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    GBCCore gbc;

    // Put a real ROM in your project folder and update this path!
    std::vector<uint8_t> rom_data = load_file("D:/Emulator/ROMs/PokemonRed.gb");
    if (rom_data.empty() || !gbc.load_rom(rom_data)) {
        std::cerr << "Failed to load ROM into core!\n";
        return -1;
    }
    bool is_running = true;
    SDL_Event event;

    while (is_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                is_running = false;
            }
        }

        gbc.run_frame();

        const uint32_t* video_buffer = gbc.get_video_buffer();

        SDL_UpdateTexture(gb_texture, nullptr, video_buffer, 160 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, gb_texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(gb_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}