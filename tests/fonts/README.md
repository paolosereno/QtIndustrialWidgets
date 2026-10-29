<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Test fonts

Fonts loaded by the tests with `QFontDatabase::addApplicationFont()`, so that a test does not depend on the fonts
installed on the machine. They are used by the tests only: they are not installed and not part of the library.

## QiwReadoutTest-Bold.ttf

The characters of a gauge readout and nothing else: `0`–`9`, `.`, `-`, `<`, `>` and the space. It has no glyph for
▲ (U+25B2) or ▼ (U+25BC), so `tst_RadialGauge` can check that the out-of-range mark is painted as a shape (#151).

- **Source**: Inter 4.1 by The Inter Project Authors, `extras/ttf/Inter-Bold.ttf` from
  <https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip>
  (SHA-256 of the archive `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`, of `Inter-Bold.ttf`
  `288316099b1e0a47a4716d159098005eef7c0066921f34e3200393dbdb01947f`).
- **Licence**: SIL Open Font License 1.1 (`LICENSES/OFL-1.1.txt`); Inter declares no Reserved Font Name.
- **Changes**: subset with fontTools 4.55.0,
  `pyftsubset Inter-Bold.ttf --text="0123456789.-<> " --layout-features='' --no-hinting --desubroutinize --name-IDs='*'`,
  then renamed in the `name` table to the family "QIW Readout Test", style "Bold" (PostScript name
  `QIWReadoutTest-Bold`); the copyright, version and licence entries are kept unchanged.
