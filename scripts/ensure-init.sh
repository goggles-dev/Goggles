#!/usr/bin/env bash
# Compatibility check for older task wiring.
# Newer tasks do not auto-install hooks; use `pixi run init` explicitly.

set -euo pipefail

if [[ "${CI:-}" == "true" ]]; then
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

exec bash "${PROJECT_ROOT}/scripts/install-pre-commit-hook.sh" --check
