#!/bin/sh
set -eu

#support out-of-tree builds
#we rely on libgpac_static.a
if [ -z "${GPAC_UNITTEST_BINARY:-}" ]; then
    if [ -x bin/gcc/unittests ]; then
        GPAC_UNITTEST_BINARY=./bin/gcc/unittests
    elif [ -x bin/gcc/unittests.exe ]; then
        GPAC_UNITTEST_BINARY=./bin/gcc/unittests.exe
    else
        script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
        GPAC_UNITTEST_BINARY=$script_dir/../bin/gcc/unittests
    fi
fi
exec "$GPAC_UNITTEST_BINARY" "$@"
