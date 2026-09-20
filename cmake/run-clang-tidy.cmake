include("${CMAKE_CURRENT_LIST_DIR}/clang-tidy-common.cmake")

# LLVM matches absolute native paths, including backslashes on Windows.
# Select the validated files exactly, retaining the guard against an empty run.
set(_patterns)
foreach(_file IN LISTS _sources)
    string(REGEX QUOTE _pattern "${_file}")
    string(REPLACE "/" "[/\\\\]" _pattern "${_pattern}")
    list(APPEND _patterns "${_pattern}")
endforeach()
list(JOIN _patterns "|" _source_filter)
set(_source_filter "^(${_source_filter})$")
if(WIN32)
    string(PREPEND _source_filter "(?i)")
endif()

message(STATUS "Running LLVM run-clang-tidy on ${_source_count} files with ${ASPIRE_TIDY_JOBS} parallel jobs")
execute_process(
    COMMAND "${ASPIRE_TIDY_PYTHON}" -X utf8 "${ASPIRE_TIDY_SCRIPT}"
        "-clang-tidy-binary=${ASPIRE_TIDY_EXECUTABLE}"
        -p "${_binary_root}" -j "${ASPIRE_TIDY_JOBS}"
        "-source-filter=${_source_filter}"
        "-config-file=${_source_root}/.clang-tidy"
        "-exclude-header-filter=${_excluded_headers}" -quiet
    WORKING_DIRECTORY "${_source_root}"
    RESULT_VARIABLE _result
)
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "LLVM run-clang-tidy failed (${_result}).")
endif()
