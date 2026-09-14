project(aspire-parser)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Json.ixx
        Module.ixx
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    aspire-core
)

add_subdirectory(test)