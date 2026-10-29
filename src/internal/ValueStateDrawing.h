/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "internal/ValueClassification.h"

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPainterPath>
#include <QtCore/QRectF>
#include <QtCore/QString>

class QPainter;

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Draws the readout \a text of an out-of-range or invalid input centred in \a podRect, #151.
 *
 * Overrange and Underrange: a triangle pointing up or down, painted as a filled polygon (not a font character)
 * in \a color, before the text and centred on the digits. The pod is sized for the valid values, so \a font
 * (with its pixel size set) is made smaller, down to 6 px, until the triangle and the text fit the width of
 * \a podRect less a margin of max(2, 0.12 × its shorter side) on each side.
 */
void drawStateReadout(QPainter &painter, const QRectF &podRect, QFont font, const QString &text, InputState state,
                      const QColor &color);

/**
 * \brief Hatches \a area with diagonal lines in \a color, #151: the mark of an invalid (NaN) input in a bar.
 *
 * The lines are 1.2 px wide, spaced by max(4, 0.45 × \a thickness) px, where \a thickness is the width of the
 * bar across its length, so that the pattern stays visible on thin bars.
 */
void drawInvalidHatch(QPainter &painter, const QPainterPath &area, double thickness, const QColor &color);

} // namespace internal
} // namespace QtIndustrialWidgets
