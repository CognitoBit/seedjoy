#!/bin/bash
# SeedJoy Firmware Upload Script

set -e
cd "$(dirname "$0")"

ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || echo "$HOME/bin/arduino-cli")}"
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
if [ ! -x "$ARDUINO_CLI" ]; then
    echo "Error: arduino-cli not found at $ARDUINO_CLI"
    exit 1
fi

# Find USB port (macOS: cu.usbmodem*, Linux: ttyACM*); override with PORT=...
PORT="${PORT:-$(ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | head -1)}"

if [ -z "$PORT" ]; then
    echo "No USB device found!"
    echo ""
    echo "Alternative upload method:"
    echo "  1. Double-tap the RESET button on the XIAO nRF52840"
    echo "  2. A USB drive appears (the UF2 bootloader)"
    echo "  3. Copy '$BUILD_DIR/seedjoy.uf2' onto that drive"
    echo "  4. The board reboots into the new firmware automatically"
    exit 1
fi

echo "Found device on: $PORT"
echo "Uploading firmware..."
echo ""

# NRFUTIL=/path/to/adafruit-nrfutil overrides Seeed's bundled x86_64 binary
# (needed on Apple Silicon Macs without Rosetta).
EXTRA=()
[ -n "$NRFUTIL" ] && EXTRA+=(--upload-property "cmd=$NRFUTIL" \
                             --upload-property "cmd.macosx=$NRFUTIL")

"$ARDUINO_CLI" upload -p "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR" "${EXTRA[@]}" .

echo ""
echo "====================================="
echo "Upload complete!"
echo "====================================="
echo ""
echo "The board should now be running SeedJoy firmware."
echo "Open Serial Monitor (115200 baud) to see boot messages."
