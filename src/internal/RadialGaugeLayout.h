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
    double minPodWidth{0.0};            ///< Width the pod needs for the widest value at the smallest value font
    double minPodHeight{0.0};           ///< Height the pod needs for the value at the smallest value font
    double unitWidth{0.0};              ///< Width of the unit text (0 when there is no unit)
};

/// Result of layoutDigitalPod().
struct PodLayout {
    LayoutRect pod;     ///< Digital display pod
    LayoutRect unit;    ///< Band above the pod where the unit text is centred
    bool lowered{false};       ///< The pod was moved below the labels that block it
    bool clearOfLabels{false}; ///< The pod and the unit band keep the margin from every label
};

/// Free space kept between the pod (or the unit) and a scale label: max(2 px, 0.02 r).
double podLabelMargin(double radius);

/**
 * \brief Places the RadialGauge digital display pod clear of the scale labels.
 *
 * The pod is centred horizontally on the dial and is max(0.28 r, \a minPodHeight) high. Its
 * preferred place is the one used before #43: top at 0.28 r below the centre, 0.68 r wide. There
 * it is narrowed so that it keeps podLabelMargin() from every label rectangle that overlaps its
 * band vertically, and stays inside the inner dial (0.95 r). The unit band, 0.14 r high and
 * directly above the pod, must leave room for \a unitWidth in the same way.
 *
 * If the width is below \a minPodWidth, the pod is moved down to start just below the lowest
 * label that lies under the centre within the preferred width, and narrowed again there; the
 * lowered place is kept only if it is wider.
 *
 * The value has precedence over the labels: if neither place leaves \a minPodWidth, the pod is
 * widened to \a minPodWidth anyway (clearOfLabels = false), so that the value is never clipped
 * or abbreviated. Non-finite inputs give an empty pod.
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
