#!/bin/sh
set -eu

if [ "${1:-}" = --list ]; then
    shift
    #a marker is emitted by tests.h only when preprocessing for discovery
    #comments and declarations in inactive preprocessor branches are absent
    sed -n 's/^[[:space:]]*GPAC_UNIT_TEST_ENTRY[[:space:]]*([[:space:]]*\([A-Za-z_][A-Za-z_0-9]*\)[[:space:]]*).*/\1/p' "$@"
    exit
fi

printf '#include "tests.h"\n\n'
for list in "$@"; do
    while IFS= read -r name; do
        printf 'void test_%s(void);\n' "$name"
    done < "$list"
done
printf '\nint main(int argc, char **argv)\n{\n'
for list in "$@"; do
    while IFS= read -r name; do
        printf '    if (register_test("%s", test_%s)) return EXIT_FAILURE;\n' "$name" "$name"
    done < "$list"
done
printf '    return run_tests(argc, argv);\n}\n'
