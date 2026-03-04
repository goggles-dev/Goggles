#!/usr/bin/env bash
set -euo pipefail

ccache_tempdir="${CCACHE_TEMPDIR:-$PWD/.cache/ccache-tmp}"
mkdir -p "$ccache_tempdir"
export CCACHE_TEMPDIR="$ccache_tempdir"

presets=(
  smoke-static-clang
  smoke-shared-clang
  smoke-static-gcc
  smoke-shared-gcc
)

for preset in "${presets[@]}"; do
  echo "==> Configuring ${preset}"
  cmake --preset "$preset"

  echo "==> Building ${preset}"
  cmake --build --preset "$preset" --target goggles-filter-chain goggles_tests

  echo "==> ABI smoke test (${preset})"
  "./build/${preset}/tests/goggles_tests" "[filter_chain_c_api][abi]"
done

echo "Filter-chain local smoke matrix passed."
