#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Build first
"$SCRIPT_DIR/build.sh" -p "$PRESET"

# By default, skip visual and gpu tests in generic test runs (CI-safe fast path).
# Set GOGGLES_INCLUDE_VISUAL_TESTS=1 to include visual tests explicitly.
CTEST_LABEL_EXCLUDE_REGEX="visual|gpu"
if [[ "${GOGGLES_INCLUDE_VISUAL_TESTS:-0}" == "1" ]]; then
  CTEST_LABEL_EXCLUDE_REGEX="gpu"
fi

# Run tests
ctest --preset "$PRESET" --output-on-failure --label-exclude "$CTEST_LABEL_EXCLUDE_REGEX"
