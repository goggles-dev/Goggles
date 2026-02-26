#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build_all_presets.sh [--clean]

Builds all non-hidden CMake configure/build presets.

Options:
  --clean     Remove build directories for each preset before building
EOF
}

clean=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)
      clean=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

ccache_tempdir="${CCACHE_TEMPDIR:-"$PWD/.cache/ccache-tmp"}"
mkdir -p "$ccache_tempdir"
export CCACHE_TEMPDIR="$ccache_tempdir"

mapfile -t all_configure_presets < <(
  cmake --list-presets=configure | sed -n 's/^  "\([^"]\+\)".*/\1/p'
)

build_one() {
  local preset="$1"

  echo "==> Building preset: $preset"
  if [[ $clean -eq 1 ]]; then
    if [[ -z "$preset" || "$preset" == *"/"* || "$preset" == *".."* ]]; then
      echo "Refusing to clean unsafe preset name: '$preset'" >&2
      exit 1
    fi
    rm -rf "build/$preset"
  fi

  cmake --preset "$preset"
  cmake --build --preset "$preset"
}

for preset in "${all_configure_presets[@]}"; do
  build_one "$preset"
done

echo "All presets built successfully."
