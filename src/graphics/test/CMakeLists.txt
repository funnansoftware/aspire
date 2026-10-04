project(aspire-graphics-test)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    Color.test.cpp
    Node.test.cpp
    Rect.test.cpp
    Transform.test.cpp
)

enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

target_link_libraries(${PROJECT_NAME} PRIVATE
    aspire-graphics
    aspire-parser
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(${PROJECT_NAME})
