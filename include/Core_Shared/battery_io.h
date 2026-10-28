#pragma once
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>

namespace CoreShared {

    inline bool load_battery_file(const std::string& path, uint8_t* data, size_t size) {
        if (!data || size == 0) return false;
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
            std::cout << "Loaded save file: " << path << "\n";
            return true;
        }
        return false;
    }

    inline bool save_battery_file(const std::string& path, const uint8_t* data, size_t size) {
        if (!data || size == 0) return false;
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            std::cout << "Saved game to: " << path << "\n";
            return true;
        }
        return false;
    }

} // namespace CoreShared