#!/usr/bin/env bash
# Pre-commit hook: format staged files only (C/C++ via clang-format, TOML via taplo).
# Invoked by the managed git hook wrapper installed via `pixi run init`.

set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

# Use pixi run for formatting tools
run_clang_format() { pixi run -q clang-format "$@"; }
run_taplo() { pixi run -q taplo "$@"; }

format_cpp() {
    mapfile -d '' -t staged_cpp < <(git diff --cached --name-only -z --diff-filter=d | grep -zE '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$' || true)
    if ((${#staged_cpp[@]} == 0)); then
        return
    fi

    echo "[pre-commit] Formatting C/C++ staged files (${#staged_cpp[@]})"
    run_clang_format -i "${staged_cpp[@]}"

    if ! git diff --quiet --exit-code -- "${staged_cpp[@]}"; then
        git add -- "${staged_cpp[@]}"
        echo "[pre-commit] Re-staged formatted C/C++ files."
    fi
}

format_toml() {
    local -a staged_toml_all=()
    local -a staged_toml=()
    local -a skipped_toml=()
    local file

    mapfile -d '' -t staged_toml_all < <(git diff --cached --name-only -z --diff-filter=d | grep -zE '\.toml$' || true)
    if ((${#staged_toml_all[@]} == 0)); then
        return
    fi

    for file in "${staged_toml_all[@]}"; do
        # Test fixtures may intentionally contain malformed TOML.
        if [[ "$file" == *"/test_data/"* ]]; then
            skipped_toml+=("$file")
            continue
        fi
        staged_toml+=("$file")
    done

    if ((${#skipped_toml[@]} > 0)); then
        echo "[pre-commit] Skipping TOML fixtures under test_data (${#skipped_toml[@]})"
    fi
    if ((${#staged_toml[@]} == 0)); then
        return
    fi

    echo "[pre-commit] Formatting TOML staged files (${#staged_toml[@]})"
    run_taplo fmt "${staged_toml[@]}"

    if ! git diff --quiet --exit-code -- "${staged_toml[@]}"; then
        git add -- "${staged_toml[@]}"
        echo "[pre-commit] Re-staged formatted TOML files."
    fi
}

format_cpp
format_toml

echo "[pre-commit] Formatting clean."
