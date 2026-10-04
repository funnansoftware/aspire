if(ANDROID)
    # srd-lite targets desktop and the web.
    return()
endif()

find_package(SDL3 3.2 CONFIG REQUIRED)

# The game's own code, as a library so the tests can link it.
add_library(srd-lite-game STATIC)
target_sources(srd-lite-game PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Character.ixx
        Level.ixx
        ViewWorld.ixx
)
target_link_libraries(srd-lite-game PUBLIC aspire-core aspire-graphics)

add_executable(srd-lite main.cpp)
target_link_libraries(srd-lite PRIVATE srd-lite-game aspire-parser aspire-sdl SDL3::SDL3)

# The game reads its data, and the one tilesheet it draws, from next to the executable.
set(SRD_LITE_SHEET_DIR "assets/kenney/1-bit-pack/Tilesheet")
set(SRD_LITE_SHEET "${CMAKE_SOURCE_DIR}/${SRD_LITE_SHEET_DIR}/colored-transparent.png")

if(EMSCRIPTEN)
    # On the web, the same layout is preloaded into the package, at the root.
    set_target_properties(srd-lite PROPERTIES SUFFIX ".html")
    target_link_options(srd-lite PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/config@/config"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/database@/database"
        "--preload-file=${SRD_LITE_SHEET}@/${SRD_LITE_SHEET_DIR}/colored-transparent.png"
    )
    install(FILES
        "$<TARGET_FILE:srd-lite>"
        "$<TARGET_FILE_DIR:srd-lite>/srd-lite.js"
        "$<TARGET_FILE_DIR:srd-lite>/srd-lite.wasm"
        "$<TARGET_FILE_DIR:srd-lite>/srd-lite.data"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/srd-lite"
    )
else()
    add_custom_command(TARGET srd-lite POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:srd-lite>/config"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/database" "$<TARGET_FILE_DIR:srd-lite>/database"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:srd-lite>/${SRD_LITE_SHEET_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRD_LITE_SHEET}" "$<TARGET_FILE_DIR:srd-lite>/${SRD_LITE_SHEET_DIR}"
        VERBATIM
    )
    install(TARGETS srd-lite RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY config database DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${SRD_LITE_SHEET}" DESTINATION "${CMAKE_INSTALL_BINDIR}/${SRD_LITE_SHEET_DIR}")
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(srd-lite-test test/Level.test.cpp)
    target_link_libraries(srd-lite-test PRIVATE srd-lite-game GTest::gtest GTest::gtest_main)
    gtest_discover_tests(srd-lite-test)
    add_test(NAME srd-lite-smoke COMMAND srd-lite --frames=3)
    set_tests_properties(srd-lite-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 10
    )
endif()
