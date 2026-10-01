# aspire

Personal integration and rendering engine.

## Table of contents

- [Prerequisites](#prerequisites)
- [Getting the source](#getting-the-source)
- [Building](#building)
  - [Choosing a preset](#choosing-a-preset)
  - [Windows](#windows)
  - [Linux](#linux)
  - [macOS](#macos)
  - [SteamOS](#steamos)
  - [WebAssembly](#webassembly)
- [Troubleshooting](#troubleshooting)

## Prerequisites

Install these tools first. Each one needs to be available from your terminal
(on your `PATH`); the installers below normally do this for you.

| Tool | Version | Purpose |
| --- | --- | --- |
| [Git](https://git-scm.com/downloads) | any recent | Downloads the project and its submodules. |
| [CMake](https://cmake.org/download/) | 4.4.2 or newer | Configures the build. |
| [Ninja](https://github.com/ninja-build/ninja/releases) | any recent | Runs the build. |

Then install a C++ compiler for your platform. The project uses C++23 and
`import std;`, so older compilers will not work.

| Platform | Compiler |
| --- | --- |
| Windows | [Visual Studio 2026](https://visualstudio.microsoft.com/downloads/) with the "Desktop development with C++" workload. |
| Linux | [Clang 22](https://apt.llvm.org/) with libc++, or [GCC 15](https://gcc.gnu.org/). Alternatively, use the [devcontainer](.devcontainer/Dockerfile), which includes everything. |
| macOS | [Xcode Command Line Tools](https://developer.apple.com/xcode/resources/) (`xcode-select --install`) and [Homebrew](https://brew.sh/) LLVM 22 (`brew install llvm`). |
| SteamOS | [Docker](https://docs.docker.com/get-started/get-docker/); the build runs in a [container](.devcontainer/steamos/README.md). |
| WebAssembly | [Python 3](https://www.python.org/downloads/). The Emscripten compiler is downloaded by a project script. |

You will also use [vcpkg](https://learn.microsoft.com/vcpkg/), a library
manager, but it comes bundled with the project as a submodule. On Linux and
macOS it needs a few basic tools; see its
[prerequisites](https://learn.microsoft.com/vcpkg/concepts/supported-hosts).

Optional: [Visual Studio Code](https://code.visualstudio.com/) with the
[CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
and [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
extensions. CMake Tools lets you pick a preset and build from the editor.

## Getting the source

The project depends on two Git submodules (other repositories included inside
this one): `vcpkg` for libraries and `emsdk` for WebAssembly builds. Clone
everything at once:

```sh
git clone --recurse-submodules https://github.com/funnansoftware/aspire.git
cd aspire
```

If you already cloned without `--recurse-submodules`, fetch the submodules now:

```sh
git submodule update --init --recursive
```

Then set up vcpkg once. This builds the `vcpkg` program that downloads and
compiles the project's libraries during your first build:

```sh
./vcpkg/bootstrap-vcpkg.sh -disableMetrics      # Linux, macOS, SteamOS
```

```powershell
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics     # Windows
```

When you pull new changes later, update the submodules too:

```sh
git pull --recurse-submodules
```

## Building

### Choosing a preset

A preset is a ready-made set of build options. Pick the one for your platform
and use its name in the commands below. Names follow the pattern
`<architecture>-<platform>-<compiler>-<configuration>`; use `debug` while
developing and `release` for optimized builds.

| Platform | Presets |
| --- | --- |
| Windows | `x64-windows-msvc-debug`, `x64-windows-msvc-release`, `x64-windows-clang-debug`, `x64-windows-clang-release` |
| Linux | `x64-linux-clang-debug`, `x64-linux-clang-release`, `x64-linux-gcc-debug`, `x64-linux-gcc-release` |
| macOS | `arm64-macos-clang-debug`, `arm64-macos-clang-release` |
| SteamOS | `x64-steamos-gcc-release` |
| WebAssembly | `wasm32-emscripten-emcc-debug`, `wasm32-emscripten-emcc-release` |

Every build follows the same four steps, run from the repository root.
Replace `<preset>` with your preset name:

```sh
cmake --preset <preset>            # configure
cmake --build --preset <preset>    # compile
ctest --preset <preset>            # run the tests
cmake --install build/<preset>     # collect the finished files
```

The first build downloads and compiles libraries, so it can take a while. The
finished files are placed in `build/<preset>/installed`.

### Windows

Open **Developer PowerShell for VS** from the Start menu (choose the x64
version) instead of a normal terminal, so the compiler can be found. If you use
VS Code with CMake Tools, it does this for you.

```powershell
cmake --preset x64-windows-msvc-debug
cmake --build --preset x64-windows-msvc-debug
ctest --preset x64-windows-msvc-debug
cmake --install build/x64-windows-msvc-debug
```

The `clang` presets use the Clang compiler from [LLVM](https://github.com/llvm/llvm-project/releases)
together with Visual Studio's libraries; install LLVM 22 to use them.

### Linux

```sh
cmake --preset x64-linux-clang-debug
cmake --build --preset x64-linux-clang-debug
ctest --preset x64-linux-clang-debug
cmake --install build/x64-linux-clang-debug
```

The `clang` presets need Clang, `clang-tools`, `libc++`, and `libc++abi`, all
from the same LLVM release, with that release's `bin` directory on your `PATH`.
The `gcc` presets need GCC 15. If you would rather not install these yourself,
open the project in the [devcontainer](.devcontainer/Dockerfile) with
[Dev Containers](https://code.visualstudio.com/docs/devcontainers/containers).

### macOS

```sh
cmake --preset arm64-macos-clang-debug
cmake --build --preset arm64-macos-clang-debug
ctest --preset arm64-macos-clang-debug
cmake --install build/arm64-macos-clang-debug
```

### SteamOS

Builds for the Steam Deck run inside a container so they match SteamOS's system
libraries. Follow the [SteamOS devcontainer instructions](.devcontainer/steamos/README.md)
and build the `x64-steamos-gcc-release` preset there.

### WebAssembly

Download the Emscripten compiler once into the project's `.emsdk` folder (rerun
this step after the `emsdk` submodule updates), then build:

```sh
git submodule update --init emsdk
cmake -P cmake/bootstrap-emsdk.cmake
cmake --preset wasm32-emscripten-emcc-release
cmake --build --preset wasm32-emscripten-emcc-release
cmake --install build/wasm32-emscripten-emcc-release
```

You do not need to install Emscripten yourself, and a system-wide installation
is ignored. Do not run `emsdk install` inside the `emsdk` folder.

## Troubleshooting

- **Missing vcpkg or submodule errors:** run
  `git submodule update --init --recursive`, then bootstrap vcpkg again as
  described in [Getting the source](#getting-the-source).
- **Compiler or `import std` errors:** make sure your compiler version matches
  [Prerequisites](#prerequisites) and, on Windows, that you are in a Developer
  PowerShell.
- **Stale settings after switching compilers or updating tools:** run
  `cmake --fresh --preset <preset>` to reconfigure from scratch.
