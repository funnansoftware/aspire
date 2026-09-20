cmake_minimum_required(VERSION 4.4.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_MODULE_STD OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(aspire)

include(CTest)
include(GNUInstallDirs)

# The legacy src/ libraries and srd-lite app are not part of the SDL3 build.
add_subdirectory(app)

include(cmake/build-apk.cmake)
include(cmake/target/clang-tidy.cmake)
