# Steam Deck build container — Steam Linux Runtime 3.0 "sniper".
#
# This image exists for BINARY COMPATIBILITY with the Steam Deck, not for the
# newest toolchain. A native binary's minimum glibc equals the glibc of the
# container it was built in. The main devcontainer is Ubuntu 26.04 with a
# bleeding-edge glibc; anything built there can fail on the Deck with
# "version `GLIBC_x.xx' not found".
#
# Sniper is Steam Linux Runtime 3.0 — the container Steam guarantees at game
# launch on the Deck and every other Steam platform. It is Debian 11 based
# (glibc 2.31), below the Deck's, and glibc is forward-compatible, so a binary
# built here runs both directly in SteamOS Desktop Mode and inside the Steam
# runtime container.
#
# Image ref: https://gitlab.steamos.cloud/steamrt/steamrt/-/blob/steamrt/sniper/README.md

ARG SNIPER_IMAGE=registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest

# ---------------------------------------------------------------------------
# Stage 1: GCC 15 against sniper's glibc.
#
# aspire requires C++23 `import std;` with no header fallback. CMake supports
# that for GCC 15+ (libstdc++'s std module) and Clang with libc++. Sniper's repos
# stop at Valve's gcc-14 backport — no gcc-15, and the only clang is Debian 11's
# clang-11 — and LLVM's prebuilt releases need a newer glibc than sniper's, so
# the one way to keep the glibc floor is to build the compiler here. Bootstrap is
# disabled, so gcc-14 is the only compiler that builds it; the result still links
# only against glibc 2.31.
# ---------------------------------------------------------------------------
FROM ${SNIPER_IMAGE} AS gcc

ENV DEBIAN_FRONTEND=noninteractive

# Debian 11 has reached end of life: its security suite still publishes an index,
# but the package files behind it are gone, so apt resolves to security versions
# it cannot download (404). Drop that source in every stage. The SDK already
# carries the patched packages it was built with, and everything installed below
# resolves from bullseye proper and Valve's repo.
RUN sed -i '/debian-security/d' /etc/apt/sources.list

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
FROM ${SNIPER_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# See the gcc stage: the end-of-life security source cannot be fetched from.
RUN sed -i '/debian-security/d' /etc/apt/sources.list

# Base utilities and build tooling — the same set as the main Dockerfile, plus
# binutils for the objdump the CI glibc-floor check runs over the installed tree.
# The sniper SDK ships most of this already; installing explicitly keeps the
# image self-describing and resilient to SDK drift. The gmp/mpfr/mpc runtime
# libraries are what the GCC copied in below links against. python3-venv, which the
# main Dockerfile also installs, is left out: it must match the SDK's python3.9
# exactly, and that security build can no longer be downloaded (see the README).
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    make \
    binutils \
    pkg-config \
    python3 \
    zip \
    unzip \
    xz-utils \
    tar \
    libgmp10 \
    libmpfr6 \
    libmpc3 \
    && rm -rf /var/lib/apt/lists/*

# GCC 15. It installs under /usr/local, ahead of sniper's gcc-10 on PATH, so
# `gcc`/`g++` — which the shared compiler-gcc preset names — resolve to it. Its
# libstdc++ is newer than sniper's, so register it for any dynamically linked
# helper built and run inside the container; the steamos preset links the C++
# runtime statically, so shipped binaries do not depend on it.
COPY --from=gcc /gcc-root/usr/local /usr/local
RUN echo /usr/local/lib64 > /etc/ld.so.conf.d/gcc-15.conf && ldconfig

# CMake's compiler detection honors CC/CXX, and vcpkg's port builds go through
# that detection — this points the dependency tree at GCC 15 rather than at the
# SDK's gcc-10, which plain `cc`/`c++` still resolve to.
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
# Debian 11 ships 3.18 and Kitware's apt repo is Ubuntu-only, so install the
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

# Modern Ninja. C++ modules need Ninja >= 1.11 for CMake's dyndep support;
# Debian 11 ships 1.10.1. The upstream release binary is built on an older glibc
# than sniper's, so it runs here.
ARG NINJA_VERSION=1.13.2
ARG NINJA_SHA256=5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6
RUN curl -fsSLo /tmp/ninja.zip "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/ninja-linux.zip" \
    && echo "${NINJA_SHA256}  /tmp/ninja.zip" | sha256sum --check \
    && unzip -o /tmp/ninja.zip -d /usr/local/bin \
    && rm /tmp/ninja.zip
