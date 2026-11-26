/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QRectF>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief The rasterization margin (#226), in logical px, on each side of the ink box of a text.
 *
 * The glyphs are drawn beyond the tight box that QFontMetricsF reports: by up to about 0.8 logical px per side with
 * CoreText (macOS) and 0.5 px with the Windows rasterizer, measured in the CI probe of #226. A widget reserves this
 * margin around the measured ink only in a container that must hold that ink (a readout pod, a flag), in the search of
 * its minimum and in the point size it chooses at every size; never in the gaps between labels, which are measured on
 * the metrics of Qt (docs/widgets.md).
 */
inline constexpr double kRasterMargin = 1.0;

/// \a ink with the rasterization margin on each side; an empty box (nothing drawn) stays empty.
inline QRectF withRasterMargin(const QRectF &ink)
{
    return ink.isEmpty() ? ink : ink.adjusted(-kRasterMargin, -kRasterMargin, kRasterMargin, kRasterMargin);
}

} // namespace internal
} // namespace QtIndustrialWidgets
