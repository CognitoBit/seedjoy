#!/bin/bash
# SeedJoy Firmware Upload Script

set -e

ARDUINO_CLI="$HOME/bin/arduino-cli"
FQBN="Seeeduino:nrf52:xiaonRF52840"
BUILD_DIR="build"

echo "====================================="
echo "SeedJoy Firmware Upload Script"
echo "====================================="
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found. Please run ./compile.sh first"
    exit 1
fi

# Check if arduino-cli exists
if [ ! -f "$ARDUINO_CLI" ]; then
    echo "Error: arduino-cli not found at $ARDUINO_CLI"
    exit 1
fi

# Find USB port
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

if [ -z "$PORT" ]; then
    echo "No USB device found!"
    echo ""
    echo "Alternative upload method:"
    echo "  1. Double-click reset button on XIAO nRF52840"
    echo "  2. A drive named 'XIAO' or 'XIAO-SENSE' will appear"
    echo "  3. Drag '$BUILD_DIR/seedjoy.ino.zip' to that drive"
    echo "  4. Board will automatically reboot with new firmware"
    exit 1
fi

echo "Found device on: $PORT"
echo "Uploading firmware..."
echo ""

$ARDUINO_CLI upload -p "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR" .

echo ""
echo "====================================="
echo "Upload complete!"
echo "====================================="
echo ""
echo "The board should now be running SeedJoy firmware."
echo "Open Serial Monitor (115200 baud) to see boot messages."
