cmake_minimum_required(VERSION 3.22)

# This directory also serves as the portable standalone entry point.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(evford LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
endif()

# Use portable headers without requiring C++ standard-library modules.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(SDL3 3.2 CONFIG REQUIRED)

add_library(evford-world STATIC world.cpp)
target_include_directories(evford-world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(evford-world PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ANDROID)
    add_library(evford SHARED main.cpp)
    # SDLActivity loads libmain.so after libSDL3.so.
    set_target_properties(evford PROPERTIES OUTPUT_NAME main)
else()
    add_executable(evford main.cpp)
endif()
target_link_libraries(evford PRIVATE evford-world SDL3::SDL3)

if(EMSCRIPTEN)
    set_target_properties(evford PROPERTIES SUFFIX ".html")
    target_link_options(evford PRIVATE "-sALLOW_MEMORY_GROWTH=1")
    install(FILES
        "$<TARGET_FILE:evford>"
        "$<TARGET_FILE_DIR:evford>/evford.js"
        "$<TARGET_FILE_DIR:evford>/evford.wasm"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/evford"
    )
elseif(NOT ANDROID)
    install(TARGETS evford RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(WIN32 AND TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET evford POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:evford>"
            VERBATIM
        )
        install(IMPORTED_RUNTIME_ARTIFACTS SDL3::SDL3-shared
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(evford-world-test test/world.test.cpp)
    target_link_libraries(evford-world-test PRIVATE evford-world)
    add_test(NAME evford-world COMMAND evford-world-test)
    add_test(NAME evford-smoke COMMAND evford --frames=3)
    set_tests_properties(evford-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 10
    )
endif()
