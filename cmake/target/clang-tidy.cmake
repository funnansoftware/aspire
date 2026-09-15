include_guard(GLOBAL)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    return()
endif()

# Include after all project targets. Both LLVM scripts use Clang-built modules
# from this build's compilation database.
include(ProcessorCount)
ProcessorCount(_aspire_tidy_processors)
if(_aspire_tidy_processors LESS 1)
    set(_aspire_tidy_processors 1)
endif()
set(ASPIRE_CLANG_TIDY_JOBS "${_aspire_tidy_processors}" CACHE STRING
    "Number of clang-tidy processes to run in parallel")

get_filename_component(_aspire_clang_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
string(REGEX MATCH "^[0-9]+" _aspire_clang_major "${CMAKE_CXX_COMPILER_VERSION}")
set(_aspire_tidy_error "")
if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    set(_aspire_tidy_error "clang-tidy requires a single-configuration Ninja build with compile_commands.json.")
else()
    if(DEFINED ASPIRE_CLANG_TIDY_EXECUTABLE AND "${ASPIRE_CLANG_TIDY_EXECUTABLE}" STREQUAL "")
        unset(ASPIRE_CLANG_TIDY_EXECUTABLE CACHE)
        unset(ASPIRE_CLANG_TIDY_EXECUTABLE)
    endif()
    find_program(ASPIRE_CLANG_TIDY_EXECUTABLE
        NAMES "clang-tidy-${_aspire_clang_major}" clang-tidy
        NAMES_PER_DIR
        HINTS "${_aspire_clang_dir}"
        DOC "clang-tidy from the same LLVM release as the C++ compiler"
    )

    if(NOT ASPIRE_CLANG_TIDY_EXECUTABLE)
        set(_aspire_tidy_error
            "clang-tidy was not found. Install it alongside Clang and reconfigure, or set ASPIRE_CLANG_TIDY_EXECUTABLE.")
    else()
        execute_process(
            COMMAND "${ASPIRE_CLANG_TIDY_EXECUTABLE}" --version
            OUTPUT_VARIABLE _aspire_tidy_version_output
            ERROR_VARIABLE _aspire_tidy_version_error
            RESULT_VARIABLE _aspire_tidy_version_result
        )
        string(REGEX MATCH "[Vv]ersion ([0-9]+\\.[0-9]+\\.[0-9]+)"
            _aspire_tidy_version_match "${_aspire_tidy_version_output}\n${_aspire_tidy_version_error}")
        set(_aspire_tidy_version "${CMAKE_MATCH_1}")
        if(NOT _aspire_tidy_version_result EQUAL 0 OR NOT _aspire_tidy_version_match)
            set(_aspire_tidy_error "Unable to read the version of ${ASPIRE_CLANG_TIDY_EXECUTABLE}.")
        elseif(NOT _aspire_tidy_version VERSION_EQUAL CMAKE_CXX_COMPILER_VERSION)
            set(_aspire_tidy_error
                "clang-tidy ${_aspire_tidy_version} does not match Clang ${CMAKE_CXX_COMPILER_VERSION}. Set ASPIRE_CLANG_TIDY_EXECUTABLE to the matching release and reconfigure.")
        endif()
    endif()
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(NOT _aspire_tidy_error AND NOT Python3_Interpreter_FOUND)
    set(_aspire_tidy_error "The LLVM clang-tidy scripts require Python 3. Install it and reconfigure, or set Python3_EXECUTABLE.")
endif()
if(NOT _aspire_tidy_error AND NOT ASPIRE_CLANG_TIDY_JOBS MATCHES "^[1-9][0-9]*$")
    set(_aspire_tidy_error "ASPIRE_CLANG_TIDY_JOBS must be a positive integer.")
endif()

set(_aspire_script_hints "${_aspire_clang_dir}")
if(ASPIRE_CLANG_TIDY_EXECUTABLE)
    get_filename_component(_aspire_tidy_dir "${ASPIRE_CLANG_TIDY_EXECUTABLE}" DIRECTORY)
    list(PREPEND _aspire_script_hints "${_aspire_tidy_dir}")
endif()
foreach(_bin_dir IN LISTS _aspire_script_hints)
    get_filename_component(_llvm_prefix "${_bin_dir}" DIRECTORY)
    list(APPEND _aspire_script_hints "${_llvm_prefix}/share/clang")
endforeach()
list(REMOVE_DUPLICATES _aspire_script_hints)
foreach(_script_variable IN ITEMS ASPIRE_RUN_CLANG_TIDY_SCRIPT ASPIRE_CLANG_TIDY_DIFF_SCRIPT)
    if(DEFINED ${_script_variable} AND "${${_script_variable}}" STREQUAL "")
        unset(${_script_variable} CACHE)
        unset(${_script_variable})
    endif()
endforeach()
find_file(ASPIRE_RUN_CLANG_TIDY_SCRIPT
    NAMES "run-clang-tidy-${_aspire_clang_major}" "run-clang-tidy-${_aspire_clang_major}.py"
        run-clang-tidy run-clang-tidy.py
    HINTS ${_aspire_script_hints} ENV PATH
    DOC "LLVM run-clang-tidy Python script"
)
find_file(ASPIRE_CLANG_TIDY_DIFF_SCRIPT
    NAMES "clang-tidy-diff-${_aspire_clang_major}.py" "clang-tidy-diff-${_aspire_clang_major}"
        clang-tidy-diff.py clang-tidy-diff
    HINTS ${_aspire_script_hints} ENV PATH
    DOC "LLVM clang-tidy-diff Python script"
)

set(_aspire_tidy_full_error "${_aspire_tidy_error}")
if(NOT _aspire_tidy_full_error AND
    (NOT EXISTS "${ASPIRE_RUN_CLANG_TIDY_SCRIPT}" OR IS_DIRECTORY "${ASPIRE_RUN_CLANG_TIDY_SCRIPT}"))
    set(_aspire_tidy_full_error "LLVM run-clang-tidy was not found. Install the LLVM Python scripts, or set ASPIRE_RUN_CLANG_TIDY_SCRIPT and reconfigure.")
endif()
set(_aspire_tidy_diff_error "${_aspire_tidy_error}")
if(NOT _aspire_tidy_diff_error AND
    (NOT EXISTS "${ASPIRE_CLANG_TIDY_DIFF_SCRIPT}" OR IS_DIRECTORY "${ASPIRE_CLANG_TIDY_DIFF_SCRIPT}"))
    set(_aspire_tidy_diff_error "LLVM clang-tidy-diff was not found. Install the LLVM Python scripts, or set ASPIRE_CLANG_TIDY_DIFF_SCRIPT and reconfigure.")
endif()
find_package(Git QUIET)
if(NOT _aspire_tidy_diff_error AND NOT Git_FOUND)
    set(_aspire_tidy_diff_error "clang-tidy-diff requires Git. Install it and reconfigure, or set GIT_EXECUTABLE.")
endif()
if(NOT DEFINED ASPIRE_CLANG_TIDY_DIFF_BASE)
    set(_aspire_tidy_default_base main)
    if(Git_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify --end-of-options "origin/main^{commit}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _aspire_tidy_origin_result OUTPUT_QUIET ERROR_QUIET)
        if(_aspire_tidy_origin_result EQUAL 0)
            set(_aspire_tidy_default_base origin/main)
        endif()
    endif()
endif()
set(ASPIRE_CLANG_TIDY_DIFF_BASE "${_aspire_tidy_default_base}" CACHE STRING
    "Git base revision for clang-tidy-diff (origin/main when available, otherwise main)")

add_custom_target(clang-tidy
    COMMAND "${CMAKE_COMMAND}"
        "-DASPIRE_TIDY_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DASPIRE_TIDY_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DASPIRE_TIDY_EXECUTABLE=${ASPIRE_CLANG_TIDY_EXECUTABLE}"
        "-DASPIRE_TIDY_PYTHON=${Python3_EXECUTABLE}"
        "-DASPIRE_TIDY_SCRIPT=${ASPIRE_RUN_CLANG_TIDY_SCRIPT}"
        "-DASPIRE_TIDY_JOBS=${ASPIRE_CLANG_TIDY_JOBS}"
        "-DASPIRE_TIDY_ERROR=${_aspire_tidy_full_error}"
        -P "${CMAKE_CURRENT_LIST_DIR}/run-clang-tidy.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking Aspire sources with LLVM run-clang-tidy"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(clang-tidy-diff
    COMMAND "${CMAKE_COMMAND}"
        "-DASPIRE_TIDY_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DASPIRE_TIDY_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DASPIRE_TIDY_EXECUTABLE=${ASPIRE_CLANG_TIDY_EXECUTABLE}"
        "-DASPIRE_TIDY_PYTHON=${Python3_EXECUTABLE}"
        "-DASPIRE_TIDY_SCRIPT=${ASPIRE_CLANG_TIDY_DIFF_SCRIPT}"
        "-DASPIRE_TIDY_JOBS=${ASPIRE_CLANG_TIDY_JOBS}"
        "-DASPIRE_TIDY_ERROR=${_aspire_tidy_diff_error}"
        "-DASPIRE_TIDY_DIFF_BASE=${ASPIRE_CLANG_TIDY_DIFF_BASE}"
        "-DASPIRE_TIDY_GIT=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_LIST_DIR}/run-clang-tidy-diff.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking changed Aspire lines with LLVM clang-tidy-diff"
    USES_TERMINAL
    VERBATIM
)

# Include tests and applications so every compilation database entry has its
# generated module dependency maps and imported modules available before lint.
function(_aspire_tidy_collect_targets directory output)
    get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_build_targets "")
    foreach(_target IN LISTS _targets)
        get_target_property(_type "${_target}" TYPE)
        if(_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            list(APPEND _build_targets "${_target}")
        endif()
    endforeach()
    get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_subdirectory IN LISTS _subdirectories)
        _aspire_tidy_collect_targets("${_subdirectory}" _subtargets)
        list(APPEND _build_targets ${_subtargets})
    endforeach()
    set("${output}" "${_build_targets}" PARENT_SCOPE)
endfunction()

_aspire_tidy_collect_targets("${CMAKE_SOURCE_DIR}" _aspire_tidy_targets)
if(_aspire_tidy_full_error)
    message(STATUS "clang-tidy: ${_aspire_tidy_full_error}")
elseif(_aspire_tidy_targets)
    add_dependencies(clang-tidy ${_aspire_tidy_targets})
endif()
if(_aspire_tidy_diff_error)
    message(STATUS "clang-tidy-diff: ${_aspire_tidy_diff_error}")
elseif(_aspire_tidy_targets)
    add_dependencies(clang-tidy-diff ${_aspire_tidy_targets})
endif()
