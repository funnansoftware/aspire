cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(aspire)

include(cmake/build-apk.cmake)
include(CTest)
include(GoogleTest)
include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(app)
