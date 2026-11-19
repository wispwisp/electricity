FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-venv python3-dev python3-pip \
    ca-certificates build-essential cmake clang-tidy clang-format clang llvm afl++ libboost-dev libssl-dev ninja-build libclang-rt-dev cppcheck \
    git npm curl wget \
    pkg-config xz-utils \
    libgl-dev libegl-dev libopengl-dev libfontconfig-dev libfreetype-dev \
    libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev \
    libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-icccm4-dev libxcb-image0-dev \
    libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
    libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xinput-dev \
    libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libwayland-dev wayland-protocols \
    xvfb xauth xdotool imagemagick fonts-dejavu-core libgl1-mesa-dri gdb \
    && rm -rf /var/lib/apt/lists/*

# Qt 6.12 LTS, static, built from source into /opt/qt
ARG QT_VER=6.12.0
RUN curl -fL https://download.qt.io/official_releases/qt/6.12/${QT_VER}/single/qt-everywhere-src-${QT_VER}.tar.xz | tar -xJ -C /tmp \
    && mkdir /tmp/qt-build && cd /tmp/qt-build \
    && /tmp/qt-everywhere-src-${QT_VER}/configure -static -release -prefix /opt/qt \
       -opensource -confirm-license -submodules qtbase,qtsvg,qttools \
       -qt-zlib -qt-libpng -qt-libjpeg -qt-pcre -qt-harfbuzz \
    && cmake --build . --parallel && cmake --install . \
    && rm -rf /tmp/qt-build /tmp/qt-everywhere-src-${QT_VER}
ENV CMAKE_PREFIX_PATH=/opt/qt

ARG USER_ID
ARG GROUP_ID

RUN usermod -u 2000 ubuntu && groupmod -g 2000 ubuntu
RUN groupadd -g $GROUP_ID claudegroup && \
    useradd --uid $USER_ID --gid $GROUP_ID -m claudeuser
USER claudeuser

RUN date -u +"%Y-%m-%dT%H:%M:%SZ" > "/home/claudeuser/build-$(date -u +%Y-%m).txt"
RUN curl -fsSL https://claude.ai/install.sh | bash

ENV PATH="/home/claudeuser/.venv/bin:/home/claudeuser/.local/bin:${PATH}"

RUN python3 -m venv ~/.venv && pip install --no-cache-dir --upgrade pip wheel && \
    pip install --no-cache-dir mcp pytest ruff

COPY ./check_grammar.sh /usr/local/bin/check-grammar

WORKDIR /home/claudeuser/workspace
ENTRYPOINT ["bash"]

