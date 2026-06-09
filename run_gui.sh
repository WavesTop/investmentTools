#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
APP_BIN="${BUILD_DIR}/InvestInsight"
FRAMEWORK_DIR="${SCRIPT_DIR}/third_party"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release 2>/dev/null
cmake --build "${BUILD_DIR}"

export DYLD_FRAMEWORK_PATH="${FRAMEWORK_DIR}${DYLD_FRAMEWORK_PATH:+:${DYLD_FRAMEWORK_PATH}}"
exec "${APP_BIN}"
