#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INSTALL_DIR="$REPO_ROOT/build/filter-chain-install/$PRESET"
VALIDATION_ROOT="$REPO_ROOT/build/filter-chain-consumers/$PRESET"
FIXTURE_DIR="$REPO_ROOT/build/filter-chain-consumer-fixtures/$PRESET"
FIXTURE_PATH="$FIXTURE_DIR/minimal.slangp"
PREFIX_PATH="$INSTALL_DIR"

if [[ -n "${CONDA_PREFIX:-}" ]]; then
  PREFIX_PATH="$PREFIX_PATH;$CONDA_PREFIX"
fi

# Build and install filter-chain as a standalone package for consumer validation.
FILTER_CHAIN_BUILD_DIR="$REPO_ROOT/build/filter-chain-package/$PRESET"
rm -rf "$INSTALL_DIR"
cmake -S "$REPO_ROOT/filter-chain" -B "$FILTER_CHAIN_BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFILTER_CHAIN_LIBRARY_TYPE=STATIC \
  -DFILTER_CHAIN_BUILD_TESTS=OFF
cmake --build "$FILTER_CHAIN_BUILD_DIR"
cmake --install "$FILTER_CHAIN_BUILD_DIR" --prefix "$INSTALL_DIR"

mkdir -p "$VALIDATION_ROOT" "$FIXTURE_DIR"

cat >"$FIXTURE_PATH" <<'EOF'
shaders = 1
shader0 = test.slang
EOF

configure_and_build() {
  local name="$1"
  local source_dir="$2"
  local build_dir="$VALIDATION_ROOT/$name"

  rm -rf "$build_dir"
  cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_PREFIX_PATH="$PREFIX_PATH"
  cmake --build "$build_dir"
}

configure_and_build "c_api" "$REPO_ROOT/filter-chain/tests/consumer/c_api"
configure_and_build "static" "$REPO_ROOT/filter-chain/tests/consumer/static"

# Detect whether a shared library was actually installed.
# If only STATIC was built (the default), the shared consumer test would
# silently link against the static archive — testing nothing new.
has_shared_lib=false
for ext in so dylib dll; do
  if compgen -G "$INSTALL_DIR/lib*/libgoggles*.$ext*" >/dev/null 2>&1; then
    has_shared_lib=true
    break
  fi
done

if $has_shared_lib; then
  configure_and_build "shared" "$REPO_ROOT/filter-chain/tests/consumer/shared"
else
  echo "⚠  Shared library not found in $INSTALL_DIR — skipping shared consumer test."
  echo "   Build with -DFILTER_CHAIN_LIBRARY_TYPE=SHARED to validate shared linkage."
fi

"$VALIDATION_ROOT/c_api/c_api_consumer" "$FIXTURE_PATH"
"$VALIDATION_ROOT/static/static_consumer"

if $has_shared_lib; then
  "$VALIDATION_ROOT/shared/shared_consumer"
fi
