#!/usr/bin/env bash
set -euo pipefail
SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SOURCE_DIR"
VERSION="${1:-1.0.0}"
if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][A-Za-z0-9.-]+)?$ ]]; then
    echo "Expected a version such as 1.0.1 or 1.0.1-preview.1" >&2
    exit 1
fi
ARCH="$(uname -m)"
case "$ARCH" in arm64) ARCH=aarch64 ;; amd64) ARCH=x86_64 ;; esac
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
PACKAGE_NAME="nextviper-v${VERSION}-${OS}-${ARCH}"
if [[ "${NEXTVIPER_SKIP_BUILD:-0}" != 1 ]]; then
    make -j"${NEXTVIPER_BUILD_JOBS:-2}" test
fi
mkdir -p dist
STAGING="$(mktemp -d "$SOURCE_DIR/dist/.package.XXXXXX")"
trap 'rm -rf -- "$STAGING"' EXIT
TARGET_DIR="$STAGING/$PACKAGE_NAME"
mkdir -p "$TARGET_DIR/bin" "$TARGET_DIR/scripts" "$TARGET_DIR/share/doc/nextviper" "$TARGET_DIR/examples"
cp bin/nextviper bin/nextviper-lsp "$TARGET_DIR/bin/"
cp install.sh "$TARGET_DIR/"
cp scripts/install.sh "$TARGET_DIR/scripts/"
cp README.md LICENSE COPYRIGHT.md THIRD_PARTY_LICENSES.md CURRENT_STATUS.md "$TARGET_DIR/"
cp HTTP_API.md DATABASE_GUIDE.md ERROR_CODES.md "$TARGET_DIR/share/doc/nextviper/"
cp -R examples/tutorials examples/todo-api "$TARGET_DIR/examples/"
cp examples/hello_world.nv "$TARGET_DIR/examples/"
"$TARGET_DIR/bin/nextviper" run "$TARGET_DIR/examples/hello_world.nv" > /dev/null
tar -czf "dist/$PACKAGE_NAME.tar.gz" -C "$STAGING" "$PACKAGE_NAME"
(cd dist && sha256sum "$PACKAGE_NAME.tar.gz" > "$PACKAGE_NAME.tar.gz.sha256")
echo "Created dist/$PACKAGE_NAME.tar.gz and its SHA-256 checksum"
