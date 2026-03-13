#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

mode="both"

usage() {
  cat <<'EOF'
Usage:
  pixi run rehearse-filter-chain-provider [--mode both|in-tree|package]

Rehearse Goggles consuming `goggles-filter-chain` through the stable provider contract.

Modes:
  both     Run in-tree baseline and package-bridged rehearsal (default)
  in-tree  Run only the in-tree baseline preset path
  package  Run only the package-bridged preset path
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      [[ $# -ge 2 ]] || { echo "Error: --mode requires a value" >&2; exit 1; }
      mode="$2"
      shift 2
      ;;
    --mode=*)
      mode="${1#*=}"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

case "$mode" in
  both|in-tree|package)
    ;;
  *)
    echo "Error: Invalid mode '$mode'. Use both, in-tree, or package." >&2
    exit 1
    ;;
esac

ccache_tempdir="${CCACHE_TEMPDIR:-$REPO_ROOT/.cache/ccache-tmp}"
mkdir -p "$ccache_tempdir"
export CCACHE_TEMPDIR="$ccache_tempdir"

run_in_tree() {
  echo "==> Provider rehearsal: in-tree baseline"
  cmake --preset smoke-provider-in-tree
  cmake --build --preset smoke-provider-in-tree
  ctest --preset smoke-provider-in-tree --output-on-failure --label-regex 'filter-chain-contract|host-integration'
}

run_package() {
  local package_build_dir="$REPO_ROOT/build/smoke-provider-package-stage"

  echo "==> Provider rehearsal: package stage"
  cmake --preset smoke-provider-package-stage
  cmake --build --preset smoke-provider-package-stage --target goggles-filter-chain
  cmake --install "$package_build_dir"

  echo "==> Provider rehearsal: package consumer"
  cmake --preset smoke-provider-package-consumer
  cmake --build --preset smoke-provider-package-consumer
  ctest --preset smoke-provider-package-consumer --output-on-failure --label-regex 'filter-chain-contract|host-integration'
}

cd "$REPO_ROOT"

if [[ "$mode" == "both" || "$mode" == "in-tree" ]]; then
  run_in_tree
fi

if [[ "$mode" == "both" || "$mode" == "package" ]]; then
  run_package
fi

echo "Filter-chain provider rehearsal passed (${mode})."
