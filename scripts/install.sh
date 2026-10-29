#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="${NEXTVIPER_INSTALL_DIR:-$HOME/.local/bin}"

if [[ "${1:-}" == "--help" ]]; then
    echo "Usage: bash scripts/install.sh [--uninstall]"
    echo "NEXTVIPER_INSTALL_DIR sets the binary destination (default: ~/.local/bin)."
    echo "NEXTVIPER_BUILD_DIR selects an existing CMake build directory."
    exit 0
fi
if [[ "${1:-}" == "--uninstall" && $# == 1 ]]; then
    rm -f -- "$INSTALL_DIR/nextviper" "$INSTALL_DIR/nextviper-lsp"
    echo "Removed NextViper binaries from $INSTALL_DIR"
    exit 0
fi
if [[ $# != 0 ]]; then
    echo "Unknown installer argument. Use --help." >&2
    exit 1
fi

if [[ -n "${NEXTVIPER_BUILD_DIR:-}" ]]; then
    BINARY_DIR="$NEXTVIPER_BUILD_DIR/bin"
elif [[ -f "$SOURCE_DIR/Makefile" ]]; then
    make -C "$SOURCE_DIR" -j"${NEXTVIPER_BUILD_JOBS:-2}" all
    BINARY_DIR="$SOURCE_DIR/bin"
else
    BINARY_DIR="$SOURCE_DIR/bin"
fi
for name in nextviper nextviper-lsp; do
    if [[ ! -x "$BINARY_DIR/$name" ]]; then
        echo "Missing executable: $BINARY_DIR/$name. Build NextViper first." >&2
        exit 1
    fi
done
"$BINARY_DIR/nextviper" --version
mkdir -p -- "$INSTALL_DIR"
for name in nextviper nextviper-lsp; do
    install -m 755 -- "$BINARY_DIR/$name" "$INSTALL_DIR/.$name.installing.$$"
    mv -f -- "$INSTALL_DIR/.$name.installing.$$" "$INSTALL_DIR/$name"
done
echo "Installed NextViper and its language server into $INSTALL_DIR"
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo "Add this directory to PATH in your shell configuration: $INSTALL_DIR"
fi
