project(aspire-core)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Overloaded.ixx
        Engine.ixx
        Event.ixx
        Module.ixx
        Object.ixx
        ObjectFactory.ixx
        Property.ixx
        Vec2.ixx
)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(nameof CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    nlohmann_json::nlohmann_json
    nameof::nameof
)

add_subdirectory(test)