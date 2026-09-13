project(aspire-raylib)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Module.ixx
        Window.ixx
)

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    raylib
    glfw
)

add_subdirectory(test)