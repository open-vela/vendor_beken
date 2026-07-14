#!/usr/bin/env bash

set -euo pipefail

BINARY_DIR="$1"
CHIP_DIR="$2"
ENCRYPT_TOOL="$3"

"${ENCRYPT_TOOL}" -crc "${BINARY_DIR}/nuttx.bin"
cat "${CHIP_DIR}/bootloader.bin" "${BINARY_DIR}/nuttx_crc.bin" > "${BINARY_DIR}/all-app.bin"
