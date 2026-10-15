# scopegpu2d draws scope's picture with SDL's 2D renderer, as scope does, but on SDL's GPU renderer with scopegpu's
# shaders for the glow and the backdrop, set on the renderer's draws as custom render state. It needs an SDL_GPU backend,
# which the web doesn't have, and scope isn't built for Android.
if(ANDROID OR EMSCRIPTEN)
    return()
endif()

find_package(SDL3 3.4 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# Draws the picture with SDL's GPU renderer and scopegpu's shaders.
add_library(scopegpu2d-painter STATIC)
target_sources(scopegpu2d-painter PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Painter.ixx
)
target_link_libraries(scopegpu2d-painter PUBLIC scope-glow scope-tessellation scope-typesetter scopegpu-shaders SDL3::SDL3)

add_executable(scopegpu2d main.cpp)
target_link_libraries(scopegpu2d PRIVATE scopegpu2d-painter SDL3_ttf::SDL3_ttf SDL3::SDL3)

# The readout font is read from next to the executable, with its licence beside it, as scope reads it.
set(SCOPEGPU2D_FONT_DIR "${CMAKE_SOURCE_DIR}/assets/b612")
set(SCOPEGPU2D_FONT_FILES
    "${SCOPEGPU2D_FONT_DIR}/B612Mono-Regular.ttf"
    "${SCOPEGPU2D_FONT_DIR}/OFL.txt"
)
add_custom_command(TARGET scopegpu2d POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:scopegpu2d>/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SCOPEGPU2D_FONT_FILES} "$<TARGET_FILE_DIR:scopegpu2d>/fonts"
    VERBATIM
)
install(TARGETS scopegpu2d RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${SCOPEGPU2D_FONT_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}/fonts")

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    # Draws offscreen, so it needs SDL's GPU renderer but no display, as scopegpu's smoke test does: runners with no GPU
    # device to make, or no video driver that makes one, skip it.
    add_test(NAME scopegpu2d-smoke COMMAND scopegpu2d --offscreen --paused --frames=3)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(scopegpu2dSmokeVideo offscreen)
    else()
        set(scopegpu2dSmokeVideo dummy)
    endif()
    set_tests_properties(scopegpu2d-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=${scopegpu2dSmokeVideo}"
        SKIP_REGULAR_EXPRESSION "No GPU renderer:"
        TIMEOUT 20
    )
endif()
