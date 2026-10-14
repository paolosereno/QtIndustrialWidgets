/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace QtIndustrialWidgets {
namespace internal {

/// Geometry of the RadialGauge scale, shared by the widget and its tests.
struct RadialScaleGeometry {
    double radius{0.0};      ///< Outer dial radius: side / 2 - 4
    double arcRadius{0.0};   ///< Radius of the threshold arc
    double arcWidth{0.0};    ///< Width of the threshold arc
    double tickOuter{0.0};   ///< Outer end of every tick
    double majorTickInner{0.0};
    double minorTickInner{0.0};
    double labelRadius{0.0}; ///< Distance of the scale label centres from the dial centre
    int labelFontPx{7};      ///< Pixel size of the (bold) scale label font
};

/// Scale geometry of a dial of the given side (the smaller widget dimension).
RadialScaleGeometry radialScaleGeometry(double side);

} // namespace internal
} // namespace QtIndustrialWidgets
