include("${CMAKE_CURRENT_LIST_DIR}/clang-tidy-common.cmake")

if(NOT ASPIRE_TIDY_GIT OR NOT EXISTS "${ASPIRE_TIDY_GIT}")
    message(FATAL_ERROR "clang-tidy-diff requires Git. Install it and reconfigure.")
endif()
if(NOT ASPIRE_TIDY_DIFF_BASE)
    message(FATAL_ERROR "Set ASPIRE_CLANG_TIDY_DIFF_BASE to the branch or commit to compare against.")
endif()
execute_process(
    COMMAND "${ASPIRE_TIDY_GIT}" rev-parse --verify --end-of-options "${ASPIRE_TIDY_DIFF_BASE}^{commit}"
    WORKING_DIRECTORY "${_source_root}"
    OUTPUT_VARIABLE _base_commit OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE _git_error RESULT_VARIABLE _result
)
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "Cannot resolve clang-tidy-diff base '${ASPIRE_TIDY_DIFF_BASE}'. Set ASPIRE_CLANG_TIDY_DIFF_BASE to an existing branch or commit.\n${_git_error}")
endif()

set(_pathspecs)
set(_patterns)
foreach(_file IN LISTS _sources)
    file(RELATIVE_PATH _relative "${_source_root}" "${_file}")
    list(APPEND _pathspecs ":(literal)${_relative}")
    set(_diff_relative "${_relative}")
    if(WIN32)
        string(REPLACE "/" "\\" _diff_relative "${_diff_relative}")
    endif()
    string(REGEX QUOTE _pattern "${_diff_relative}")
    list(APPEND _patterns "${_pattern}")
    # LLVM's diff parser truncates filenames at whitespace or double quotes.
    # Do not let affected changed files silently turn into a successful no-op.
    if(_relative MATCHES "[ \t\r\n\"]")
        execute_process(
            COMMAND "${ASPIRE_TIDY_GIT}" diff --quiet --no-ext-diff --no-textconv
                "${_base_commit}" -- ":(literal)${_relative}"
            WORKING_DIRECTORY "${_source_root}" RESULT_VARIABLE _changed
        )
        if(_changed STREQUAL "1")
            message(FATAL_ERROR "LLVM clang-tidy-diff cannot parse the changed filename '${_relative}' because it contains whitespace or quotes. Use the full clang-tidy target for this file.")
        elseif(NOT _changed STREQUAL "0")
            message(FATAL_ERROR "Git could not check changes to '${_relative}' (${_changed}).")
        endif()
    endif()
endforeach()
list(JOIN _patterns "|" _source_filter)

# Use a file for stdin instead of a platform-specific shell pipeline. Pin the
# patch format independently of the user's Git configuration.
set(_patch "${_binary_root}/clang-tidy.diff")
execute_process(
    COMMAND "${ASPIRE_TIDY_GIT}" -c core.quotePath=false diff
        --no-ext-diff --no-textconv --no-color --unified=0 --inter-hunk-context=0
        --no-renames --relative --src-prefix=a/ --dst-prefix=b/
        "${_base_commit}" -- ${_pathspecs}
    WORKING_DIRECTORY "${_source_root}"
    OUTPUT_FILE "${_patch}"
    ERROR_VARIABLE _git_error RESULT_VARIABLE _result
)
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "Unable to generate the clang-tidy diff (${_result}).\n${_git_error}")
endif()

if(WIN32)
    # Clang's line filter compares native filename suffixes. LLVM's diff script
    # otherwise forwards Git's '/' paths unchanged and hides Windows warnings.
    # Keep the b/ prefix for -p1 and convert only the filename in +++ headers.
    file(READ "${_patch}" _patch_text)
    foreach(_file IN LISTS _sources)
        file(RELATIVE_PATH _relative "${_source_root}" "${_file}")
        string(REPLACE "/" "\\" _native_relative "${_relative}")
        string(REPLACE "\n+++ b/${_relative}\n" "\n+++ b/${_native_relative}\n"
            _patch_text "${_patch_text}")
    endforeach()
    file(WRITE "${_patch}" "${_patch_text}")
endif()

message(STATUS "Running LLVM clang-tidy-diff against ${ASPIRE_TIDY_DIFF_BASE} with ${ASPIRE_TIDY_JOBS} parallel jobs")
execute_process(
    COMMAND "${ASPIRE_TIDY_PYTHON}" -X utf8 "${ASPIRE_TIDY_SCRIPT}"
        "-clang-tidy-binary=${ASPIRE_TIDY_EXECUTABLE}"
        -p1 -path "${_binary_root}" -j "${ASPIRE_TIDY_JOBS}"
        "-regex=(${_source_filter})"
        "-config-file=${_source_root}/.clang-tidy"
    WORKING_DIRECTORY "${_source_root}"
    INPUT_FILE "${_patch}" RESULT_VARIABLE _result
)
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "LLVM clang-tidy-diff failed (${_result}).")
endif()
