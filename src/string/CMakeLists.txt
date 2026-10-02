project(aspire-string)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Concepts.ixx
        ErrorCode.ixx
        FromString.ixx
        Module.ixx
        ToString.ixx
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()