project(aspire-core-test)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    Engine.test.cpp
    Object.test.cpp
    ObjectFactory.test.cpp
    Property.test.cpp
    Vec2.test.cpp
)

enable_testing()
    
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

target_link_libraries(${PROJECT_NAME} PRIVATE
    aspire-core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(${PROJECT_NAME})