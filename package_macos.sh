#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build-macos}"
CONFIGURATION="${CONFIGURATION:-Release}"
TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-}"
SKIP_BUILD=0
NO_ZIP=0
APP_NAME="InvestInsight"
PACKAGE_BASE="$ROOT_DIR/InvestInsight-macOS.app"
ZIP_BASE="$ROOT_DIR/InvestInsight-macOS.zip"

usage() {
  cat <<EOF
Usage: ./package_macos.sh [options]

Options:
  --build-dir PATH          CMake build directory. Default: build-macos
  --configuration NAME      Build configuration. Default: Release
  --toolchain-file PATH     CMake toolchain file.
  --skip-build              Reuse an existing build output.
  --no-zip                  Do not create InvestInsight-macOS.zip.
  -h, --help                Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="${2:?Missing value for --build-dir}"
      shift 2
      ;;
    --build-dir=*)
      BUILD_DIR="${1#*=}"
      shift
      ;;
    --configuration)
      CONFIGURATION="${2:?Missing value for --configuration}"
      shift 2
      ;;
    --configuration=*)
      CONFIGURATION="${1#*=}"
      shift
      ;;
    --toolchain-file)
      TOOLCHAIN_FILE="${2:?Missing value for --toolchain-file}"
      shift 2
      ;;
    --toolchain-file=*)
      TOOLCHAIN_FILE="${1#*=}"
      shift
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --no-zip)
      NO_ZIP=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[package] Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "$BUILD_DIR" != /* ]]; then
  BUILD_DIR="$ROOT_DIR/$BUILD_DIR"
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "[package] macOS packaging must run on macOS." >&2
  exit 1
fi

get_cmake_cache_value() {
  local cache_path="$1"
  local name="$2"
  if [[ ! -f "$cache_path" ]]; then
    return 0
  fi

  awk -F= -v name="$name" '
    {
      split($1, key, ":")
      if (key[1] == name) {
        print $2
        exit
      }
    }
  ' "$cache_path"
}

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

  if [[ ! -d "$BUILD_DIR" ]]; then
    return 0
  fi

  find "$BUILD_DIR" -name "$APP_NAME.app" -type d -print | head -n 1
}

find_macdeployqt() {
  local candidate cache qt_dir share_dir installed_root

  if command -v macdeployqt >/dev/null 2>&1; then
    command -v macdeployqt
    return 0
  fi

  if [[ -n "${QTDIR:-}" ]]; then
    for candidate in "$QTDIR/bin/macdeployqt" "$QTDIR/libexec/macdeployqt"; do
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
  fi

  for cache in "$BUILD_DIR/CMakeCache.txt" "$ROOT_DIR/build/CMakeCache.txt"; do
    for qt_var in Qt5_DIR Qt6_DIR; do
      qt_dir="$(get_cmake_cache_value "$cache" "$qt_var")"
      if [[ -n "$qt_dir" ]]; then
        share_dir="$(dirname "$qt_dir")"
        installed_root="$(dirname "$(dirname "$share_dir")")"
        for candidate in "$installed_root/bin/macdeployqt" "$installed_root/libexec/macdeployqt"; do
          if [[ -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
          fi
        done
      fi
    done
  done

  for candidate in \
    /opt/homebrew/opt/qt/bin/macdeployqt \
    /opt/homebrew/opt/qt/libexec/macdeployqt \
    /usr/local/opt/qt/bin/macdeployqt \
    /usr/local/opt/qt/libexec/macdeployqt; do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

if [[ -z "$TOOLCHAIN_FILE" ]]; then
  TOOLCHAIN_FILE="$(get_cmake_cache_value "$ROOT_DIR/build/CMakeCache.txt" CMAKE_TOOLCHAIN_FILE)"
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  configure_args=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$CONFIGURATION"
  )
  if [[ -n "$TOOLCHAIN_FILE" ]]; then
    configure_args+=("-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE")
  fi

  echo "[package] Configure CMake..."
  cmake "${configure_args[@]}"

  echo "[package] Build $CONFIGURATION..."
  cmake --build "$BUILD_DIR" --config "$CONFIGURATION" --parallel
fi

APP_SOURCE="$(find_app_bundle)"
if [[ -z "$APP_SOURCE" || ! -d "$APP_SOURCE" ]]; then
  echo "[package] $APP_NAME.app was not found." >&2
  exit 1
fi

PACKAGE_APP="$(clean_output_path "$PACKAGE_BASE")"
cp -R "$APP_SOURCE" "$PACKAGE_APP"

DEPLOY_QT="$(find_macdeployqt || true)"
if [[ -n "$DEPLOY_QT" ]]; then
  echo "[package] Running macdeployqt: $DEPLOY_QT"
  "$DEPLOY_QT" "$PACKAGE_APP" -always-overwrite
else
  echo "[package] macdeployqt was not found. The .app was copied, but target machines may miss Qt Frameworks." >&2
fi

if [[ "$NO_ZIP" -eq 0 ]]; then
  ZIP_PATH="$(clean_output_path "$ZIP_BASE")"
  if command -v ditto >/dev/null 2>&1; then
    ditto -c -k --sequesterRsrc --keepParent "$PACKAGE_APP" "$ZIP_PATH"
  else
    (cd "$ROOT_DIR" && zip -r "$ZIP_PATH" "$(basename "$PACKAGE_APP")")
  fi
  echo "[package] ZIP: $ZIP_PATH"
fi

echo "[package] macOS app: $PACKAGE_APP"
