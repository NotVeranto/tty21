#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

./tty21 --help >"$tmp_dir/help"
grep -F "Requires a terminal of at least 70x42." "$tmp_dir/help" >/dev/null

./tty21 --version >"$tmp_dir/version"
grep -F "tty21 " "$tmp_dir/version" >/dev/null

if ./tty21 --seed invalid >"$tmp_dir/invalid-seed" 2>&1; then
    echo "invalid seed unexpectedly succeeded" >&2
    exit 1
fi
grep -F "error: invalid seed" "$tmp_dir/invalid-seed" >/dev/null

if ./tty21 --unknown-option >"$tmp_dir/unknown-option" 2>&1; then
    echo "unknown option unexpectedly succeeded" >&2
    exit 1
fi
grep -F "error: unknown option" "$tmp_dir/unknown-option" >/dev/null

if HOME="$tmp_dir" XDG_DATA_HOME="$tmp_dir/data" \
    ./tty21 --continue >"$tmp_dir/missing-save" 2>&1; then
    echo "--continue unexpectedly succeeded without a save" >&2
    exit 1
fi
grep -F "error: no save to continue." "$tmp_dir/missing-save" >/dev/null

echo "CLI smoke tests passed"
