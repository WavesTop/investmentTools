#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
FRAMEWORK_DIR="${SCRIPT_DIR}/third_party"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" 2>/dev/null
        cmake --build "${BUILD_DIR}" --config Release
        exec "${BUILD_DIR}/Release/InvestInsight.exe"
        ;;
    *)
        cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release 2>/dev/null
        cmake --build "${BUILD_DIR}"
        export DYLD_FRAMEWORK_PATH="${FRAMEWORK_DIR}${DYLD_FRAMEWORK_PATH:+:${DYLD_FRAMEWORK_PATH}}"
        exec "${BUILD_DIR}/InvestInsight"
        ;;
esac
