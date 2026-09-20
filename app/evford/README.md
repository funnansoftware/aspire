# evford

An asset-free SDL3 example for Windows, Linux, macOS, Android, and WebAssembly.
1,024 moving particles are stored in separate, contiguous arrays of positions
and velocities. The simulation updates those arrays in a linear pass; the
renderer gathers rectangles into four color batches. There are no per-particle
objects, virtual calls, or allocations in the frame loop.

- Space, left click, or tap: pause/resume.
- R: reset to the same deterministic initial state.
- Escape: quit.

`world.hpp` and `world.cpp` contain the simulation without SDL dependencies.
`main.cpp` owns SDL resources and implements `SDL_AppInit`, `SDL_AppEvent`,
`SDL_AppIterate`, and `SDL_AppQuit`. These callbacks give the browser and Android
control of the event loop. Rendering uses a letterboxed 960x540 logical canvas.
No game assets or external fonts are needed.

This is the active app in the repository. The legacy framework and `srd-lite`
sources are retained but disabled. SDL3 is the only application dependency.
The app uses C++17 headers so NDK, Emscripten, and Apple Clang can build it
without `import std` support.

## Windows, Linux, and macOS

Install CMake 4.4.3 or newer, Ninja, and a C++17 compiler. On Windows, use an MSVC
developer terminal. On macOS, install the Xcode command-line tools. On Debian or
Ubuntu, SDL3's default vcpkg features also need the development packages below:

```sh
sudo apt install build-essential ninja-build pkg-config libx11-dev libxft-dev \
    libxext-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev libibus-1.0-dev
```

Initialize and bootstrap the repository's pinned vcpkg checkout once:

```sh
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh
```

Use `./vcpkg/bootstrap-vcpkg.bat` on Windows. Then, from the repository root:

```sh
cmake --preset evford-native
cmake --build --preset evford-native
ctest --preset evford-native
cmake --install build/evford-native
```

Launch `build/evford-native/installed/bin/evford` (append `.exe` on Windows).
The Windows build and install copy SDL3's DLL beside the executable.

The platform-specific desktop presets also build `evford`. For example:

```sh
cmake --preset arm64-macos-clang-debug
cmake --build --preset arm64-macos-clang-debug --target evford
./build/arm64-macos-clang-debug/app/evford/evford
```

All presets use the same SDL3-only manifest. No framework feature or toggle is
needed.

## WebAssembly

Install and activate a current stable [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).
Run `source /path/to/emsdk/emsdk_env.sh` (or `emsdk_env.bat` on Windows) in the
build terminal. `EMSDK` and the SDK tools must be available to CMake and vcpkg.
From the repository root:

```sh
cmake --preset evford-web
cmake --build --preset evford-web
cmake --install build/evford-web
python3 -m http.server 8000 --directory build/evford-web/installed/share/evford
```

Open [localhost:8000/evford.html](http://localhost:8000/evford.html). Deploy
`evford.html`, `evford.js`, and `evford.wasm` together, using an HTTP server;
opening the HTML as a local file will not load WebAssembly correctly.
The build uses SDL3 from vcpkg and SDL's browser callbacks. It needs neither
Asyncify nor pthreads, and does not require cross-origin isolation headers.
The existing Linux Emscripten presets also select this portable build path.

## Android

The [Gradle project](android/README.md) builds SDL3 through the repository's
vcpkg manifest, including its Java activity support and native shared libraries.
The native target becomes `libmain.so` and uses the
same C++ sources. Follow that README for SDK requirements and build/install
commands. The root Android NDK presets can also invoke `apk-debug` or
`apk-release`; Gradle owns packaging and the release APK is unsigned.

## Validation and standalone use

`ctest --preset evford-native` runs deterministic simulation checks and a short
SDL smoke test with the dummy video/software renderer. For a visible smoke test:

```sh
./build/evford-native/app/evford/evford --frames=120
```

Verified on macOS with Apple Clang 17 and LLVM 21, on Android with debug/release
APK builds and an ARM64 emulator, and in Chrome with Emscripten 6.0.3. Windows
and Linux build paths are provided but have not been executed in this checkout.

Tests are omitted from cross builds. A native build can also configure this
directory directly with CMake 3.22+ and the same vcpkg manifest (from the
repository root, after bootstrapping vcpkg):

```sh
cmake -S app/evford -B build/evford-standalone -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_DIR="$PWD"
cmake --build build/evford-standalone
ctest --test-dir build/evford-standalone --output-on-failure
```

SDL references: [main callbacks](https://wiki.libsdl.org/SDL3/README-main-functions),
[Emscripten](https://wiki.libsdl.org/SDL3/README-emscripten),
[Android](https://wiki.libsdl.org/SDL3/README-android).
