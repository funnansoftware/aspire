if(ANDROID)
    # scope targets desktop and the web, and SDL_ttf isn't built for Android; see vcpkg.json.
    return()
endif()

find_package(SDL3 3.4 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# The scope's state, which the tests check.
add_library(scope-world STATIC)
target_sources(scope-world PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        World.ixx
)
target_link_libraries(scope-world PUBLIC aspire-core aspire-graphics)

# A frame laid out as shapes and text, for the painter to draw.
add_library(scope-picture STATIC)
target_sources(scope-picture PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Picture.ixx
)
target_link_libraries(scope-picture PUBLIC scope-world)

# Shapes turned into antialiased triangles.
add_library(scope-mesh STATIC)
target_sources(scope-mesh PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Mesh.ixx
)
target_link_libraries(scope-mesh PUBLIC scope-world)

# A picture's shapes as triangles, for painters that draw triangles: this one, and scopegpu's.
add_library(scope-tessellation STATIC)
target_sources(scope-tessellation PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Tessellation.ixx
)
target_link_libraries(scope-tessellation PUBLIC scope-picture scope-mesh)

# How the glow's drawn: the size of the textures it's blurred in, and the blur.
add_library(scope-glow STATIC)
target_sources(scope-glow PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Glow.ixx
)
target_link_libraries(scope-glow PUBLIC scope-mesh)

# Draws a picture's text with SDL's renderer and SDL_ttf: this painter's, and scopegpu2d's.
add_library(scope-typesetter STATIC)
target_sources(scope-typesetter PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Typesetter.ixx
)
target_link_libraries(scope-typesetter PUBLIC scope-picture scope-mesh SDL3_ttf::SDL3_ttf SDL3::SDL3)

# Draws the picture with SDL's renderer, and its text with SDL_ttf.
add_library(scope-painter STATIC)
target_sources(scope-painter PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Painter.ixx
)
target_link_libraries(scope-painter PUBLIC scope-glow scope-tessellation scope-typesetter SDL3::SDL3)

add_executable(scope main.cpp)
target_link_libraries(scope PRIVATE scope-painter aspire-sdl SDL3::SDL3)

# The readout font is read from next to the executable, with its licence beside it.
set(SCOPE_FONT_DIR "${CMAKE_SOURCE_DIR}/assets/b612")
set(SCOPE_FONT_FILES
    "${SCOPE_FONT_DIR}/B612Mono-Regular.ttf"
    "${SCOPE_FONT_DIR}/OFL.txt"
)

if(EMSCRIPTEN)
    # On the web, the font and its licence are preloaded into the package, under /fonts.
    set_target_properties(scope PROPERTIES SUFFIX ".html")
    target_link_options(scope PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file=${SCOPE_FONT_DIR}/B612Mono-Regular.ttf@/fonts/B612Mono-Regular.ttf"
        "--preload-file=${SCOPE_FONT_DIR}/OFL.txt@/fonts/OFL.txt"
    )
    install(FILES
        "$<TARGET_FILE:scope>"
        "$<TARGET_FILE_DIR:scope>/scope.js"
        "$<TARGET_FILE_DIR:scope>/scope.wasm"
        "$<TARGET_FILE_DIR:scope>/scope.data"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/scope"
    )
else()
    add_custom_command(TARGET scope POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:scope>/fonts"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SCOPE_FONT_FILES} "$<TARGET_FILE_DIR:scope>/fonts"
        VERBATIM
    )
    install(TARGETS scope RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES ${SCOPE_FONT_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}/fonts")
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(scope-world-test test/World.test.cpp)
    target_link_libraries(scope-world-test PRIVATE scope-world GTest::gtest GTest::gtest_main)
    gtest_discover_tests(scope-world-test)
    add_executable(scope-mesh-test test/Mesh.test.cpp)
    target_link_libraries(scope-mesh-test PRIVATE scope-mesh GTest::gtest GTest::gtest_main)
    gtest_discover_tests(scope-mesh-test)
    add_executable(scope-tessellation-test test/Tessellation.test.cpp)
    target_link_libraries(scope-tessellation-test PRIVATE scope-tessellation GTest::gtest GTest::gtest_main)
    gtest_discover_tests(scope-tessellation-test)
    add_test(NAME scope-smoke COMMAND scope --frames=3)
    set_tests_properties(scope-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 10
    )
endif()
