# Working on GPAC

GPAC is a C99 multimedia framework centered on `libgpac` and its filter engine.
`MP4Box` provides media inspection, import, extraction, and packaging commands;
`gpac` exposes filter pipelines. The tools and applications embedding `libgpac`
share parsing, filter, and ownership code, so trace a change across those layers.

## Find the right code

- `applications/mp4box/` and `applications/gpac/` contain command-line entry
  points. Start from the command that exhibits a problem when one exists.
- `src/isomedia/` handles ISO BMFF boxes, tracks, samples, items, and fragments.
  Public APIs are in `include/gpac/`; `include/gpac/internal/` holds internal headers.
- `src/filters/` contains input, output, and processing filters. For new
  filters, consult `src/filters/base_filter_example.c` and `include/gpac/filters.h`.
  `src/filter_core/` resolves and runs filter graphs and manages PIDs and
  packets. A PID connects filters, carries packets and synchronized properties,
  and can fan out; input and output PIDs have different lifecycle rules.
- `src/media_tools/` implements media import, DASH and MPD handling,
  segmentation, and codec utilities. `src/utils/` holds shared bitstream,
  downloader, allocation, threading, and other support code.
- `modules/` holds optional loadable modules; built-in filters live in `src/filters/`.
- `unittests/` is the unit-test harness; component tests live in
  `src/*/unittests/`. `testsuite/` is a separate Git submodule with command-line
  tests and media fixtures.
- `bin/gcc/`, configured build files, and `unittests/build/` are generated. Keep them out of patches.

## Make and review changes

- Reproduce a bug on current `master` before patching when possible. Record the
  input, exact command or API sequence, build configuration, observed result,
  and expected result. A library-only reproducer should include complete build
  and run commands and preserve relevant caller preconditions.
- Follow the surrounding C code's `GF_Err` and `GF_*` error conventions,
  `GF_LOG`, and `gf_*` allocation APIs. Check sizes, ownership, callbacks,
  cleanup, and state restoration across callers and error paths. A disappearing
  crash alone does not establish that the underlying state is sound.
- Keep fixes focused and add a regression test near the affected component or
  in `testsuite/`. Rerun the trigger and representative valid inputs before and
  after the change. Avoid unrelated refactors, formatting churn, and public API
  changes in a bug fix.
- For filter scheduling or packet lifetime changes, explain the synchronization
  and ownership invariant. Exercise concurrent processing and teardown, and
  identify configurations or architectures that were not tested. These changes
  need review from someone familiar with the core.
- Search existing issues and PRs before reporting or duplicating a fix. Keep
  each commit to one logical change and describe the behavior, tests run, and
  remaining limitations in plain language.

## Build and test

Configure options select optional dependencies and filters; record the resulting
feature set when reproducing a problem. From a clean checkout:

```sh
./configure --unittests
make -j2
make unit_tests
```

`make` runs the enabled unit tests; `make unit_tests` reruns them. For a memory
error investigation, add `--enable-sanitizer` to `./configure` in a separate
build. Use that build's `bin/gcc/MP4Box` and `bin/gcc/gpac`, rather than a
system-installed copy. See [the unit-test guide](unittests/README.md) for the
harness and component test patterns. For tests of private functions, follow its
`GF_STATIC`, `GF_NOT_EXPORTED`, and filter-source inclusion patterns instead of
exporting an API solely for testing.

For a targeted command-line test, initialize the pinned submodule and run from
its directory. Replace `NAME` and the script path with the affected test;
`-re` clears that script's cached results before rerunning the named test:

```sh
git submodule update --init testsuite
cd testsuite
PATH="$(pwd)/../bin/gcc:$PATH" ./make_tests.sh -re -test=NAME -p=0 scripts/affected.sh
```

Follow the [testsuite guide](https://wiki.gpac.io/Build/tests/Using-the-testsuite/)
for platform prerequisites, fixtures, and other test modes. Report the
platforms and configurations actually checked.

## Security reports

Read [the threat model](THREAT_MODEL.md) before assessing impact. Confirm
security findings on the current `master` HEAD before reporting, and follow
[SECURITY.md](SECURITY.md) and the
[issue template](.github/ISSUE_TEMPLATE/bug_report.md) for executable steps,
sample input, and AI-assistance disclosure. If public disclosure is unreasonable
or confidential material must be shared, use the policy's `security@gpac.io`
contact.
