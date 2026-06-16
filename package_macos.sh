#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-macos}"
CONFIGURATION="${CONFIGURATION:-Release}"
APP_NAME="InvestInsight"
PACKAGE_BASE="$ROOT_DIR/InvestInsight-macOS.app"
ZIP_BASE="$ROOT_DIR/InvestInsight-macOS.zip"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "[package] macOS packaging must run on macOS." >&2
  exit 1
fi

clean_output_path() {
  local path="$1"
  if [[ -e "$path" ]]; then
    if rm -rf "$path" 2>/dev/null; then
      printf '%s\n' "$path"
      return 0
    fi

    local dir leaf stem ext candidate
    dir="$(dirname "$path")"
    leaf="$(basename "$path")"
    if [[ "$leaf" == *.* ]]; then
      stem="${leaf%.*}"
      ext=".${leaf##*.}"
    else
      stem="$leaf"
      ext=""
    fi

    for i in $(seq 1 999); do
      candidate="$dir/$stem-$i$ext"
      if [[ ! -e "$candidate" ]]; then
        echo "[package] Cannot remove $path, using $candidate instead." >&2
        printf '%s\n' "$candidate"
        return 0
      fi
    done

    echo "[package] Cannot find an available incremental name for $path." >&2
    return 1
  fi

  printf '%s\n' "$path"
}

find_app_bundle() {
  local candidates=(
    "$BUILD_DIR/$APP_NAME.app"
    "$BUILD_DIR/$CONFIGURATION/$APP_NAME.app"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -d "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  find "$BUILD_DIR" -name "$APP_NAME.app" -type d -print | head -n 1
}

echo "[package] Configure CMake..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIGURATION"

echo "[package] Build $CONFIGURATION..."
cmake --build "$BUILD_DIR" --config "$CONFIGURATION" --parallel

APP_SOURCE="$(find_app_bundle)"
if [[ -z "$APP_SOURCE" || ! -d "$APP_SOURCE" ]]; then
  echo "[package] $APP_NAME.app was not found." >&2
  exit 1
fi

PACKAGE_APP="$(clean_output_path "$PACKAGE_BASE")"
cp -R "$APP_SOURCE" "$PACKAGE_APP"

if command -v macdeployqt >/dev/null 2>&1; then
  echo "[package] Running macdeployqt..."
  macdeployqt "$PACKAGE_APP" -always-overwrite
else
  echo "[package] macdeployqt was not found. The .app was copied, but target machines may miss Qt Frameworks." >&2
fi

ZIP_PATH="$(clean_output_path "$ZIP_BASE")"
if command -v ditto >/dev/null 2>&1; then
  ditto -c -k --sequesterRsrc --keepParent "$PACKAGE_APP" "$ZIP_PATH"
else
  (cd "$ROOT_DIR" && zip -r "$ZIP_PATH" "$(basename "$PACKAGE_APP")")
fi

echo "[package] macOS app: $PACKAGE_APP"
echo "[package] ZIP: $ZIP_PATH"
