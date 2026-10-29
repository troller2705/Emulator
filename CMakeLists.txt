cmake_minimum_required(VERSION 3.10)

# Fallback vcpkg toolchain detection if not provided by preset
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
    if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    elseif(EXISTS "D:/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "D:/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    elseif(EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
    endif()
endif()

project(AllInOneEmulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED CONFIG)
find_package(imgui CONFIG REQUIRED)

set(SOURCES
        src/Launcher/main.cpp
        src/Launcher/input_mapper.cpp

        # Game Boy / Game Boy Color Core
        src/Core_GBC/gbc_core.cpp
        src/Core_GBC/gbc_mmu.cpp
        src/Core_GBC/gbc_cpu.cpp
        src/Core_GBC/gbc_ppu.cpp
        src/Core_GBC/gbc_timer.cpp
        src/Core_GBC/gbc_apu.cpp

        # Game Boy Advance Core
        src/Core_GBA/gba_core.cpp
        src/Core_GBA/gba_mmu.cpp
        src/Core_GBA/gba_cpu.cpp
        src/Core_GBA/gba_ppu.cpp
        src/Core_GBA/gba_timer.cpp
        src/Core_GBA/gba_apu.cpp

        # ImGui Backends
        src/imgui/imgui_impl_sdl2.cpp
        src/imgui/imgui_impl_sdlrenderer2.cpp
)

add_executable(EmulatorLauncher ${SOURCES})

target_include_directories(EmulatorLauncher PRIVATE include include/*)

# Link SDL2 and vcpkg's target-managed ImGui library safely
target_link_libraries(EmulatorLauncher
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        imgui::imgui
)

# Automatically copy SDL2.dll to the build output directory so executables run right away
if(WIN32)
    foreach(CONFIG_DIR "cmake-build-debug" "cmake-build-release-static")
        if(EXISTS "${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/vcpkg_installed/x64-mingw-dynamic/bin/SDL2.dll")
            add_custom_command(TARGET EmulatorLauncher POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/vcpkg_installed/x64-mingw-dynamic/bin/SDL2.dll"
                    "$<TARGET_FILE_DIR:EmulatorLauncher>/SDL2.dll"
            )
        endif()
    endforeach()
endif()