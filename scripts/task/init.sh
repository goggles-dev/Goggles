#!/usr/bin/env bash
# Init: install, repair, or check the managed pre-commit hook.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [[ "${1:-}" == "--" ]]; then
  shift
fi

exec bash "${PROJECT_ROOT}/scripts/install-pre-commit-hook.sh" "$@"
