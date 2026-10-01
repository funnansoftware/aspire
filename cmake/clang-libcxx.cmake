set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Preserve vcpkg's Linux flags, including -fPIC for static dependencies.
include("${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/toolchains/linux.cmake")
