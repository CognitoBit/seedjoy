#!/bin/bash
# SeedJoy Firmware Compile Script

set -e

ARDUINO_CLI="$HOME/bin/arduino-cli"
FQBN="Seeeduino:nrf52:xiaonRF52840"
BUILD_DIR="build"

echo "====================================="
echo "SeedJoy Firmware Build Script"
echo "====================================="
echo ""

# Check if arduino-cli exists
if [ ! -f "$ARDUINO_CLI" ]; then
    echo "Error: arduino-cli not found at $ARDUINO_CLI"
    echo "Please install it first: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

# Compile
echo "Compiling firmware..."
$ARDUINO_CLI compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" .

echo ""
echo "====================================="
echo "Build successful!"
echo "====================================="
echo ""
echo "Firmware files in $BUILD_DIR/:"
ls -lh "$BUILD_DIR"/*.hex "$BUILD_DIR"/*.zip 2>/dev/null || true
echo ""
echo "To upload:"
echo "  1. Double-click reset button on XIAO nRF52840"
echo "  2. Drag '$BUILD_DIR/seedjoy.ino.zip' to XIAO drive"
echo ""
echo "Or use: $ARDUINO_CLI upload -p /dev/cu.usbmodem* --fqbn $FQBN --input-dir $BUILD_DIR ."
