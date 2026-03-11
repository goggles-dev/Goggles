#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(git rev-parse --show-toplevel)
BUILD_DIR="$ROOT_DIR/build/debug"
TOOL="$BUILD_DIR/tests/goggles_shader_batch_report"
OUTPUT="$ROOT_DIR/build/shader_test_results.json"

if [[ ! -x "$TOOL" ]]; then
    pixi run build -p debug
fi

ARGS=(--output "$OUTPUT")
if [[ $# -ge 1 ]]; then
    ARGS+=(--category "$1")
fi

"$TOOL" "${ARGS[@]}"
