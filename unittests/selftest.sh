#!/bin/sh
set -eu
binary=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/gpac-unit-selftest.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

success() {
    if ! "$binary" "$@" > "$work/output" 2>&1; then
        cat "$work/output"
        echo "Expected success: $*" >&2
        exit 1
    fi
}
failure() {
    if "$binary" "$@" > "$work/output" 2>&1; then
        cat "$work/output"
        echo "Expected failure: $*" >&2
        exit 1
    else
        status=$?
        if [ "$status" != 1 ]; then
            cat "$work/output"
            echo "Expected exit status 1, got $status: $*" >&2
            exit 1
        fi
    fi
}
contains() {
    if ! grep -F "$1" "$work/output" >/dev/null; then
        cat "$work/output"
        echo "Missing output: $1" >&2
        exit 1
    fi
}
absent() {
    if grep -F "$1" "$work/output" >/dev/null; then
        cat "$work/output"
        echo "Unexpected output: $1" >&2
        exit 1
    fi
}

success --list
contains 'test[0]: pass'
# Listing does not initialize GPAC (and must not need a module/config path).
absent 'Couldn'
cp "$work/output" "$work/list"
success --only pass
contains 'Tests passed: 1'
contains 'cleanup ran'
success --only 0
contains 'Tests passed: 1'
for invalid in nonexistent -1 99999999999999999999999999 0junk ''; do
    failure --only "$invalid"
    contains 'Test not found:'
done
failure --only
contains 'Missing argument'
failure --unknown
contains 'Unknown option'
failure --only fatal
contains 'Checks failed: 1'
contains 'cleanup ran'
absent 'unreachable'
absent 'Leaked GPAC resources'
failure --only nonfatal
contains 'continued after check'
contains 'Checks failed: 1'
success --only skipped
contains 'Tests skipped: 1'
contains 'selftest feature unavailable'
contains 'Tests passed: 0'
contains 'cleanup ran'
absent 'unreachable'
success --only cleanup_order
contains 'cleanup order correct'
if [ "$(grep -c 'cleanup ran' "$work/output")" != 2 ]; then
    cat "$work/output"
    exit 1
fi
if grep -F ': leak' "$work/list" >/dev/null; then
    failure --only leak
    contains 'Leaked GPAC resources'
    failure --only file_leak
    contains 'Leaked GPAC resources'
fi
printf 'Unit-test harness checks passed.\n'
