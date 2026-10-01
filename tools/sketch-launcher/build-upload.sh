#!/bin/sh
if ! command -v node >/dev/null 2>&1; then
  echo 'Node.js is missing. Install it from https://nodejs.org/en/download' >&2
  exit 1
fi
exec node "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/loom-build.cjs" "$@"
