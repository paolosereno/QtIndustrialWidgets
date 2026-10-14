// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "RadialGaugeLayout.h"

#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

RadialScaleGeometry radialScaleGeometry(double side)
{
    RadialScaleGeometry g;
    g.radius = side * 0.5 - 4.0;
    g.arcRadius = g.radius * 0.82;
    g.arcWidth = std::max(3.0, g.radius * 0.055);
    g.tickOuter = g.arcRadius - g.arcWidth * 0.6;
    g.majorTickInner = g.tickOuter - g.radius * 0.09;
    g.minorTickInner = g.tickOuter - g.radius * 0.045;
    g.labelRadius = g.majorTickInner - g.radius * 0.12;
    g.labelFontPx = std::max(7, static_cast<int>(g.radius * 0.082));
    return g;
}

} // namespace internal
} // namespace QtIndustrialWidgets
