cmake_minimum_required(VERSION 3.27)

get_filename_component(project_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(emsdk_source "${project_root}/emsdk")
set(emsdk_install "${project_root}/.emsdk")

if(NOT EXISTS "${emsdk_source}/emsdk.py")
    message(FATAL_ERROR
        "Initialize the SDK submodule first: git submodule update --init emsdk")
endif()

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

# emsdk installs relative to its own script, with no separate install prefix.
# Run a copy so the pinned submodule stays clean and all generated data lives
# under the ignored .emsdk directory. Recopying also picks up submodule updates.
file(COPY "${emsdk_source}/" DESTINATION "${emsdk_install}"
    PATTERN ".git" EXCLUDE)

# Resolve the release from the pinned submodule, rather than a remote latest.
file(READ "${emsdk_source}/emscripten-releases-tags.json" emsdk_tags)
string(JSON emsdk_version GET "${emsdk_tags}" aliases latest)
message(STATUS "Installing Emscripten ${emsdk_version} in ${emsdk_install}")

foreach(action IN ITEMS install activate)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${emsdk_install}/emsdk.py"
            "${action}" "${emsdk_version}"
        WORKING_DIRECTORY "${emsdk_install}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()

message(STATUS "SDK ready. Configure with cmake --preset wasm32-emscripten-emcc-debug")
