# Steam Deck build container (Steam Linux Runtime "sniper")

A second devcontainer whose only job is to produce `evford` binaries that **run
on the Steam Deck**. The default dev container (`.devcontainer/`) is Ubuntu 26.04
with a bleeding-edge glibc; a binary built there can fail on the Deck with
`version 'GLIBC_x.xx' not found`.

## Why sniper

A native binary's minimum glibc equals the glibc of the container it was built
in. "Sniper" is Steam Linux Runtime 3.0, the container Steam guarantees at game
launch on the Deck and every other Steam platform. It is Debian 11 based
(glibc 2.31), below the Deck's, and glibc is forward-compatible — so binaries
built here run both directly in SteamOS Desktop Mode and inside the Steam
runtime container.

|           | default dev container | this (sniper)          | Steam Deck (SteamOS 3.x) |
| ---       | ---                   | ---                    | ---                      |
| base      | Ubuntu 26.04          | Debian 11 (sniper)     | Arch / Holo              |
| glibc     | ~2.43                 | **2.31**               | newer                    |
| toolchain | gcc-15 / clang-22     | **gcc-15 (built here)**| —                        |

### Why the image builds GCC

aspire requires C++23 `import std;` with no header fallback. CMake supports that
for GCC 15+ and for Clang with libc++, and sniper has neither: Valve's backport
repo stops at gcc-14 (whose libstdc++ has no `std` module), the only clang is
Debian 11's clang-11, and LLVM's prebuilt releases need a newer glibc than
sniper's. So the Dockerfile compiles GCC 15 inside the sniper SDK — a modern
compiler against the old glibc — and installs it under `/usr/local`, where plain
`gcc`/`g++` resolve to it ahead of the SDK's gcc-10. The shared `compiler-gcc`
preset therefore applies unchanged. The image also installs CMake 4.4.2 and Ninja
1.13, because Debian 11's CMake 3.18 and Ninja 1.10 are too old for modules.

That compile takes a while (tens of minutes) once; Docker caches the layer
afterwards. It builds only C and C++ and skips the sanitizer, OpenMP and
transactional-memory runtimes, so `-fsanitize=` does not work in this image.

GCC 15's libstdc++ is newer than SteamOS's runtime may provide, so the `x64-steamos-gcc-release`
preset links `-static-libstdc++ -static-libgcc`. The glibc floor is then the only
runtime contract. SDL3 is a static vcpkg library and loads X11/Wayland/audio
libraries at runtime, so the Deck's own copies are used.

### What sniper can't build

`vcpkg.json` makes SDL3's IBus input-method support an `ibus` manifest feature,
on by default. IBus pulls in D-Bus, which pulls in `libsystemd`, and systemd needs
`mallinfo2` and kernel headers newer than sniper's glibc 2.31 provides. The
`x64-steamos-gcc-release` preset sets `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON` to
skip it, so SteamOS builds have no IBus IME; other presets still build it. Add
new dependencies with this floor in mind, and run the steamos workflow to check.

### Debian 11 is end of life

Debian 11's security repository no longer serves the files its index lists, so apt
in the stock sniper SDK cannot install packages that have a security build. The
Dockerfile drops that source; everything it installs resolves from bullseye proper
and Valve's repo. Two consequences if a future port needs them:

- `python3-venv` can't be installed as-is: it must match the SDK's python3.9
  security build exactly, which no longer exists. Downgrading the whole 3.9 set to
  bullseye's 3.9.2-1 with `apt-get install --allow-downgrades` works (tested).
- Debian 11's Autoconf is 2.69; ports that autoreconf with `AC_PREREQ(2.70)`
  (vcpkg's gperf, for one) need the upstream release built into `/usr/local`.

The current steamos dependency plan needs neither.

## Open it

VS Code → **Dev Containers: Reopen in Container** → pick **aspire-steamos**.
(With multiple `.devcontainer/*/devcontainer.json` files, VS Code prompts for
which configuration to use.) The first launch pulls the sniper SDK image (a few
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

Before shipping, confirm nothing newer than the sniper floor leaked in (the CI
steamos workflow runs the same check over the whole installed tree):

```sh
objdump -T build/x64-steamos-gcc-release/installed/bin/evford \
  | grep -oE 'GLIBC_[0-9.]+' | sort -uV | tail -1
# expect: GLIBC_2.31 (or lower)
```

Then copy `evford` to the Deck and run it from Desktop Mode, or add it to Steam
as a non-Steam game so Steam wraps it in the Steam Linux Runtime.
