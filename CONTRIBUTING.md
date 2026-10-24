<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Contributing to QtIndustrialWidgets

To report a bug or propose a feature, use the [issue forms](https://github.com/paolosereno/QtIndustrialWidgets/issues/new/choose);
for questions, use [GitHub Discussions](https://github.com/paolosereno/QtIndustrialWidgets/discussions); for security
vulnerabilities, follow [SECURITY.md](SECURITY.md).

Everyone taking part in the project follows the [Code of Conduct](CODE_OF_CONDUCT.md) (Contributor Covenant 2.1);
report unacceptable behaviour to pablitos1024@gmail.com.

Thank you for contributing. QtIndustrialWidgets is used for instrumentation, so correctness, honest documentation
and traceable changes come before speed of delivery. This page describes how changes are made and what a pull request
has to show.

## Before you start

- **Open or pick an issue** and describe your plan there (files you will touch, tests you will add, open questions)
  before writing code. Wait for the maintainer's agreement on anything that changes the public API, the behaviour
  users see, or the scope of the issue.
- **Stay in scope.** Do what the issue asks. Anything else you notice (other bugs, doc errors) goes into the pull
  request as a suggestion, not into the diff.
- **No public API change** (additions, removals, signature or semantic changes) without the maintainer's approval.
  Public headers must stay ABI-stable within a major version: no data members in public headers (d-pointer pattern),
  and symbols exported only through `QTINDUSTRIALWIDGETS_EXPORT`.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=OFF
cmake --build build

# Full test suite: every test program runs at QT_SCALE_FACTOR 1, 1.25, 1.5 and 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

# One test program, verbose
QT_QPA_PLATFORM=offscreen ./build/bin/tst_StripChart -v2

# Licence compliance
reuse lint
```

## How changes are made

### Bugs: the test comes first

Write a regression test that reproduces the bug and **show that it fails on the unmodified code** before you fix
it. Put the failing output in the pull request, next to the passing output after the fix.

### Tests check behaviour

"It rendered and the pixmap is not null" is not a test. Assert what would change if the feature were broken:
vertices, segments, windows, labels or pixels.

For every new test, **show that a deliberate sabotage of the feature makes it fail**: break the code the test covers
(for example invert a condition or skip a step), run the test, see it fail, then restore the code. Do not commit the
sabotage. After restoring, rebuild from clean (`cmake --build build --clean-first`) and run the full `ctest` before
any measurement or screenshot, so that no binary built from the sabotaged code is left behind. List the sabotages and
the tests that caught them in the pull request.

### Never hide a failure

Do not add `QEXPECT_FAIL`, `QSKIP`, special cases keyed to the current output, or wider tolerances to make a test
pass. If a test exposes a defect or a wrong assumption, report it in the issue with a minimal reproduction; only the
maintainer can approve an expected failure.

### Numbers and claims

- Statements in the README, the docs and the CHANGELOG must be verifiable in the code, the CI workflows, the release
  assets or [docs/benchmarks.md](docs/benchmarks.md). Words like "zero-allocation" or "60 FPS" need a measurement
  behind them.
- Performance numbers come from one benchmark session (interleaved runs merged with `benchmarks/merge_runs.py`; all
  runs are published) in `docs/benchmarks.md`, with machine, compiler, Qt version, resolution and exact command line. If a performance target is missed, report the measured number and its
  cause; do not lower, reword or drop the target.
- Non-finite input (NaN, ±Inf) must never reach `QPainter` coordinates or corrupt ranges.

### Code conventions

- C++17, everything in `namespace QtIndustrialWidgets` (alias `qiw`), no `Q` prefix on class names.
- Zero warnings with `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4 /permissive-` (MSVC).
- Every new file starts with an SPDX header (copyright text and the MIT licence identifier), as in the existing
  files; files that cannot hold comments get a `<file>.license` sidecar. `reuse lint` must pass.

## When a change is done

- The build has **zero warnings**.
- The **full `ctest`** passes, at all four device pixel ratios (1, 1.25, 1.5 and 2).
- **`reuse lint`** passes.
- Behaviour changes that users can see are listed in [CHANGELOG.md](CHANGELOG.md) under **Changed** (Keep a
  Changelog sections: Added, Changed, Deprecated, Removed, Fixed, Performance, Tests).

## Commits

One commit per logical step, with [Conventional Commits](https://www.conventionalcommits.org/) messages, for example
`fix(stripchart): …`, `test(decimator): …`, `perf(…)`, `docs(…)`, `chore(…)`.

## Pull requests

Reference the issue (`Fixes #<n>`) and include a report with:

- the failing test output before the fix and the passing output after it (bugs);
- the sabotages run for new tests and the tests that caught them;
- the full `ctest` result and the `reuse lint` result;
- the benchmark table, if performance was touched (`bench_StripChart` run with the existing data unchanged before and
  after the change);
- screenshots, if the rendering changed;
- any deviation from the issue and why, and anything you noticed outside its scope.

CI builds and tests every pull request on Linux, Windows and macOS. The maintainer reviews and merges.
