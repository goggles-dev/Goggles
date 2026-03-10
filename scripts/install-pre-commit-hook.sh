#!/usr/bin/env bash
# Install, repair, or check the managed pre-commit formatting hook.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_SRC="${PROJECT_ROOT}/scripts/pre-commit-format.sh"
MANAGED_MARKER_PREFIX="# goggles-managed-pre-commit-hook"
MANAGED_MARKER="# goggles-managed-pre-commit-hook v2"
EXPECTED_ROOT_LINE='repo_root="$(git rev-parse --show-toplevel)"'
EXPECTED_EXEC_LINE='exec "$repo_root/scripts/pre-commit-format.sh" "$@"'
MODE="install"
FORCE=0

usage() {
  cat <<'EOF'
Usage: scripts/install-pre-commit-hook.sh [--check] [--force]

  --check   Validate the managed hook without changing it
  --force   Replace an existing unmanaged hook after backing it up
  -h, --help  Show this help text
EOF
}

while (($# > 0)); do
  case "$1" in
    --check)
      MODE="check"
      ;;
    --force)
      FORCE=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[pre-commit] Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[pre-commit] Not a git checkout; skipping hook install."
  exit 0
fi

normalize_path() {
  local base_dir="$1"
  local path="$2"

  if [[ "$path" = /* ]]; then
    readlink -m "$path"
  else
    readlink -m "$base_dir/$path"
  fi
}

HOOK_DST="$(git -C "$PROJECT_ROOT" rev-parse --git-path hooks/pre-commit)"
HOOK_DST="$(normalize_path "$PROJECT_ROOT" "$HOOK_DST")"
HOOKS_DIR="$(dirname "$HOOK_DST")"
CURRENT_COMMON_DIR="$(git -C "$PROJECT_ROOT" rev-parse --git-common-dir)"
CURRENT_COMMON_DIR="$(normalize_path "$PROJECT_ROOT" "$CURRENT_COMMON_DIR")"

has_managed_marker() {
  [[ -f "$HOOK_DST" && ! -L "$HOOK_DST" ]] || return 1
  grep -Fq "$MANAGED_MARKER_PREFIX" "$HOOK_DST"
}

is_legacy_managed_target_path() {
  local hook_target="$1"

  [[ -n "$hook_target" ]] || return 1
  [[ "$hook_target" == */scripts/pre-commit-format.sh || "$hook_target" == scripts/pre-commit-format.sh ]]
}

is_legacy_managed_hook() {
  [[ -L "$HOOK_DST" ]] || return 1

  local raw_target
  local resolved_hook
  local target_repo
  local target_common_dir

  raw_target="$(readlink "$HOOK_DST" 2>/dev/null || true)"
  resolved_hook="$(readlink -f "$HOOK_DST" 2>/dev/null || true)"
  if [[ -z "$resolved_hook" ]]; then
    is_legacy_managed_target_path "$raw_target"
    return $?
  fi

  is_legacy_managed_target_path "$resolved_hook" || return 1

  target_repo="$(cd "$(dirname "$resolved_hook")/.." && pwd)"
  git -C "$target_repo" rev-parse --is-inside-work-tree >/dev/null 2>&1 || return 1

  target_common_dir="$(git -C "$target_repo" rev-parse --git-common-dir)"
  target_common_dir="$(normalize_path "$target_repo" "$target_common_dir")"

  [[ "$target_common_dir" == "$CURRENT_COMMON_DIR" ]]
}

is_current_managed_wrapper() {
  [[ -x "$HOOK_DST" ]] || return 1
  has_managed_marker || return 1
  grep -Fqx "$EXPECTED_ROOT_LINE" "$HOOK_DST" || return 1
  grep -Fqx "$EXPECTED_EXEC_LINE" "$HOOK_DST"
}

status_message() {
  if is_current_managed_wrapper; then
    echo "[pre-commit] Managed hook is installed -> $HOOK_DST"
    return 0
  fi

  if is_legacy_managed_hook; then
    echo "[pre-commit] Legacy managed hook detected at $HOOK_DST; run 'pixi run init' to migrate it." >&2
    return 1
  fi

  if has_managed_marker; then
    echo "[pre-commit] Stale managed hook detected at $HOOK_DST; run 'pixi run init' to repair it." >&2
    return 1
  fi

  if [[ -e "$HOOK_DST" ]]; then
    echo "[pre-commit] Unmanaged hook detected at $HOOK_DST." >&2
  else
    echo "[pre-commit] Managed hook is not installed at $HOOK_DST." >&2
  fi

  return 1
}

write_managed_hook() {
  local tmp_hook

  mkdir -p "$HOOKS_DIR"
  tmp_hook="$(mktemp "${HOOK_DST}.tmp.XXXXXX")"

  cat >"$tmp_hook" <<'EOF'
#!/usr/bin/env bash
# goggles-managed-pre-commit-hook v2
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
exec "$repo_root/scripts/pre-commit-format.sh" "$@"
EOF

  chmod +x "$HOOK_SRC" "$tmp_hook"
  mv -f "$tmp_hook" "$HOOK_DST"
}

backup_unmanaged_hook() {
  local backup_path

  backup_path="${HOOK_DST}.bak.$(date +%Y%m%d%H%M%S)"
  mv "$HOOK_DST" "$backup_path"
  echo "[pre-commit] Backed up existing hook -> $backup_path"
}

if [[ "$MODE" == "check" ]]; then
  if status_message; then
    exit 0
  fi

  exit 1
fi

if is_current_managed_wrapper; then
  echo "[pre-commit] Managed hook already installed -> $HOOK_DST"
  exit 0
fi

action="Installed"

if [[ -L "$HOOK_DST" ]]; then
  if ! is_legacy_managed_hook; then
    if ((FORCE == 0)); then
      echo "[pre-commit] Existing hook symlink is not managed; leave it unchanged: $HOOK_DST" >&2
      exit 1
    fi

    backup_unmanaged_hook
    action="Replaced"
  else
    action="Migrated"
  fi
elif has_managed_marker; then
  action="Repaired"
elif [[ -e "$HOOK_DST" ]]; then
  if ((FORCE == 0)); then
    echo "[pre-commit] Existing hook not managed; leave it unchanged: $HOOK_DST" >&2
    exit 1
  fi

  backup_unmanaged_hook
  action="Replaced"
fi

write_managed_hook
echo "[pre-commit] ${action} managed hook -> $HOOK_DST"
