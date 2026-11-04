#!/bin/sh
studio_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
exec node "$studio_dir/sign-in.mjs" "$@"
