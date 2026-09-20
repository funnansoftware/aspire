if(ANDROID)
    message(FATAL_ERROR "Android builds are temporarily disabled pending NDK import std support.")
endif()

find_package(SDL3 3.2 CONFIG REQUIRED)

add_library(evford-world STATIC world.cpp)
target_include_directories(evford-world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(evford-world PUBLIC cxx_std_23)
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
