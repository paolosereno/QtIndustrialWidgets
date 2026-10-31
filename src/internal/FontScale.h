/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QString>
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

/// Ink of a text, in logical pixels from its baseline (negative above it).
struct TextInk {
    double top{0.0};
    double bottom{0.0};
};

/**
 * \brief Vertical extent of the pixels a text really draws at a device pixel ratio.
 *
 * Measured on a raster, since the font metrics do not tell: tightBoundingRect() goes up to 1 px past
 * the ink with DejaVu Sans, falls 1.0 px short of it on macOS (#37, cause not determined), and
 * without installed fonts is much larger than the box glyphs. A text without ink gives 0, 0.
 */
TextInk measureTextInk(const QString &text, const QFont &font, double dpr);

/// Ink of a text around the centre of its rectangle, in logical pixels (negative to the left of it and above it).
struct TextInkBox {
    double left{0.0};
    double top{0.0};
    double right{0.0};
    double bottom{0.0};
};

/**
 * \brief Pixels a text really draws in a width x height rectangle with Qt::AlignCenter, centred on a device pixel.
 *
 * In both directions, for the same reason as measureTextInk(), and at the antialiasing of a label. With the rectangle
 * on a device pixel the text is drawn as measured. A text without ink gives an empty box at the centre.
 */
TextInkBox measureCenteredTextInk(const QString &text, const QFont &font, double width, double height, double dpr);

} // namespace internal
} // namespace QtIndustrialWidgets
