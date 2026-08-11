cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(streams)

include(cmake/build-apk.cmake)
include(CTest)
include(GoogleTest)

add_subdirectory(src)
