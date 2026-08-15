#!/usr/bin/env bash
set -e

VERSION="${1:-1.0.0}"
ARCH="$(uname -m)"
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"

if [ "$ARCH" = "x86_64" ]; then
    ARCH_NAME="x86_64"
elif [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    ARCH_NAME="arm64"
else
    ARCH_NAME="$ARCH"
fi

DIST_DIR="dist"
PACKAGE_NAME="nextviper-v${VERSION}-${OS}-${ARCH_NAME}"
TARGET_DIR="${DIST_DIR}/${PACKAGE_NAME}"

echo "=================================================="
echo "  PACKAGING NEXTVIPER RELEASE v${VERSION} (${OS}-${ARCH_NAME})"
echo "=================================================="

# 1. Clean and build targets
make clean
make -j$(nproc)
make test

# 2. Prepare staging directory
rm -rf "$TARGET_DIR" "${DIST_DIR}/${PACKAGE_NAME}.tar.gz"
mkdir -p "${TARGET_DIR}/bin" "${TARGET_DIR}/share/doc/nextviper"

# 3. Copy binaries and assets
cp bin/nextviper "${TARGET_DIR}/bin/"
cp bin/nextviper-lsp "${TARGET_DIR}/bin/"
cp LICENSE "${TARGET_DIR}/"
cp COPYRIGHT.md "${TARGET_DIR}/"
cp THIRD_PARTY_LICENSES.md "${TARGET_DIR}/"
cp README.md "${TARGET_DIR}/"
cp ERROR_CODES.md "${TARGET_DIR}/share/doc/nextviper/"

# 4. Create tarball
cd "$DIST_DIR"
tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"
rm -rf "${PACKAGE_NAME}"
cd ..

# 5. Compute SHA-256
cd "$DIST_DIR"
sha256sum "${PACKAGE_NAME}.tar.gz" > "${PACKAGE_NAME}.tar.gz.sha256"
cd ..

echo "✓ Created: ${DIST_DIR}/${PACKAGE_NAME}.tar.gz"
echo "✓ SHA-256: $(cat ${DIST_DIR}/${PACKAGE_NAME}.tar.gz.sha256)"
echo "=================================================="
