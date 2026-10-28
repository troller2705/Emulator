cmake_minimum_required(VERSION 3.10)

# Fallback vcpkg toolchain detection if not provided by preset
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
    if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    elseif(EXISTS "D:/.vcpkg-clion/vcpkg/scripts/buildsystems")
        set(CMAKE_TOOLCHAIN_FILE "D:/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    elseif(EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    endif()
endif()

project(AllInOneEmulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_STATIC_EXE "Statically bundle C++ runtime and libraries" OFF)

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
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        imgui::imgui
)

# If using dynamic SDL2.dll from cmake-build-debug, copy it to the output folder automatically
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/x64-mingw-dynamic/bin/SDL2.dll")
    add_custom_command(TARGET EmulatorLauncher POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/x64-mingw-dynamic/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:EmulatorLauncher>/SDL2.dll"
    )
endif()