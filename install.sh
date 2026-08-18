#!/usr/bin/env bash
# ==============================================================================
# NextViper Official Installation Script
# https://nextviper.nuratix.com
# ==============================================================================
set -euo pipefail

VERSION="1.0.0"
INSTALL_DIR="${NEXTVIPER_INSTALL_DIR:-$HOME/.local/bin}"
REPO_URL="https://github.com/nuratix/nextviper"

echo "=================================================="
echo "      NextViper Installer v${VERSION}"
echo "=================================================="

# 1. Detect Operating System and Architecture
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "$ARCH" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    aarch64|arm64)
        ARCH="aarch64"
        ;;
    armv7l|armv8l)
        ARCH="arm"
        ;;
    *)
        echo "Warning: Architecture '$ARCH' may require compilation from source."
        ;;
esac

echo "Detected OS:           ${OS}"
echo "Detected Architecture: ${ARCH}"
echo "Target Directory:      ${INSTALL_DIR}"
echo ""

mkdir -p "${INSTALL_DIR}"

# 2. Check if local repository build is present
if [ -f "./bin/nextviper" ]; then
    echo "Found local binary build. Installing..."
    cp -f ./bin/nextviper "${INSTALL_DIR}/nextviper"
    chmod +x "${INSTALL_DIR}/nextviper"
    if [ -f "./bin/nextviper-lsp" ]; then
        cp -f ./bin/nextviper-lsp "${INSTALL_DIR}/nextviper-lsp"
        chmod +x "${INSTALL_DIR}/nextviper-lsp"
    fi
    echo "✓ Installed local NextViper binaries to ${INSTALL_DIR}"
elif command -v make >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1 && [ -f "./Makefile" ]; then
    echo "Building NextViper from source..."
    make -j"$(nproc 2>/dev/null || echo 2)"
    cp -f ./bin/nextviper "${INSTALL_DIR}/nextviper"
    chmod +x "${INSTALL_DIR}/nextviper"
    if [ -f "./bin/nextviper-lsp" ]; then
        cp -f ./bin/nextviper-lsp "${INSTALL_DIR}/nextviper-lsp"
        chmod +x "${INSTALL_DIR}/nextviper-lsp"
    fi
    echo "✓ Built and installed NextViper to ${INSTALL_DIR}"
else
    echo "Note: Remote release artifact distribution is managed via GitHub Releases."
    echo "To build and install from source:"
    echo "  git clone ${REPO_URL}.git"
    echo "  cd nextviper && make -j4 && ./scripts/install.sh"
fi

# 3. Verify PATH
if [[ ":$PATH:" != *":${INSTALL_DIR}:"* ]]; then
    echo ""
    echo "=================================================="
    echo "Notice: ${INSTALL_DIR} is not currently in your PATH."
    echo "Add the following line to your shell configuration profile (~/.bashrc or ~/.zshrc):"
    echo ""
    echo "  export PATH=\"${INSTALL_DIR}:\$PATH\""
    echo "=================================================="
else
    echo ""
    echo "✓ ${INSTALL_DIR} is already in your PATH."
fi

echo ""
echo "NextViper installation step completed."
