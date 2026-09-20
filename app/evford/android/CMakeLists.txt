cmake_minimum_required(VERSION 3.22)
project(evford-android LANGUAGES CXX)

# Gradle installs SDL through vcpkg before configuring this project. Keep the
# NDK toolchain supplied by AGP and resolve SDL from that installation only.
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(_sdl_triplet arm64-android-sdl)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(_sdl_triplet x64-android-sdl)
else()
    message(FATAL_ERROR "Unsupported evford Android ABI: ${ANDROID_ABI}")
endif()
if(NOT EVFORD_VCPKG_INSTALLED_DIR)
    message(FATAL_ERROR "Build Android through Gradle to install the vcpkg dependencies first.")
endif()
set(SDL3_DIR "${EVFORD_VCPKG_INSTALLED_DIR}/${_sdl_triplet}/${_sdl_triplet}/share/sdl3")
# AGP uses RelWithDebInfo for release APKs; vcpkg installs Debug and Release.
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
find_package(SDL3 3.2 CONFIG REQUIRED COMPONENTS SDL3-shared Jar
    PATHS "${SDL3_DIR}" NO_DEFAULT_PATH)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." evford)
