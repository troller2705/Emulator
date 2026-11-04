cmake_minimum_required(VERSION 3.10)

# Bypass vcpkg on MinGW to force building SDL2 & ImGui from source with your exact GCC 15+ compiler
if(MINGW)
    set(CMAKE_TOOLCHAIN_FILE "" CACHE INTERNAL "")
    message(STATUS "MinGW detected: Bypassing vcpkg to ensure ABI compatibility with GCC.")
else()
    # Standard vcpkg detection for MSVC / Linux if not provided
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
        set(_VCPKG_CANDIDATES
                "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
                "$ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake"
                "$ENV{USERPROFILE}/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
                "$ENV{HOME}/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
                "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
                "D:/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
        )
        foreach(_CANDIDATE IN LISTS _VCPKG_CANDIDATES)
            if(EXISTS "${_CANDIDATE}")
                set(CMAKE_TOOLCHAIN_FILE "${_CANDIDATE}" CACHE FILEPATH "vcpkg toolchain" FORCE)
                break()
            endif()
        endforeach()
    endif()
endif()

project(AllInOneEmulator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_STATIC_EXE "Statically link runtime and libraries into a standalone executable" OFF)

# 2. Static runtime flags for MinGW / GCC / Clang when requested
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
elseif(UNIX AND NOT APPLE AND BUILD_STATIC_EXE)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

include(FetchContent)

# 3. Locate SDL2 (vcpkg / System Package -> FetchContent fallback)
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_SDL2 QUIET sdl2)
    endif()
endif()

if(NOT SDL2_FOUND AND NOT PC_SDL2_FOUND)
    message(STATUS "SDL2 not found on system; fetching SDL2 from source...")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-2.30.9
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

# 4. Locate Dear ImGui (vcpkg / System Package -> FetchContent fallback)
find_package(imgui CONFIG QUIET)
if(NOT imgui_FOUND)
    message(STATUS "imgui package not found on system; fetching Dear ImGui from source...")
    FetchContent_Declare(
            imgui_src
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        v1.91.5
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui_src)

    add_library(imgui STATIC
            "${imgui_src_SOURCE_DIR}/imgui.cpp"
            "${imgui_src_SOURCE_DIR}/imgui_demo.cpp"
            "${imgui_src_SOURCE_DIR}/imgui_draw.cpp"
            "${imgui_src_SOURCE_DIR}/imgui_tables.cpp"
            "${imgui_src_SOURCE_DIR}/imgui_widgets.cpp"
    )
    target_include_directories(imgui PUBLIC "${imgui_src_SOURCE_DIR}")
    add_library(imgui::imgui ALIAS imgui)
endif()

# 5. Emulator Sources
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

        # Super Nintendo Entertainment System Core
        src/Core_SNES/snes_core.cpp
        src/Core_SNES/snes_cpu.cpp
        src/Core_SNES/snes_mmu.cpp
        src/Core_SNES/snes_ppu.cpp
        src/Core_SNES/snes_apu.cpp

        # SEGA Genesis Core
        src/Core_Genesis/genesis_core.cpp
        src/Core_Genesis/genesis_mmu.cpp
        src/Core_Genesis/genesis_cpu.cpp
        src/Core_Genesis/genesis_vdp.cpp

        # ImGui Backends
        src/imgui/imgui_impl_sdl2.cpp
        src/imgui/imgui_impl_sdlrenderer2.cpp
)

add_executable(EmulatorLauncher ${SOURCES})

# Explicitly list include directories (CMake does not expand globs like include/*)
target_include_directories(EmulatorLauncher PRIVATE include include/*)

# 6. Link SDL2 and ImGui across any configuration
if(TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static)
    target_link_libraries(EmulatorLauncher PRIVATE
            $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
            $<IF:$<BOOL:${BUILD_STATIC_EXE}>,$<IF:$<TARGET_EXISTS:SDL2::SDL2-static>,SDL2::SDL2-static,SDL2::SDL2>,$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>>
    )
    # Ensure both <SDL2/SDL.h> and <SDL.h> resolve cleanly when built via FetchContent
    if(DEFINED SDL2_SOURCE_DIR)
        target_include_directories(EmulatorLauncher PRIVATE "${SDL2_SOURCE_DIR}/include")
    endif()
elseif(PC_SDL2_FOUND)
    target_include_directories(EmulatorLauncher PRIVATE ${PC_SDL2_INCLUDE_DIRS})
    target_link_libraries(EmulatorLauncher PRIVATE ${PC_SDL2_LINK_LIBRARIES})
endif()

target_link_libraries(EmulatorLauncher PRIVATE imgui::imgui)