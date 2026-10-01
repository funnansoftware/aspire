FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

# Base utilities and build tooling. vcpkg's libsystemd build needs Python venvs.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    make \
    ninja-build \
    pkg-config \
    python3 \
    python3-venv \
    zip \
    unzip \
    xz-utils \
    tar \
    && rm -rf /var/lib/apt/lists/*

# Keep this aligned with CMAKE_EXPERIMENTAL_CXX_IMPORT_STD and native CI.
ARG CMAKE_VERSION=4.4.2
RUN mkdir /tmp/cmake-install \
    && cd /tmp/cmake-install \
    && case "$(uname -m)" in x86_64|aarch64) arch="$(uname -m)" ;; *) exit 1 ;; esac \
    && curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${arch}.tar.gz" \
    && curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-SHA-256.txt" \
    && sha256sum --check --ignore-missing "cmake-${CMAKE_VERSION}-SHA-256.txt" \
    && tar -xzf "cmake-${CMAKE_VERSION}-linux-${arch}.tar.gz" --strip-components=1 -C /usr/local \
    && cd / \
    && rm -rf /tmp/cmake-install

# GCC 15.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-15 \
    g++-15 \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100

# Clang 22.
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-22 \
    clang-tools-22 \
    libc++-22-dev \
    libc++abi-22-dev \
    clang-format-22 \
    clang-tidy-22 \
    clangd-22 \
    lld-22 \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-22 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-22 100 \
    && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-22 100 \
    && update-alternatives --install /usr/bin/lld lld /usr/bin/lld-22 100 \
    && update-alternatives --install /usr/bin/run-clang-tidy run-clang-tidy /usr/bin/run-clang-tidy-22 100 \
    && update-alternatives --install /usr/bin/clang-tidy-diff clang-tidy-diff /usr/bin/clang-tidy-diff-22.py 100

# CMake's C++ module support also needs the matching clang-scan-deps.
ENV PATH="/usr/lib/llvm-22/bin:${PATH}"

# The Debian libc++ package's manifest uses paths relative to LLVM's lib directory.
# Clang 22 discovers it in the multiarch directory instead, breaking import std.
# The Clang preset forwards this override to CMake; other toolchains ignore it.
ENV CMAKE_CXX_STDLIB_MODULES_JSON="/usr/lib/llvm-22/lib/libc++.modules.json"

# SDL3 is built by vcpkg; install its Linux system dependencies.
# Cover the default X11, Wayland and IBus features, plus OpenGL/EGL rendering.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libx11-dev \
    libxft-dev \
    libxext-dev \
    libwayland-dev \
    wayland-protocols \
    libxkbcommon-dev \
    libibus-1.0-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    libltdl-dev \
    && rm -rf /var/lib/apt/lists/*
