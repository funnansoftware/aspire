# Evford on Android

This Gradle project packages the same C++ example as the desktop and WebAssembly
builds. Its CMake entry point adds `app/evford`, builds `evford` as
`libmain.so`, and launches it through `EvfordActivity`, a small `SDLActivity`
subclass. The default APK includes `arm64-v8a` and `x86_64` libraries and runs on
Android 5.0 (API 21) or newer.

## Prerequisites

- JDK 17.
- Android SDK platform 36, build tools 36.0.0, and platform tools.
- Android NDK 28.2.13676358 and SDK CMake 3.22.1.
- The repository's initialized and bootstrapped vcpkg submodule.
- Network access on the first build for Gradle, its plugins, and SDL.

Install the Android packages with Android Studio's SDK Manager, or use:

```sh
sdkmanager "platforms;android-36" "build-tools;36.0.0" "platform-tools" \
  "ndk;28.2.13676358" "cmake;3.22.1"
```

Set `ANDROID_HOME` to your SDK directory, or add `sdk.dir=/absolute/path/to/sdk`
to an untracked `local.properties` in this directory. Set `JAVA_HOME` to JDK 17
if another Java version is the system default.

## Build and run

From the repository root:

```sh
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh
cd app/evford/android
./gradlew assembleDebug
./gradlew installDebug
adb shell am start -n org.aspire.evford/.EvfordActivity
```

Use `bootstrap-vcpkg.bat` and `gradlew.bat` on Windows. `installDebug` requires an emulator or a device
connected with USB debugging enabled. The APK is written to
`app/build/outputs/apk/debug/app-debug.apk`. You can also open this directory in
Android Studio and run the `app` configuration. `assembleRelease` creates an
unsigned release APK; configure signing before distributing it.

## SDL dependency

The `installSdl` task invokes the repository's vcpkg executable and SDL3-only
root manifest. SDL's version, source checksum, patches,
and binary cache are managed by the pinned vcpkg checkout, just as on desktop
and WebAssembly. There is no separate SDL archive download or SDL version pin
in Gradle.

The overlay triplets in `cmake/triplets` build shared libraries for
`arm64-v8a` and `x86_64` at API 21, and enable SDL's Java JAR. Gradle passes its
selected SDK, NDK, and JDK to vcpkg. Each triplet has a separate installation
root under `app/build/vcpkg_installed`, since manifest installs remove
unrequested triplets. The generated Java JAR is copied to
`app/build/dependencies/SDL3.jar` for Java compilation. CMake resolves
`SDL3::SDL3` from the matching triplet, and Gradle packages the linked shared
libraries in the APK. Both Debug and Release use their corresponding vcpkg
libraries.

The task runs before Java compilation and native configuration; repeated builds
let vcpkg check its cache. The app does not need the framework's C++ module
toolchain. SDL's Java and native integration is described in its
[Android documentation](https://wiki.libsdl.org/SDL3/README-android).

## Validation

The debug and unsigned release APKs were built for both configured ABIs. The
debug APK was also installed and visually checked on an arm64 Android API 34
emulator, including touch pause and background/foreground transitions. Physical
devices and the x86_64 emulator have not been run yet.
