# Gradle owns the package, including SDL's Java activity and native libraries.
# The NDK CMake presets can invoke the same Android build.
if(NOT ANDROID)
    return()
endif()

set(_evford_android_dir "${CMAKE_SOURCE_DIR}/app/evford/android")
if(CMAKE_HOST_WIN32)
    set(_evford_gradle "${_evford_android_dir}/gradlew.bat")
else()
    set(_evford_gradle "${_evford_android_dir}/gradlew")
endif()

add_custom_target(apk-debug
    COMMAND "${_evford_gradle}" -p "${_evford_android_dir}" assembleDebug
    WORKING_DIRECTORY "${_evford_android_dir}"
    COMMENT "Building the evford debug APK"
    USES_TERMINAL
    VERBATIM
)
add_custom_target(apk-release
    COMMAND "${_evford_gradle}" -p "${_evford_android_dir}" assembleRelease
    WORKING_DIRECTORY "${_evford_android_dir}"
    COMMENT "Building the unsigned evford release APK"
    USES_TERMINAL
    VERBATIM
)
add_custom_target(apk DEPENDS apk-debug apk-release)
