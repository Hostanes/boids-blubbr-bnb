cmake_minimum_required(VERSION 3.16)
project(MechArenaDemo LANGUAGES C)

# ------------------- C standard -------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ------------------- Output -------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# ------------------- Source Files -------------------
set(SOURCES
    src/main.c
    src/game.c
    src/engine.c
    src/engine_components.c

    src/systems/systems.c
)

add_executable(MechArenaDemo ${SOURCES})

# ------------------- Include Directories -------------------
target_include_directories(MechArenaDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ------------------- Raylib -------------------
# First try a system-installed raylib package
find_package(raylib 4.0 QUIET)

# If not found, fetch raylib automatically (handy for CI / new machines)
if(NOT raylib_FOUND)
    include(FetchContent)

    # Raylib options (optional)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 4.0.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

target_link_libraries(MechArenaDemo PRIVATE raylib)

# ------------------- Platform-specific libs -------------------
if(UNIX AND NOT APPLE)
    target_link_libraries(MechArenaDemo PRIVATE m pthread dl)
endif()

# macOS usually doesn't need m/pthread/dl manually when linking raylib;
# raylib handles frameworks when built properly.
