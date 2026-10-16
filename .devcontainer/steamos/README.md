# Steam Deck build container (Steam Linux Runtime 4.0 "steamrt4")

A second devcontainer whose only job is to produce `evford` binaries that **run
on the Steam Deck**. The default dev container (`.devcontainer/`) is Ubuntu 26.04
with a bleeding-edge glibc; a binary built there can fail on the Deck with
`version 'GLIBC_x.xx' not found`.

## Why steamrt4

A native binary's minimum glibc equals the glibc of the container it was built
in. "steamrt4" is Steam Linux Runtime 4.0, the container Steam runs native Linux
games in. It is Debian 13 based (glibc 2.41), and glibc is forward-compatible —
so binaries built here run inside the Steam Linux Runtime 4.0 container on every
Steam platform, and directly in SteamOS Desktop Mode from SteamOS 3.8 on.
SteamOS 3.7 has glibc 2.40, so a binary that needs a `GLIBC_2.41` symbol runs
there only inside the runtime container.

|           | default dev container | this (steamrt4)         | Steam Deck (SteamOS 3.8+) |
| ---       | ---                   | ---                     | ---                       |
| base      | Ubuntu 26.04          | Debian 13 (steamrt4)    | Arch / Holo               |
| glibc     | ~2.43                 | **2.41**                | 2.41 or newer             |
| toolchain | gcc-15 / clang-22     | **gcc-15 (built here)** | —                         |

### Why the image builds GCC

aspire requires C++23 `import std;` with no header fallback. CMake supports that
for GCC 15+ and for Clang with libc++. steamrt4 ships Debian 13's gcc-14, whose
libstdc++ has no `std` module, and neither Debian 13 nor Valve's steamrt4 repo has
gcc-15. (Valve's repo does carry clang-21 with libc++, but the steamos preset is
GCC with a statically linked libstdc++.) So the Dockerfile compiles GCC 15 inside
the steamrt4 SDK — a modern compiler against the runtime's glibc — and installs it
under `/usr/local`, where plain `gcc`/`g++` resolve to it ahead of the SDK's
gcc-14. The shared `compiler-gcc` preset therefore applies unchanged. The image
also installs CMake 4.4.2, because Debian 13's CMake 3.31 is too old for
`import std`; Debian 13's Ninja 1.12 is new enough for modules.

That compile takes a while (tens of minutes) once; Docker caches the layer
afterwards. It builds only C and C++ and skips the sanitizer, OpenMP and
transactional-memory runtimes, so `-fsanitize=` does not work in this image.

GCC 15's libstdc++ is newer than SteamOS's runtime may provide, so the `x64-steamos-gcc-release`
preset links `-static-libstdc++ -static-libgcc`. The glibc floor is then the only
runtime contract. SDL3 is a static vcpkg library and loads X11/Wayland/audio
libraries at runtime, so the Deck's own copies are used.

### IBus

SteamOS builds include SDL3's IBus input-method support, like the other Linux
presets. It pulls in D-Bus and `libsystemd`, which vcpkg builds as static
libraries, so the image carries the IBus headers and the autotools their ports
need. Add new dependencies with the glibc floor in mind, and run the steamos
workflow to check.

### Updating the base image

The Dockerfile pins the SDK to a release and its digest, so the image changes only
when the Dockerfile does. To move to the current stable release — the one the
`latest-container-runtime-depot` tag points at — print its release and digest:

```sh
docker buildx imagetools inspect \
  registry.gitlab.steamos.cloud/steamrt/steamrt4/sdk:latest-container-runtime-depot \
  --format '{{index .Image.Config.Labels "os_release.build_id"}} {{.Manifest.Digest}}'
```

and put both in `STEAMRT_IMAGE` as `sdk:<release>@<digest>`. The
`latest-container-runtime-public-beta` tag is the beta. Each release's package
list is under <https://repo.steampowered.com/steamrt4/images/>.

## CI

The steamos workflow keeps this image in GHCR as
`ghcr.io/funnansoftware/aspire/steamos-devcontainer`, tagged with a hash of the
Dockerfile. A run pulls the image for the current Dockerfile; only a Dockerfile
change builds and pushes a new one, reusing the layers under the `buildcache` tag,
so a change that leaves the GCC stage alone does not recompile it.

## Open it

VS Code → **Dev Containers: Reopen in Container** → pick **aspire-steamos**.
(With multiple `.devcontainer/*/devcontainer.json` files, VS Code prompts for
which configuration to use.) The first launch pulls the steamrt4 SDK image (a few
GB) and builds GCC; the first configure then builds SDL3 and GoogleTest through
vcpkg, which are binary-cached afterwards.

## Build

```sh
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset x64-steamos-gcc-release
cmake --build --preset x64-steamos-gcc-release
ctest --preset x64-steamos-gcc-release
cmake --install build/x64-steamos-gcc-release
```

The installed tree is `build/x64-steamos-gcc-release/installed/`; `bin/evford`
is the self-contained binary to ship.

## Verify the glibc floor

Before shipping, confirm nothing newer than the steamrt4 floor leaked in (the CI
steamos workflow runs the same check over the whole installed tree):

```sh
objdump -T build/x64-steamos-gcc-release/installed/bin/evford \
  | grep -oE 'GLIBC_[0-9.]+' | sort -uV | tail -1
# expect: GLIBC_2.41 (or lower)
```

Then copy `evford` to the Deck and run it from Desktop Mode on SteamOS 3.8 or
later; older SteamOS needs it run inside the Steam Linux Runtime 4.0 container.
