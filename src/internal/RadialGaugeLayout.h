/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/// Axis-aligned rectangle in widget coordinates (y grows downwards).
struct LayoutRect {
    double x{0.0};
    double y{0.0};
    double w{0.0};
    double h{0.0};

    [[nodiscard]] double right() const { return x + w; }
    [[nodiscard]] double bottom() const { return y + h; }
    /// True when the interiors overlap (touching edges do not count).
    [[nodiscard]] bool intersects(const LayoutRect &o) const
    {
        return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom();
    }
};

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

/// Input of layoutDigitalPod().
struct PodLayoutInput {
    double centerX{0.0};
    double centerY{0.0};
    double radius{0.0};
    std::vector<LayoutRect> labelRects; ///< Bounding rectangles of the drawn scale labels
    double unitWidth{0.0};              ///< Width of the unit text (0 when there is no unit)
    double padding{0.0};                ///< Horizontal space kept between the value and each pod side
    int preferredFontPx{8};             ///< Largest value font
    int minimumFontPx{8};               ///< Smallest value font
    std::function<double(int)> valueWidth;  ///< Width of the widest value text at a pixel size
    std::function<double(int)> valueHeight; ///< Height of the value text at a pixel size
};

/// Result of layoutDigitalPod().
struct PodLayout {
    LayoutRect pod;            ///< Digital display pod
    LayoutRect unit;           ///< Band above the pod where the unit text is centred
    int valueFontPx{0};        ///< Pixel size of the value font
    bool lowered{false};       ///< The pod is below the labels instead of at its preferred place
    bool clearOfLabels{false}; ///< The pod and the unit band keep the margin from every label
};

/// Free space kept between the pod (or the unit) and a scale label: max(2 px, 0.02 r).
double podLabelMargin(double radius);

/**
 * \brief Places the RadialGauge digital display pod clear of the scale labels and picks the value font.
 *
 * The pod is centred horizontally on the dial and is max(0.28 r, value height at the minimum
 * font) high. Two places are considered:
 * - the preferred place, the one used before #43: top at 0.28 r below the centre;
 * - the lowered place: top just below the lowest label that lies under the centre within the
 *   preferred width (0.68 r), when there is such a label.
 *
 * At each place the pod is at most 0.68 r wide and is narrowed so that it keeps podLabelMargin()
 * from every label rectangle that overlaps its band vertically and stays inside the inner dial
 * (0.95 r); the unit band, 0.14 r high and directly above the pod, must leave room for
 * \a unitWidth in the same way. The value font at a place is the largest size in
 * [minimumFontPx, preferredFontPx] at which the widest value fits with \a padding on each side
 * (fitFontPixelSize()). The place giving the larger font is used; on a tie, the preferred place.
 *
 * The value has precedence over the labels: if the value fits at neither place even at the
 * minimum font, the wider place is used (the preferred one on a tie) and the pod is widened to
 * hold the value at the minimum font (clearOfLabels = false), so that the value is never clipped
 * or abbreviated. Non-finite inputs or missing text metrics give an empty pod.
 */
PodLayout layoutDigitalPod(const PodLayoutInput &in);

/// Result of fitFontPixelSize().
struct FontFit {
    int pixelSize{0};
    bool fits{false};
};

/**
 * \brief Largest pixel size in [\a minimum, \a preferred] whose text fits in \a maxWidth x \a maxHeight.
 *
 * \a textWidth returns the width of the widest text at a pixel size, \a textHeight its height.
 * If no size fits, the result is \a minimum with fits = false.
 */
FontFit fitFontPixelSize(int preferred, int minimum, double maxWidth, double maxHeight,
                         const std::function<double(int)> &textWidth,
                         const std::function<double(int)> &textHeight);

} // namespace internal
} // namespace QtIndustrialWidgets
