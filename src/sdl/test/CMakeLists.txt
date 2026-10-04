project(aspire-sdl-test)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    Event.test.cpp
    RenderBackend.test.cpp
)

enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

target_link_libraries(${PROJECT_NAME} PRIVATE
    aspire-sdl
    GTest::gtest
    GTest::gtest_main
)

# The window tests need no display: SDL's dummy video driver and software renderer work headless.
gtest_discover_tests(${PROJECT_NAME}
    PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
)
