cmake_minimum_required(VERSION 3.10)
project(AllInOneEmulator)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where to find our public headers
include_directories(include)

# Find the SDL2 library on your system
find_package(SDL2 REQUIRED CONFIG)
include_directories(${SDL2_INCLUDE_DIRS})

# Define the source files
set(SOURCES
        src/Launcher/main.cpp
        src/Core_GBC/gbc_core.cpp
        src/Core_GBC/mmu.cpp
        src/Core_GBC/cpu.cpp
)

# Create the executable
add_executable(EmulatorLauncher ${SOURCES})

# Link using the imported target
target_link_libraries(EmulatorLauncher
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)