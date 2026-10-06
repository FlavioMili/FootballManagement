#!/usr/bin/env bash
# -----------------------------------------------------------------------------
#  Football Management Project
#  Smoke test for an installed or extracted game package (CI, release, local).
#
#  Launches the game headless (SDL dummy drivers) away from the source tree
#  and checks that it
#   1. is still running after FM_SMOKE_SECONDS (default 10),
#   2. shuts down cleanly on SIGTERM and reports a game data root inside the
#      package (skipped on Windows, where the process can only be killed),
#   3. created its log under <user-data-dir>, and
#   4. wrote nothing inside the package directory.
#
#  Usage: smoke_test.sh <package-dir> <executable> <user-data-dir>
#  <user-data-dir> is where the platform puts SDL_GetPrefPath, e.g.
#  $XDG_DATA_HOME/FlavioMili/FootballManagement on Linux.
# -----------------------------------------------------------------------------
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <package-dir> <executable> <user-data-dir>" >&2
  exit 2
fi

package_dir=$(cd "$1" && pwd -P)
executable="$(cd "$(dirname "$2")" && pwd -P)/$(basename "$2")"
user_data_dir=$3
seconds=${FM_SMOKE_SECONDS:-10}
work=$(mktemp -d)
pid=
cleanup() {
  if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
    kill -KILL "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  rm -rf "$work"
}
trap cleanup EXIT

fail() {
  echo "SMOKE FAIL: $*" >&2
  echo "--- game output ---" >&2
  cat "$work/output.txt" >&2 2>/dev/null
  exit 1
}

case "$(uname -s)" in
  MINGW* | MSYS* | CYGWIN*) windows=1 ;;
  *) windows=0 ;;
esac

export SDL_VIDEO_DRIVER=dummy SDL_VIDEODRIVER=dummy
export SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
unset FM_ASSET_ROOT FM_RUNTIME_ROOT FM_TEST_RUNTIME_ROOT

snapshot() {
  (
    cd "$package_dir"
    find . | LC_ALL=C sort
    if command -v sha256sum > /dev/null; then
      find . -type f -exec sha256sum {} + | LC_ALL=C sort
    else
      find . -type f -exec shasum -a 256 {} + | LC_ALL=C sort
    fi
  )
}
snapshot > "$work/before.txt"

# Start from an unrelated directory so relative paths cannot hide a bug.
(cd "$work" && exec "$executable") > "$work/output.txt" 2>&1 &
pid=$!

for ((i = 0; i < seconds; ++i)); do
  sleep 1
  kill -0 "$pid" 2>/dev/null || break
done
if ! kill -0 "$pid" 2>/dev/null; then
  status=0
  wait "$pid" || status=$?
  pid=
  fail "game exited early with status $status"
fi

if [[ $windows == 1 ]]; then
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  sleep 2 # let Windows release the executable and log file
else
  # SDL turns SIGTERM into a quit event: the game must exit on its own.
  kill -TERM "$pid"
  for ((i = 0; i < 30; ++i)); do
    kill -0 "$pid" 2>/dev/null || break
    sleep 1
  done
  if kill -0 "$pid" 2>/dev/null; then
    kill -KILL "$pid"
    fail "game did not quit within 30 s of SIGTERM"
  fi
  status=0
  wait "$pid" || status=$?
  [[ $status == 0 ]] || fail "game exited with status $status after SIGTERM"

  root_line=$(grep -m1 "Game data root: " "$work/output.txt") ||
    fail "no 'Game data root' line in the game output"
  case "$root_line" in
    *"Game data root: $package_dir"*) ;;
    *) fail "game data resolved outside the package: $root_line" ;;
  esac
  echo "$root_line"
fi

pid=

[[ -s "$user_data_dir/logs/log.txt" || $windows == 1 && -e "$user_data_dir/logs/log.txt" ]] ||
  fail "no log at $user_data_dir/logs/log.txt"

snapshot > "$work/after.txt"
if ! diff -u "$work/before.txt" "$work/after.txt" > "$work/diff.txt"; then
  cat "$work/diff.txt" >&2
  fail "the game wrote files inside the package directory"
fi

echo "SMOKE OK: $executable ($seconds s, user data in $user_data_dir)"
