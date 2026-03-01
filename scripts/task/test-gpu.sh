#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

if ! python -c "import renderdoc" >/dev/null 2>&1; then
  echo "ERROR: RenderDoc Python module is unavailable." >&2
  echo "       Run 'pixi install --locked' to install project dependencies." >&2
  echo "       Verify with: pixi run python -c \"import renderdoc\"" >&2
  exit 1
fi

if ! rdc --version >/dev/null 2>&1; then
  echo "ERROR: rdc CLI is unavailable in the active Pixi environment." >&2
  echo "       Run 'pixi install --locked' to install project dependencies." >&2
  echo "       Verify with: pixi run rdc --version" >&2
  exit 1
fi

required_rdc_commands=(assert-clean assert-state assert-pixel diff)
missing_rdc_commands=()
for rdc_command in "${required_rdc_commands[@]}"; do
  if ! rdc "$rdc_command" --help >/dev/null 2>&1; then
    missing_rdc_commands+=("$rdc_command")
  fi
done

if [ "${#missing_rdc_commands[@]}" -ne 0 ]; then
  echo "ERROR: rdc command contract is incomplete in the active Pixi environment." >&2
  echo "       Missing subcommands: ${missing_rdc_commands[*]}" >&2
  echo "       Provide an rdc implementation that supports assert-clean/assert-state/assert-pixel/diff." >&2
  echo "       Verify with: pixi run rdc assert-clean --help" >&2
  exit 1
fi

"$SCRIPT_DIR/build.sh" -p "$PRESET"

pytest tests/gpu -v
