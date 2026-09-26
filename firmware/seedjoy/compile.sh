#!/bin/bash
# SeedJoy Firmware Compile Script
#
# Builds for the Seeed Studio XIAO nRF52840 (non-Sense) and produces:
#   build/seedjoy.ino.hex   - raw image
#   build/seedjoy.ino.zip   - DFU package (used by `arduino-cli upload` / upload.sh)
#   build/seedjoy.uf2       - drag-and-drop image for the UF2 bootloader drive
#
# Optional environment overrides (needed on Apple Silicon Macs WITHOUT Rosetta,
# because Seeed's bundled gcc, adafruit-nrfutil and Arduino's ctags are x86_64-only):
#   ARM_GCC_BIN=/path/to/arm-gnu-toolchain/bin   native arm-none-eabi toolchain
#   NRFUTIL=/path/to/adafruit-nrfutil             e.g. from `pip install adafruit-nrfutil`
#   CTAGS_DIR=/path/to/dir-containing-ctags       Arduino ctags fork built natively

set -e
cd "$(dirname "$0")"

FQBN="Seeeduino:nrf52:xiaonRF52840"
BUILD_DIR="build"
CORE_VERSION_DIR=$(ls -d "$HOME"/Library/Arduino15/packages/Seeeduino/hardware/nrf52/* \
                         "$HOME"/.arduino15/packages/Seeeduino/hardware/nrf52/* 2>/dev/null | tail -1)

ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || echo "$HOME/bin/arduino-cli")}"

echo "====================================="
echo "SeedJoy Firmware Build Script"
echo "====================================="
echo ""

if [ ! -x "$ARDUINO_CLI" ]; then
    echo "Error: arduino-cli not found (looked on PATH and at $HOME/bin/arduino-cli)"
    echo "Install it: https://arduino.github.io/arduino-cli/latest/installation/"
    exit 1
fi

EXTRA=()
[ -n "$ARM_GCC_BIN" ] && EXTRA+=(--build-property "compiler.path=${ARM_GCC_BIN%/}/")
[ -n "$NRFUTIL" ]     && EXTRA+=(--build-property "tools.nrfutil.cmd=$NRFUTIL" \
                                 --build-property "tools.nrfutil.cmd.macosx=$NRFUTIL")
[ -n "$CTAGS_DIR" ]   && EXTRA+=(--build-property "tools.ctags.path=${CTAGS_DIR%/}")

echo "Compiling firmware..."
"$ARDUINO_CLI" compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" "${EXTRA[@]}" .

# The Seeed core (1.1.x) skips UF2 generation for the XIAO, so do it here.
# 0xADA52840 is the UF2 family ID of the XIAO nRF52840's Adafruit-based bootloader.
UF2CONV="$CORE_VERSION_DIR/tools/uf2conv/uf2conv.py"
if [ -f "$UF2CONV" ]; then
    python3 "$UF2CONV" -f 0xADA52840 -c -o "$BUILD_DIR/seedjoy.uf2" "$BUILD_DIR/seedjoy.ino.hex"
else
    echo "Warning: uf2conv.py not found in the Seeed core; skipping .uf2 generation"
fi

echo ""
echo "====================================="
echo "Build successful!"
echo "====================================="
echo ""
echo "Firmware files in $BUILD_DIR/:"
ls -lh "$BUILD_DIR"/*.hex "$BUILD_DIR"/*.zip "$BUILD_DIR"/*.uf2 2>/dev/null || true
echo ""
echo "To flash:"
echo "  1. Double-tap RESET on the XIAO nRF52840 (a USB drive appears)"
echo "  2. Copy '$BUILD_DIR/seedjoy.uf2' onto that drive"
echo ""
echo "Or over serial: ./upload.sh"
