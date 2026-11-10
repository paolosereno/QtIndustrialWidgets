// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QtGui/QColor>

namespace QtIndustrialWidgets {
class CircularBargraph;
class Compass;
class IndustrialKnob;
class IndustrialSlider;
class LinearGauge;
class RadialGauge;
class TankDisplay;
class ValveIndicator;
} // namespace QtIndustrialWidgets

// Theme colours of the gallery (#190): the single source used by the live gallery (MainWindow::applyTheme()), the
// image export (Showcases.cpp) and tst_ReadoutContrast, which measures the contrast of these colours. Compiled by path
// into the gallery and into the tests, so the tests build without BUILD_EXAMPLES.
namespace GalleryShowcase {

enum class Theme { Dark, Light };

/// Background the showcase images are rendered on (the gallery's group-box colour).
QColor backgroundColor(Theme theme);

// Each function sets every colour it handles in both themes, so that a widget switched from one theme to the other
// ends with the same colours as a widget given only the second one
void applyGalleryTheme(QtIndustrialWidgets::RadialGauge &gauge, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::LinearGauge &gauge, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::IndustrialKnob &knob, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::IndustrialSlider &slider, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::Compass &compass, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::CircularBargraph &bargraph, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::TankDisplay &tank, Theme theme);
void applyGalleryTheme(QtIndustrialWidgets::ValveIndicator &valve, Theme theme);

} // namespace GalleryShowcase
