#!/usr/bin/env bash
# ESP32 development environment setup — CLI only, no IDE required.
# Reproduces this repo's proven setup on any Debian/Ubuntu machine:
#   ESP-IDF v6.1 + Xtensa toolchain + esptool, verified by building hello_world.
#
# Usage:
#   ./scripts/setup-esp32.sh            # full setup (sudo path if available, else user-space)
#   ./scripts/setup-esp32.sh --verify   # only verify an existing install
#
# After setup, each new shell needs:
#   get_idf                                   # if alias installed, OR
#   . ~/esp/esp-idf-v6.1/export.sh            # manual activation
#
# Then: idf.py set-target esp32 && idf.py build && idf.py -p /dev/ttyUSB0 flash monitor
set -euo pipefail

IDF_VERSION="${IDF_VERSION:-v6.1}"
IDF_DIR="${IDF_DIR:-$HOME/esp/esp-idf-v6.1}"
SYSROOT="$HOME/.local/esp-sysroot"
TARGETS="${IDF_TARGETS:-esp32}"

log() { echo "==> $*"; }
has_sudo() { sudo -n true 2>/dev/null; }

install_sysdeps_apt() {
  log "Installing system prerequisites via apt (sudo)..."
  sudo apt-get update
  sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
}

install_sysdeps_userspace() {
  # No sudo: extract Ubuntu .deb binaries into ~/.local/esp-sysroot (no root needed).
  log "No sudo — installing prerequisites into user space ($SYSROOT)..."
  local tmp; tmp="$(mktemp -d)"
  ( cd "$tmp" && apt-get download flex bison gperf cmake cmake-data ninja-build \
    ccache dfu-util librhash0 )
  mkdir -p "$SYSROOT"
  for deb in "$tmp"/*.deb; do dpkg-deb -x "$deb" "$SYSROOT"; done
  rm -rf "$tmp"
  export PATH="$SYSROOT/usr/bin:$PATH"
  export LD_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  log "cmake: $(cmake --version | head -1), ninja: $(ninja --version)"
}

clone_idf() {
  if [ -d "$IDF_DIR" ]; then
    log "ESP-IDF already present at $IDF_DIR — skipping clone."
    return
  fi
  log "Cloning ESP-IDF $IDF_VERSION (shallow + submodules)..."
  mkdir -p "$(dirname "$IDF_DIR")"
  git clone -b "$IDF_VERSION" --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "$IDF_DIR"
}

run_installer() {
  log "Running ESP-IDF installer for target(s): $TARGETS ..."
  ( cd "$IDF_DIR" && ./install.sh "$TARGETS" )
}

install_alias() {
  local rc="${SHELL##*/}"; rc="$HOME/.${rc}rc"
  local line="alias get_idf='. $IDF_DIR/export.sh'"
  if ! grep -qF "alias get_idf=" "$rc" 2>/dev/null; then
    echo "$line" >> "$rc"
    log "Added get_idf alias to $rc"
  else
    log "get_idf alias already present in $rc"
  fi
  # Persist user-space sysroot on PATH for non-sudo machines.
  if ! has_sudo && ! grep -qF "esp-sysroot" "$rc" 2>/dev/null; then
    {
      echo 'export PATH="$HOME/.local/esp-sysroot/usr/bin:$PATH"'
      echo 'export LD_LIBRARY_PATH="$HOME/.local/esp-sysroot/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"'
    } >> "$rc"
    log "Persisted esp-sysroot PATH entries to $rc"
  fi
}

verify() {
  log "Verifying install..."
  if ! has_sudo; then
    export PATH="$SYSROOT/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  fi
  # shellcheck disable=SC1091
  . "$IDF_DIR/export.sh" >/dev/null
  idf.py --version
  python -m esptool version
  local demo="$HOME/esp/hello_world"
  rm -rf "$demo"
  cp -r "$IDF_PATH/examples/get-started/hello_world" "$demo"
  idf.py -C "$demo" set-target esp32
  idf.py -C "$demo" build
  log "VERIFY OK — $demo/build/hello_world.bin built for esp32."
}

fix_serial_permissions() {
  if groups | grep -q dialout; then
    log "User already in dialout group."
  else
    log "Adding user to dialout group (re-login required afterwards)..."
    if has_sudo; then sudo usermod -a -G dialout "$USER"; else
      echo "NO SUDO: ask an admin to run: sudo usermod -a -G dialout $USER"
    fi
  fi
}

if [ "${1:-}" = "--verify" ]; then verify; exit 0; fi

command -v git >/dev/null || { echo "git is required but missing."; exit 1; }
if has_sudo; then install_sysdeps_apt; else install_sysdeps_userspace; fi
clone_idf
run_installer
install_alias
fix_serial_permissions
verify
log "Done. Open a new shell (or run: get_idf) and start building."
