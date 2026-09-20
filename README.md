# aspire
Personal integration and rendering engine.

## SDL3 data-oriented example

[`app/evford`](app/evford/README.md) renders an animated particle simulation using
separate arrays for positions and velocities, with SDL3 handling rendering and
application lifecycle. It includes build paths for Windows, Linux, macOS,
Android, and WebAssembly.

Presets are named `<architecture>-<platform>-<compiler>-<configuration>`.
With CMake 4.4.3+, Ninja, a C++17 compiler, and the vcpkg submodule bootstrapped,
choose the preset for your platform. For example, on an ARM64 Mac:

```sh
cmake --preset arm64-macos-clang-debug
cmake --build --preset arm64-macos-clang-debug
ctest --preset arm64-macos-clang-debug
cmake --install build/arm64-macos-clang-debug
```

Use `x64-windows-msvc-debug` on Windows or `x64-linux-clang-debug` on Linux.
Run `build/<preset>/installed/bin/evford` (`evford.exe` on Windows).
Space, click, or tap pauses; R resets; Escape exits.

All presets build `evford`; SDL3 is the only application dependency in the vcpkg
manifest. The legacy `src/` libraries and `srd-lite` app remain in the repository
but are disabled in the build. See the
[example README](app/evford/README.md) for platform prerequisites and web builds,
and the [Android project](app/evford/android/README.md) for APK builds.

## Installing on Windows

Configure, build, and install using the same preset:

```powershell
cmake --preset x64-windows-clang-debug
cmake --build --preset x64-windows-clang-debug
cmake --install build/x64-windows-clang-debug
& ./build/x64-windows-clang-debug/installed/bin/evford.exe
```

The Windows presets enable vcpkg's experimental `X_VCPKG_APPLOCAL_DEPS_INSTALL`
option so installation copies required vcpkg DLLs beside the executable in
`installed/bin`. The default `VCPKG_APPLOCAL_DEPS` option only copies them beside
the build output. Reconfigure existing build directories with their preset before
installing to pick up this setting.

## Static analysis

The `clang-tidy` and `clang-tidy-diff` build targets are available when the C++
compiler is Clang. Configure with a Clang preset, then run either target:

```powershell
cmake --preset x64-windows-clang-debug
cmake --build --preset x64-windows-clang-debug --target clang-tidy
cmake --build --preset x64-windows-clang-debug --target clang-tidy-diff
```

```sh
cmake --preset x64-linux-clang-debug
cmake --build --preset x64-linux-clang-debug --target clang-tidy
cmake --build --preset x64-linux-clang-debug --target clang-tidy-diff
```

Release builds use `x64-windows-clang-release` or `x64-linux-clang-release`.
You can also use `cmake --build <clang-build-directory> --target clang-tidy`.
MSVC and GCC builds do not expose these targets.

Install Clang and clang-tidy from the same LLVM release, their `run-clang-tidy`
and `clang-tidy-diff.py` scripts, Python 3, Git, and Ninja. The Windows
Clang presets use clang-cl with the MSVC STL and Windows SDK configured by the
existing MSVC preset. The app requires C++17 and does not use `import std`.

Both targets use LLVM's Python runners in parallel and build project dependencies
first. `clang-tidy` checks compiled C/C++ sources under `app/evford/`, including
tests. Disabled legacy sources, build output, and vendored sources are excluded.

`clang-tidy-diff` reports findings on changed lines in those files, comparing the
working tree (including staged changes) against `origin/main`, or local `main`
when `origin/main` is unavailable at configuration time. Untracked files and
standalone headers are outside this diff check. LLVM still analyzes each changed
file in full. Its diff parser cannot handle repository-relative filenames with
spaces; use the full target for such files.

Both targets use `.clang-tidy`, propagate failures, and do not apply fixes.
Parallelism defaults to the detected CPU count. Configure a job limit or another
comparison base as needed:

```powershell
cmake --preset x64-windows-clang-debug -DASPIRE_CLANG_TIDY_JOBS=4 -DASPIRE_CLANG_TIDY_DIFF_BASE=main
```

Tools are discovered automatically, including LLVM's Windows `bin` and
`share/clang` locations. Overrides are `ASPIRE_CLANG_TIDY_EXECUTABLE`,
`ASPIRE_RUN_CLANG_TIDY_SCRIPT`, `ASPIRE_CLANG_TIDY_DIFF_SCRIPT`, and
`Python3_EXECUTABLE`.
