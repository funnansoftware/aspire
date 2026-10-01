include_guard(GLOBAL)

find_program(CLANG_FORMAT_EXECUTABLE clang-format QUIET)

if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE _aspire_format_sources CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/app/*
        ${CMAKE_SOURCE_DIR}/src/*
    )

    list(FILTER _aspire_format_sources INCLUDE REGEX ".*\\.(cpp|h|hpp|ixx)$")

    add_custom_target(clang-format-check
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --dry-run -Werror ${_aspire_format_sources}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking format of source files..."
        VERBATIM
    )

    add_custom_target(clang-format
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" -i ${_aspire_format_sources}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting source files..."
        VERBATIM
    )
else()
    message(WARNING "clang-format executable not found - clang-format targets will be skipped.")
endif()
