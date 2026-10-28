cmake_minimum_required(VERSION 3.15)
project(AllInOneEmulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_STATIC_EXE "Bundle all libraries into a single standalone executable" OFF)

if(BUILD_STATIC_EXE)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    elseif(UNIX AND NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
endif()

find_package(SDL2 REQUIRED CONFIG)
find_package(imgui CONFIG REQUIRED)

set(SOURCES
        src/Launcher/main.cpp
        src/Core_GBC/gbc_core.cpp
        src/Core_GBC/mmu.cpp
        src/Core_GBC/cpu.cpp
        src/Core_GBC/ppu.cpp
        src/Core_GBC/timer.cpp
        src/Core_GBC/apu.cpp
        src/Core_GBA/gba_core.cpp
        src/imgui/imgui_impl_sdl2.cpp
        src/imgui/imgui_impl_sdlrenderer2.cpp
)

add_executable(EmulatorLauncher ${SOURCES})

target_include_directories(EmulatorLauncher PRIVATE include)

target_link_libraries(EmulatorLauncher
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2-static>,SDL2::SDL2-static,SDL2::SDL2>
        imgui::imgui
)