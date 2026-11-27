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

if ./tty21 --seed=invalid >"$tmp_dir/invalid-seed-equals" 2>&1; then
    echo "--seed=invalid unexpectedly succeeded" >&2
    exit 1
fi
grep -F "error: invalid seed" "$tmp_dir/invalid-seed-equals" >/dev/null

if ./tty21 -sinvalid >"$tmp_dir/invalid-seed-short" 2>&1; then
    echo "-sinvalid unexpectedly succeeded" >&2
    exit 1
fi
grep -F "error: invalid seed" "$tmp_dir/invalid-seed-short" >/dev/null

if ./tty21 --seed >"$tmp_dir/missing-seed" 2>&1; then
    echo "--seed unexpectedly succeeded without a value" >&2
    exit 1
fi
grep -F "error: missing seed value" "$tmp_dir/missing-seed" >/dev/null

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

# Session flags and a seed are accepted together with --continue.
if HOME="$tmp_dir" XDG_DATA_HOME="$tmp_dir/data" \
    ./tty21 --continue --seed=12345 --fast --no-save >"$tmp_dir/combined-flags" 2>&1; then
    echo "--continue unexpectedly succeeded without a save" >&2
    exit 1
fi
grep -F "error: no save to continue." "$tmp_dir/combined-flags" >/dev/null

# A continue request cannot be paired with either new-game mode.
for mode in --classic --roguelike; do
    if ./tty21 --continue "$mode" >"$tmp_dir/conflicting-flags" 2>&1; then
        echo "--continue unexpectedly accepted $mode" >&2
        exit 1
    fi
done
grep -F "error: --continue cannot be combined" "$tmp_dir/conflicting-flags" >/dev/null

# Explicit new-game modes are mutually exclusive in either argument order.
for modes in "--classic --roguelike" "--roguelike --classic"; do
    if ./tty21 $modes >"$tmp_dir/conflicting-modes" 2>&1; then
        echo "conflicting game modes unexpectedly succeeded: $modes" >&2
        exit 1
    fi
grep -F "error: --roguelike and --classic cannot be combined" "$tmp_dir/conflicting-modes" >/dev/null
done

echo "CLI smoke tests passed"
