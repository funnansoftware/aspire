# GitHub Actions

These workflows mirror [evford's](https://github.com/funnansoftware/evford/tree/main/.github).
Each platform builds on pushes and pull requests targeting `main`. All five
workflows can also be started manually from the Actions tab.

| Workflow | Runner / toolchain | CMake preset | Artifact |
| --- | --- | --- | --- |
| Windows | Windows Server 2025, Visual Studio 2026 / MSVC | `x64-windows-msvc-release` | `aspire-windows-x64` |
| Linux | Devcontainer: Ubuntu 26.04 / Clang 22 and libc++ | `x64-linux-clang-release` | `aspire-linux-x64` |
| SteamOS | Devcontainer: Steam Runtime sniper (glibc 2.31) / GCC 15 built from source | `x64-steamos-gcc-release` | `aspire-steamos-x64` |
| macOS | macOS 15 ARM64 / Homebrew LLVM 22 | `arm64-macos-clang-release` | `aspire-macos-arm64` |
| Web | Devcontainer: Ubuntu 26.04 / pinned Emscripten SDK | `wasm32-emscripten-emcc-release` | `aspire-web` |

Each workflow configures, builds, installs, and uploads a Release build of the
`evford` app. Native workflows run the world test and the SDL smoke test using
its dummy video driver. Web builds check that the installed HTML, JavaScript,
and WebAssembly files are nonempty; this does not replace a browser runtime test.

Linux and Web use the shared `devcontainer` action to build
`.devcontainer/Dockerfile` with the same `.devcontainer` build context as local
development. SteamOS passes `directory: .devcontainer/steamos` to build that
image instead. Docker Buildx caches image layers per workflow in the Actions
cache and loads the image on the Ubuntu 24.04 runner. All dependency bootstrap,
configure, build, test, and packaging commands run inside that image. The
workspace is mounted at its original path and commands run as the runner's
UID/GID, keeping caches and artifacts accessible to subsequent Actions steps.
Images are built from the checked-out revision, including pull requests that
change the Dockerfile. No registry credentials or package write permissions
are needed, including for fork pull requests. Windows and macOS retain native
runners because the Linux devcontainer does not provide MSVC or Apple's SDK.

CMake is pinned to 4.4.2 in the devcontainer and the native workflows'
`get-cmake` steps, because 4.4.3 is not yet in that action's release catalog.
The project minimum matches. Its experimental `import std` UUID is also valid
for 4.4.3, so newer local installs work unchanged. When changing CMake
versions, check the UUID in that release's
[`Help/dev/experimental.rst`](https://github.com/Kitware/CMake/blob/v4.4.2/Help/dev/experimental.rst#c-import-std-support)
and update the project minimum, Dockerfile, and workflow pins together.

The devcontainer installs Ubuntu's LLVM 22 packages, including the libc++
modules and Clang dependency scanner, and GCC 15. Windows initializes an MSVC
developer environment; the shared Windows presets find the compiler and SDK
tools on `PATH`. The VS Code workspace explicitly enables the Visual Studio
developer environment, including when standalone Clang and Ninja are already
on `PATH`. The Windows presets request x64 host and target tools with the
`external` strategy so CMake Tools can initialize the environment for Ninja.
Command-line developers should use an x64 Visual Studio Developer PowerShell or
Developer Command Prompt, or initialize `VsDevCmd.bat -arch=amd64` first.
The clang-cl presets also use that environment's `VCToolsInstallDir` to find
the MSVC standard library modules.

CI uses the same Release presets as developers, without CI-specific presets
or compiler flag overrides. For Linux Clang builds, install matching Clang,
clang-tools (for clang-scan-deps), libc++, and libc++abi packages and put that
LLVM installation's `bin` directory on `PATH`. The Linux Clang presets build
vcpkg dependencies with the `x64-linux-libcxx-release` overlay triplet so C++
dependencies share the application's libc++ ABI. The macOS preset selects
Homebrew LLVM 22's libc++ for both compilation and linking. Run the same
configure, build, and test commands shown in the platform workflow to
reproduce a CI build locally.

The shared `get-vcpkg` action bootstraps the checked-in submodule and caches
compiled dependencies. Linux and Web defer bootstrap to the container, after
restoring the cache on the host. Cache keys separate platforms and workflows
and track the manifest, ports, triplets, SDK, and commit, with fallback
restores. This allows updated compiler ABIs to be saved on subsequent commits.
Emscripten has its own cache and is installed by `cmake/bootstrap-emsdk.cmake`
using the release recorded in the SDK submodule.

The SteamOS workflow builds in the Steam Linux Runtime 3.0 "sniper" SDK
(glibc 2.31) so the binary runs on the Steam Deck. Sniper offers neither GCC 15
nor a usable Clang for `import std`, so
[`.devcontainer/steamos/Dockerfile`](../.devcontainer/steamos/Dockerfile) builds
GCC 15 from source in a separate stage and installs CMake 4.4.2 and Ninja 1.13
beside it. A cold image build takes tens of minutes, which is why that job's
timeout is longer; Buildx caches the compiler layer in the Actions cache like
the other images. The preset links libstdc++ and libgcc statically, and the
workflow fails if any installed binary references a glibc symbol newer than
`GLIBC_2.31`. The artifact needs only SteamOS's own glibc and SDL runtime
libraries (SDL3 itself is linked statically); see
[its README](../.devcontainer/steamos/README.md).

Download artifacts from a successful Actions run. Linux, SteamOS and macOS outputs
are tar archives to preserve executable permissions. These are build outputs,
not signed installers or fully bundled distributions: Linux requires the
matching libc++ and SDL's X11 or Wayland system libraries, macOS requires
Homebrew LLVM 22, and Windows requires the MSVC runtime (SDL3.dll is included).

To preview the web artifact, extract it, run `python -m http.server 8000` in
that directory, and open `http://localhost:8000/evford.html`. No deployment
service or repository secrets are required by these workflows.
