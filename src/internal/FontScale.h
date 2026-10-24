/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtGui/QFont>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Size rule of the widgets whose text sizes come from their geometry (#51, rule (b)).
 *
 * The ratio between the effective pixel sizes (effectivePixelSize(): QFontInfo, so that fonts set
 * with setPixelSize(), whose pointSize() is -1, work too) of the widget font and of the application font. A widget with
 * the application font gets exactly 1.0, so its texts keep their sizes. Non-positive sizes give 1.0.
 */
double fontScale(const QFont &widgetFont, const QFont &applicationFont);

/**
 * \brief Effective pixel size of a font: QFontInfo when a font is resolved, otherwise the requested
 * size (pixelSize(), or pointSizeF() at the logical DPI of the primary screen, 96 without one);
 * -1 when neither is known. Without any installed font (e.g. Windows offscreen CI) QFontInfo gives -1.
 */
double effectivePixelSize(const QFont &font);

/// A geometry-derived pixel size scaled by fontScale(): \a px itself when \a scale is 1.0, otherwise
/// the nearest integer, at least 1.
int scalePixelSize(int px, double scale);

/// A geometry-derived point size scaled by fontScale().
double scalePointSize(double pt, double scale);

} // namespace internal
} // namespace QtIndustrialWidgets
