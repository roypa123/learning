cmake_minimum_required(VERSION 3.16)

project(audiobook
        VERSION 1.0.0
        DESCRIPTION "Audio Programming in C++ - companion library"
        LANGUAGES CXX)

# --- language settings -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- default to a Release build ----------------------------------------
# CMake's default build type is EMPTY, meaning no optimisation flags at all.
# Unoptimised DSP can be 10x too slow. See Chapter 5.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- warnings ----------------------------------------------------------
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- optional sanitizers (Exercise 17.6) -------------------------------
option(ENABLE_SANITIZERS "Build with address and UB sanitizers" OFF)
if(ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# --- put all binaries in one place -------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- subdirectories ----------------------------------------------------
add_subdirectory(lib)
add_subdirectory(tests)
add_subdirectory(examples)

enable_testing()
add_test(NAME unit_tests COMMAND audio_tests)
