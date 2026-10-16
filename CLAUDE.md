<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# QtIndustrialWidgets — instructions for Claude Code

C++17 widget library for industrial instrumentation (gauges, strip chart, annunciator, …) built on Qt Widgets.
Supported: **Qt 6.2+**, Linux / Windows / macOS (Qt 5.15 is not supported at the moment, see #53). The library is
being turned into a commercial product: correctness, honest documentation and traceable changes matter more than
speed of delivery.

## Commands

```bash
# Configure (Release, tests on; enable what you need)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=OFF

cmake --build build

# Full test suite (includes tst_StripChart_hidpi, which runs with QT_SCALE_FACTOR=2)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

# One test binary, verbose (shows QINFO / XFAIL / SKIP lines)
QT_QPA_PLATFORM=offscreen ./build/bin/tst_StripChart -v2

# Benchmarks (only when the task is about performance)
cmake -B build -DBUILD_BENCHMARKS=ON && cmake --build build
QT_QPA_PLATFORM=offscreen ./build/bin/bench_StripChart

# Documentation images (docs/images/, DPR 2, dark theme; see docs/gallery_images.md)
./build/bin/QtIndustrialWidgetsGallery --export-gallery docs/images

# Licence compliance (CI fails without it)
reuse lint
```

A change is "done" only when: build has zero warnings, the full `ctest` run passes (normal + HiDPI), and `reuse lint` passes.

## Code conventions

- Everything lives in `namespace QtIndustrialWidgets` (alias `qiw`). **No `Q` prefix** on class names.
- Public headers: `include/QtIndustrialWidgets/`. Internal code: `src/internal/` (never installed, never exported).
- **d-pointer pattern**: no data members in public headers; private state goes in `*_p.h` (e.g. `src/internal/StripChart_p.h`).
  Public headers must stay ABI-stable within a major version.
- Shared library is built with hidden visibility; export only through `QTINDUSTRIALWIDGETS_EXPORT`.
- Zero warnings with `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4 /permissive-` (MSVC). No Qt-6-only APIs.
<!-- REUSE-IgnoreStart -->
- Every new file has the SPDX header (`SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>`,
  `SPDX-License-Identifier: MIT`). Files that cannot hold comments (JSON, images) get a `<file>.license` sidecar.
<!-- REUSE-IgnoreEnd -->
- Non-finite input (NaN, ±Inf) must never reach `QPainter` coordinates and must never corrupt ranges.

## Working rules (non-negotiable)

1. **Test first for bugs.** A regression test must be written and shown to **fail** on the unmodified code before
   the fix. Report the failing output.
2. **Never hide a failure.** Do not add `QEXPECT_FAIL`, `QSKIP`, special cases keyed to current output, or wider
   tolerances to make a test pass. If a test exposes a defect or a wrong assumption, **stop and report it** with a
   minimal reproduction. Only the user can approve an expected failure.
3. **Tests check behaviour.** "It rendered and the pixmap is not null" is not a test. Assert vertices, segments,
   windows, labels or pixels that would change if the feature were broken. For new tests, show that a deliberate
   sabotage of the feature makes them fail (do not commit the sabotage).
4. **No public API change** (additions, removals, signature or semantic changes) without explicit approval.
   Behaviour changes that users can see must be listed in the CHANGELOG under **Changed**.
5. **Never move a target.** If a performance or quality target is missed, report the measured number and the
   cause (profile). Do not lower, reword or drop the target.
6. **Stay in scope.** Do only what the issue asks. Anything else you notice (bugs, smells, doc errors) goes into
   the final report as a suggestion, not into the diff.
7. **When the spec is wrong or ambiguous, ask.** Do not improvise an alternative design.
8. After a mutation check, restore the source (`git checkout HEAD -- <files>`), rebuild from clean with `cmake --build build --clean-first` and run the full `ctest` until green before taking any measurement or image. Touching the restored files is not enough: under WSL the clock can jump back, so make may see a mutated binary as newer than the restored sources and skip the relink.

## Documentation, benchmarks and releases

- Public claims (README, docs, CHANGELOG) must be **verifiable**. Forbidden without a measurement behind them:
  "zero-aliasing", "zero-allocation", "60 FPS", "full backward compatibility", "N× faster".
- Performance numbers come from **one** benchmark run, published in `docs/benchmarks.md` with machine (CPU, OS,
  compiler, Qt version), resolution and exact command line. README and CHANGELOG quote that table; they never
  carry their own numbers. No speed-up factors against baselines that were not re-measured in the same run.
- `bench_StripChart` must be run with the existing data unchanged before and after a performance change.
- Version bumps follow SemVer and touch **all four** places: `CMakeLists.txt` (`project VERSION`),
  `include/QtIndustrialWidgets/qtindustrialwidgets_global.h`, `conanfile.py`, `ports/qtindustrialwidgets/vcpkg.json`.
  Grep for the old version string afterwards (README `GIT_TAG`, package names in docs).
- `CHANGELOG.md` follows Keep a Changelog: Added / Changed / Deprecated / Removed / Fixed / Performance / Tests.
- Do not bump the version or tag a release unless the issue asks for it.

## Workflow with GitHub issues

1. Read the task: `gh issue view <n>` (include comments: `gh issue view <n> --comments`).
2. Before writing code, post a **short plan**: files to touch, tests to add, risks, open questions. Wait for approval
   on anything that changes public API, behaviour, or scope.
3. Work on a branch `issue-<n>-<short-slug>`. One commit per logical step, Conventional Commits
   (`fix(stripchart): …`, `test(decimator): …`, `perf(…)`, `docs(…)`, `chore(…)`).
4. Open a PR with `Fixes #<n>` and a report containing:
   - failing test output before the fix and passing output after;
   - full `ctest` result (normal + HiDPI) and `reuse lint` result;
   - benchmark table if performance was touched;
   - any deviation from the issue and why; anything noticed out of scope.
5. When the work is done and verified, push the issue branch and open the PR yourself with `Fixes #<n>` and the
   full report as body. Never merge: the maintainer reviews and merges.
6. Screenshots for a PR go on the `pr-assets` branch (worktree `../qiw-pr-assets`), in `pr<number>/`, as compressed
   PNG; reference them in the PR body via
   `https://raw.githubusercontent.com/paolosereno/QtIndustrialWidgets/pr-assets/pr<number>/<file>.png`.
   Never merge `pr-assets` into `main`.
7. Never push to `main`, never force-push, never merge your own PR.
8. You may open issues (`gh issue create`), edit them (`gh issue edit`) and comment on them (`gh issue comment`)
   when the maintainer asks, or to post the plan of point 2. Issue texts are in English; show the text to the
   maintainer before publishing unless told to publish directly. Never close, delete or transfer issues, and never
   change their labels or assignees unless asked.
9. Work every issue in its own git worktree, never in the main checkout:
   `git worktree add ../qiw-issue-<n> -b issue-<n>-<short-slug> origin/main`. Build, test and export images inside
   that worktree (its own `build/`). Never commit or switch branches in the main checkout, which the maintainer also
   uses. Remove the worktree (`git worktree remove ../qiw-issue-<n>`) after the PR is merged.

## Map of the code worth knowing

- `src/StripChart.cpp` + `src/internal/StripChart_p.h`: strip chart widget, insertion APIs, paint.
- `src/internal/M4Decimator.*`: M4 min/max decimation (batch + `IncrementalStream`).
- `src/internal/StripChartGeometry.*`: single source of truth for time window / bucket geometry
  (`computeTimeWindow`, `computeIndexBuckets`). Do not recompute windows elsewhere.
- `tests/StripChartTestAccess.h`, `tests/StripChartTestUtils.h`: test hook (`FrameDiagnostics`) and pixel helpers.
- `benchmarks/bench_StripChart.cpp`, `docs/benchmarks.md`: performance measurements.