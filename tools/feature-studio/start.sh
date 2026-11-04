#!/bin/sh
studio_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
command -v node >/dev/null 2>&1 || { echo 'Install Node.js 18 or newer from https://nodejs.org then run this again.'; exit 1; }
exec node "$studio_dir/launch.mjs" "$@"
