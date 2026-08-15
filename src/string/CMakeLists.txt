project(aspire-string)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Concepts.ixx
        ErrorCode.ixx
        FromString.ixx
        ToString.ixx
        String.ixx
)

add_subdirectory(test)