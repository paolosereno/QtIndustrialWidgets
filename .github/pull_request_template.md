## Linked issue

Fixes #

## What changes

<!-- What the pull request changes and why. Mention any deviation from the issue. -->

## Tests

<!-- Tests added or changed. For a bug: the test output on the unmodified code (it must fail) and after the fix.
     For new tests: the deliberate sabotage of the feature that makes them fail (not committed). -->

## Checklist (see CONTRIBUTING.md)

- [ ] The build has zero warnings
- [ ] The full `ctest` passes at device pixel ratios 1, 1.25, 1.5 and 2 (`QT_QPA_PLATFORM=offscreen ctest --test-dir build`)
- [ ] `reuse lint` passes
- [ ] If the change alters what the gallery draws, `docs/images` is re-exported (`--export-gallery docs/images`, see `docs/gallery_images.md`; CI checks it)
- [ ] User-visible changes are listed in `CHANGELOG.md`
