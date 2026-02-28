#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

source "${SCRIPT_DIR}/parse-preset.sh" "$@"
BUILD_DIR="${PROJECT_ROOT}/build/${PRESET}"
GOLDEN_DIR="${PROJECT_ROOT}/tests/golden"
CONFIGS_DIR="${PROJECT_ROOT}/tests/visual/configs"

GOGGLES_BIN="${BUILD_DIR}/bin/goggles"
QUADRANT_CLIENT_BIN="${BUILD_DIR}/tests/clients/quadrant_client"

echo "==> Updating golden reference images (preset: ${PRESET})"
echo "    Build dir:    ${BUILD_DIR}"
echo "    Golden dir:   ${GOLDEN_DIR}"

if [[ ! -x "${GOGGLES_BIN}" ]]; then
    echo "ERROR: goggles binary not found at ${GOGGLES_BIN}"
    echo "       Run 'pixi run build' (or 'pixi run build -p ${PRESET}') first."
    exit 1
fi

if [[ ! -x "${QUADRANT_CLIENT_BIN}" ]]; then
    echo "ERROR: quadrant_client binary not found at ${QUADRANT_CLIENT_BIN}"
    echo "       Run 'pixi run build' first."
    exit 1
fi

mkdir -p "${GOLDEN_DIR}"

echo ""
echo "--> Capturing shader_bypass_quadrant.png ..."
"${GOGGLES_BIN}" \
    --headless \
    --frames 5 \
    --output "${GOLDEN_DIR}/shader_bypass_quadrant.png" \
    --config "${CONFIGS_DIR}/shader_bypass.toml" \
    -- "${QUADRANT_CLIENT_BIN}"
echo "    Done: ${GOLDEN_DIR}/shader_bypass_quadrant.png"

echo ""
echo "--> Capturing shader_zfast_quadrant.png ..."
"${GOGGLES_BIN}" \
    --headless \
    --frames 5 \
    --output "${GOLDEN_DIR}/shader_zfast_quadrant.png" \
    --config "${CONFIGS_DIR}/shader_zfast.toml" \
    -- "${QUADRANT_CLIENT_BIN}"
echo "    Done: ${GOLDEN_DIR}/shader_zfast_quadrant.png"

echo ""
echo "==> Golden images updated successfully."
echo "    Review with: feh ${GOLDEN_DIR}/*.png"
echo "    Commit when satisfied: git add tests/golden/*.png && git commit -m 'chore(tests): update golden reference images'"
