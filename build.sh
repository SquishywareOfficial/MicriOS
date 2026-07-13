#!/usr/bin/env bash
set -euo pipefail

ESP32_BOARD_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

TARGETS=(
  "MicriOS-C3|esp32:esp32:esp32c3:PartitionScheme=huge_app"
  "MicriOS-T-Display|esp32:esp32:esp32:PartitionScheme=huge_app"
  "MicriOS-C3-Headless|esp32:esp32:esp32c3:PartitionScheme=huge_app"
  "MicriOS-S3-Zero-Headless|esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=opi,UploadSpeed=921600"
)

sync_shared_code() {
  local sketch="$1"
  local source="$SCRIPT_DIR/shared"
  local dest_parent="$SCRIPT_DIR/$sketch/src"
  local dest="$dest_parent/shared"

  if [[ ! -d "$source" ]]; then
    echo "Shared source folder not found: $source" >&2
    exit 1
  fi

  rm -rf "$dest"
  mkdir -p "$dest_parent"
  cp -R "$source" "$dest"
  echo "Synced shared code -> $sketch/src/shared"
}

echo "Preparing Arduino CLI dependencies"
arduino-cli config init >/dev/null 2>&1 || true
arduino-cli config set board_manager.additional_urls "$ESP32_BOARD_URL"
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install U8g2 "NimBLE-Arduino" "TFT_eSPI" ArduinoJson

for target in "${TARGETS[@]}"; do
  IFS='|' read -r sketch fqbn <<< "$target"
  sync_shared_code "$sketch"
done

cd "$SCRIPT_DIR"

for target in "${TARGETS[@]}"; do
  IFS='|' read -r sketch fqbn <<< "$target"
  echo
  echo "Building $sketch"
  arduino-cli compile --fqbn "$fqbn" "$sketch"
done

echo
echo "All MicriOS targets built successfully"
