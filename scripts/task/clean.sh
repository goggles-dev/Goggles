#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/parse-preset.sh" "$@"
rm -rf "build/$PRESET"
echo "Removed build/$PRESET"
