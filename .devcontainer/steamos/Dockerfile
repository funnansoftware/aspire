# Steam Deck build container — Steam Linux Runtime 4.0 "steamrt4".
#
# This image exists for BINARY COMPATIBILITY with the Steam Deck, not for the
# newest toolchain. A native binary's minimum glibc equals the glibc of the
# container it was built in. The main devcontainer is Ubuntu 26.04 with a
# bleeding-edge glibc; anything built there can fail on the Deck with
# "version `GLIBC_x.xx' not found".
#
# steamrt4 is Steam Linux Runtime 4.0, the container Steam runs native Linux
# games in. It is Debian 13 based (glibc 2.41), and glibc is forward-compatible,
# so a binary built here runs inside the Steam Linux Runtime 4.0 container on
# every Steam platform, and directly in SteamOS Desktop Mode from SteamOS 3.8
# (glibc 2.41) on.
#
# Image ref: https://gitlab.steamos.cloud/steamrt/steamrt/-/blob/steamrt/steamrt4/README.md

# Pinned to a release and its digest so the image changes only when this file
# does: `latest` moves with every Steam Runtime release, and each move would
# rebuild GCC below. This is the stable release, the one the
# `latest-container-runtime-depot` tag points at; the README says how to update.
ARG STEAMRT_IMAGE=registry.gitlab.steamos.cloud/steamrt/steamrt4/sdk:4.0.20260928.262390@sha256:0c056268506cc0152b6dad5756a52075ee1a3e86845d35d9c7acecb9971842e1

# ---------------------------------------------------------------------------
# Stage 1: GCC 15 against steamrt4's glibc.
#
# aspire requires C++23 `import std;` with no header fallback. CMake supports
# that for GCC 15+ (libstdc++'s std module) and Clang with libc++. steamrt4
# ships Debian 13's gcc-14, and neither Debian 13 nor Valve's repo has gcc-15,
# so the one way to keep the glibc floor with GCC is to build the compiler here.
# Bootstrap is disabled, so gcc-14 is the only compiler that builds it; the
# result still links only against glibc 2.41.
# ---------------------------------------------------------------------------
FROM ${STEAMRT_IMAGE} AS gcc

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    make \
    xz-utils \
    tar \
    gcc-14 \
    g++-14 \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Keep the major version aligned with the main devcontainer's GCC 15. The sanitizer,
# OpenMP and transactional-memory runtimes are skipped: nothing here uses them and
# they dominate the build time.
ARG GCC_VERSION=15.3.0
ARG GCC_SHA256=fa59c1beef8995f27c4d71c1df227587189315d3e6faff1bb4306e61b0c530eb
RUN mkdir /tmp/gcc \
    && cd /tmp/gcc \
    && curl -fsSLo gcc.tar.xz "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz" \
    && echo "${GCC_SHA256}  gcc.tar.xz" | sha256sum --check \
    && mkdir src build \
    && tar -xJf gcc.tar.xz --strip-components=1 -C src \
    && cd build \
    && CC=gcc-14 CXX=g++-14 ../src/configure \
        --prefix=/usr/local \
        --enable-languages=c,c++ \
        --enable-default-pie \
        --disable-bootstrap \
        --disable-multilib \
        --disable-nls \
        --disable-libsanitizer \
        --disable-libgomp \
        --disable-libitm \
        --disable-libquadmath \
        --disable-libvtv \
        --disable-libssp \
        --with-system-zlib \
    && make -j"$(nproc)" \
    && make install-strip DESTDIR=/gcc-root \
    && cd / \
    && rm -rf /tmp/gcc

# ---------------------------------------------------------------------------
# Stage 2: the build environment.
# ---------------------------------------------------------------------------
FROM ${STEAMRT_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# Base utilities and build tooling — the same set as the main Dockerfile, plus
# binutils for the objdump the CI glibc-floor check runs over the installed tree.
# The steamrt4 SDK ships most of this already; installing explicitly keeps the
# image self-describing and resilient to SDK drift. The gmp/mpfr/mpc runtime
# libraries are what the GCC copied in below links against. Debian 13's Ninja
# (1.12) is new enough for C++ modules, which need >= 1.11 for CMake's dyndep
# support.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    make \
    ninja-build \
    binutils \
    pkg-config \
    python3 \
    python3-venv \
    zip \
    unzip \
    xz-utils \
    tar \
    libgmp10 \
    libmpfr6 \
    libmpc3 \
    && rm -rf /var/lib/apt/lists/*

# GCC 15. It installs under /usr/local, ahead of the SDK's gcc-14 on PATH, so
# `gcc`/`g++` — which the shared compiler-gcc preset names — resolve to it. Its
# libstdc++ is newer than steamrt4's, so register it for any dynamically linked
# helper built and run inside the container; the steamos preset links the C++
# runtime statically, so shipped binaries do not depend on it.
COPY --from=gcc /gcc-root/usr/local /usr/local
RUN echo /usr/local/lib64 > /etc/ld.so.conf.d/gcc-15.conf && ldconfig

# CMake's compiler detection honors CC/CXX, and vcpkg's port builds go through
# that detection — this points the dependency tree at GCC 15 rather than at the
# SDK's gcc-14, which plain `cc`/`c++` still resolve to.
ENV CC=gcc \
    CXX=g++

# SDL3 is built by vcpkg; install its Linux system dependencies, as in the main
# Dockerfile: the X11 and Wayland backends plus OpenGL/EGL rendering. The main
# Dockerfile's IBus headers and autotools set are omitted — the steamos preset
# builds SDL3 without IBus (see the README), and no port in its plan autoreconfs.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libx11-dev \
    libxft-dev \
    libxext-dev \
    libwayland-dev \
    wayland-protocols \
    libxkbcommon-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# Modern CMake. CMakeLists requires CMake >= 4.4.2 for experimental `import std`;
# Debian 13 ships 3.31 and Kitware's apt repo is Ubuntu-only, so install the
# official upstream binary release. Keep this aligned with the main Dockerfile.
ARG CMAKE_VERSION=4.4.2
RUN mkdir /tmp/cmake-install \
    && cd /tmp/cmake-install \
    && curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    && curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-SHA-256.txt" \
    && sha256sum --check --ignore-missing "cmake-${CMAKE_VERSION}-SHA-256.txt" \
    && tar -xzf "cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" --strip-components=1 -C /usr/local \
    && cd / \
    && rm -rf /tmp/cmake-install
