#!/usr/bin/env bash
# Installs the system packages needed to build the Phoenix engine natively on
# Debian/Ubuntu (SDL3 video/input backends + toolchain). Used by CI and devs.
set -euo pipefail
SUDO=""
if [[ $(id -u) -ne 0 ]]; then SUDO="sudo"; fi
$SUDO apt-get update -qq
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
  build-essential cmake ninja-build pkg-config git \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libudev-dev \
  libasound2-dev libpulse-dev libdbus-1-dev libibus-1.0-dev
