project(aspire-core)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Engine.ixx
        Module.ixx
        Object.ixx
        Property.ixx
)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    nlohmann_json::nlohmann_json
)

add_subdirectory(test)