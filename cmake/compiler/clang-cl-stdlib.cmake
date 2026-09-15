# Run before compiler detection: CMake does not discover the MSVC STL module
# sources for clang-cl. Clang must compile its own copies of these modules.
if(CMAKE_CXX_STDLIB_MODULES_JSON)
    return()
endif()

file(TO_CMAKE_PATH "$ENV{ASPIRE_MSVC_DIR}/modules" _aspire_std_module_dir)
foreach(_module std std.compat)
    if(NOT EXISTS "${_aspire_std_module_dir}/${_module}.ixx")
        message(FATAL_ERROR
            "Missing ${_aspire_std_module_dir}/${_module}.ixx. Set ASPIRE_MSVC_DIR to an installed MSVC toolset, or provide CMAKE_CXX_STDLIB_MODULES_JSON.")
    endif()
endforeach()

# MSVC's modules.json uses module-sources; CMake needs the normalized modules
# format below, containing the absolute source path for each logical name.
set(CMAKE_CXX_STDLIB_MODULES_JSON "${CMAKE_BINARY_DIR}/clang-cl-stdlib.modules.json")
file(CONFIGURE OUTPUT "${CMAKE_CXX_STDLIB_MODULES_JSON}" CONTENT [=[
{
    "version": 1,
    "revision": 1,
    "modules": [
        {
            "logical-name": "std",
            "source-path": "@_aspire_std_module_dir@/std.ixx",
            "is-std-library": true
        },
        {
            "logical-name": "std.compat",
            "source-path": "@_aspire_std_module_dir@/std.compat.ixx",
            "is-std-library": true
        }
    ]
}
]=] @ONLY)
