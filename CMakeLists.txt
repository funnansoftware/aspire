cmake_minimum_required(VERSION 4.4.2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
set(CMAKE_CXX_MODULE_STD ON)

project(aspire)

include(CTest)
include(GNUInstallDirs)

# The raylib-based srd-lite app is not part of the SDL3 build.
add_subdirectory(src)
add_subdirectory(app)

# Android packaging is retained in cmake/build-apk.cmake but temporarily disabled.
include(cmake/clang-format.cmake)
include(cmake/clang-tidy.cmake)
