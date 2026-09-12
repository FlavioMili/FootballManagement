#!/usr/bin/env bash
# -----------------------------------------------------------------------------
#  Football Management Project
#  Prints the version a CI or release build stamps into the game.
#
#  Usage: release_version.sh [version-or-tag]
#  Without an argument (or with an empty one) this is the project version from
#  CMakeLists.txt. With a tag or version ("v1.0.0", "1.0.0-rc1") the leading v
#  is dropped and the numbers must equal the project version, so a tag can
#  never ship a build that reports a different version.
# -----------------------------------------------------------------------------
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
project=$(sed -nE 's/^project\(FootballManagement VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
  "$root/CMakeLists.txt")
if [[ -z $project ]]; then
  echo "release_version.sh: no project(FootballManagement VERSION x.y.z) in CMakeLists.txt" >&2
  exit 1
fi

requested=${1:-}
requested=${requested#v}
if [[ -z $requested ]]; then
  echo "$project"
  exit 0
fi
if [[ ! $requested =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]]; then
  echo "release_version.sh: '$requested' is not a version like 1.0.0 or 1.0.0-rc1" >&2
  exit 1
fi
if [[ ${requested%%-*} != "$project" ]]; then
  echo "release_version.sh: version $requested does not match the project version $project" \
    "(update project(... VERSION) in CMakeLists.txt or fix the tag)" >&2
  exit 1
fi
echo "$requested"
