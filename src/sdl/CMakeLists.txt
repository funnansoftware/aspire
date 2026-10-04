project(aspire-sdl)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Event.ixx
        Module.ixx
        RenderBackend.ixx
)

find_package(SDL3 3.2 CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    aspire-core
    aspire-graphics
    SDL3::SDL3
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
