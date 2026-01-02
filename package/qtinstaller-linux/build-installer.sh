#!/bin/bash
set -e

echo "========================================"
echo "Building Traintastic QtIFW Installer"
echo "========================================"
echo

# Set paths
PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$PACKAGE_DIR/../.."
CONFIG_DIR="$PACKAGE_DIR/config"
PACKAGES_DIR="$PACKAGE_DIR/packages"
OUTPUT_DIR="$PACKAGE_DIR/output"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    ARCH_NAME="x64"
elif [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    ARCH_NAME="arm64"
else
    echo "ERROR: Unsupported architecture: $ARCH"
    exit 1
fi

echo "Architecture: $ARCH_NAME ($ARCH)"

# QtIFW binary creator tool path
if [ -f "$HOME/QtIFW/bin/binarycreator" ]; then
    BINARYCREATOR="$HOME/QtIFW/bin/binarycreator"
elif [ -f "/opt/QtIFW/bin/binarycreator" ]; then
    BINARYCREATOR="/opt/QtIFW/bin/binarycreator"
else
    BINARYCREATOR="binarycreator"
fi

# Check if binarycreator exists
if ! command -v "$BINARYCREATOR" &> /dev/null; then
    echo "ERROR: Qt Installer Framework not found"
    echo "Please install Qt Installer Framework"
    exit 1
fi

echo "Using binarycreator: $BINARYCREATOR"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Get version from server binary or use default
if [ -f "$PACKAGES_DIR/org.traintastic.server/data/bin/traintastic-server" ]; then
    VERSION=$("$PACKAGES_DIR/org.traintastic.server/data/bin/traintastic-server" --version 2>&1 | grep -oP '\d+\.\d+\.\d+(-\S+)?' || echo "0.0.0")
else
    VERSION="0.0.0"
fi

echo "Version: $VERSION"
echo

# Update version in config files
echo "Updating version in config files..."
sed -i "s|<Version>.*</Version>|<Version>$VERSION</Version>|g" "$CONFIG_DIR/config.xml"
sed -i "s|<Version>.*</Version>|<Version>$VERSION</Version>|g" "$PACKAGES_DIR/org.traintastic.server/meta/package.xml"
sed -i "s|<Version>.*</Version>|<Version>$VERSION</Version>|g" "$PACKAGES_DIR/org.traintastic.client/meta/package.xml"
sed -i "s|<Version>.*</Version>|<Version>$VERSION</Version>|g" "$PACKAGES_DIR/org.traintastic.shared/meta/package.xml"

# Build installer
echo
echo "Building installer..."
INSTALLER_NAME="traintastic-setup-v${VERSION}-linux-${ARCH_NAME}.run"

"$BINARYCREATOR" --offline-only \
    -c "$CONFIG_DIR/config.xml" \
    -p "$PACKAGES_DIR" \
    "$OUTPUT_DIR/$INSTALLER_NAME"

if [ $? -ne 0 ]; then
    echo
    echo "ERROR: Installer build failed!"
    exit 1
fi

echo
echo "========================================"
echo "Build completed successfully!"
echo "Output: $OUTPUT_DIR/$INSTALLER_NAME"
echo "========================================"
echo
echo "To install:"
echo "  chmod +x $OUTPUT_DIR/$INSTALLER_NAME"
echo "  ./$OUTPUT_DIR/$INSTALLER_NAME"
echo
