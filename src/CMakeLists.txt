add_subdirectory(core)
add_subdirectory(parser)
add_subdirectory(string)

project(aspire)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Module.ixx
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    aspire-parser
    aspire-core
    aspire-string
)