/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
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

/// Geometry of the IndustrialKnob scale, shared by the widget and its tests.
struct KnobScaleGeometry {
    double centerX{0.0};     ///< Dial centre
    double centerY{0.0};
    double radius{0.0};      ///< Dial radius: side / 2 - 4, with side = min(w, 0.82 h)
    double scaleRadius{0.0}; ///< Outer end of every tick
    double labelRadius{0.0}; ///< Distance of the scale label centres from the dial centre
    int labelFontPx{7};      ///< Pixel size of the (bold) scale label font
};

/// Scale geometry of a knob of the given widget size, at the full radius.
KnobScaleGeometry knobScaleGeometry(double width, double height);

/// Boxes of the labels a knob draws at a geometry, after the selection of layoutRadialScaleLabels().
using KnobDrawnLabels = std::function<std::vector<LayoutRect>(const KnobScaleGeometry &)>;

inline constexpr double kKnobEdgeMargin = 1.0;         ///< Free space between a label box and the widget edge
inline constexpr double kKnobMinimumRadiusRatio = 0.5; ///< Smallest dial, as a fraction of the full radius

/**
 * \brief Scale geometry of a knob whose drawn labels stay inside the widget (#170).
 *
 * The largest radius, from the full one down to half of it, at which every box of \a drawnLabels keeps
 * kKnobEdgeMargin from the widget edge. The centre does not move. At the smallest radius labels can still
 * reach the edge: minimumSizeHint() keeps the widget above that.
 */
KnobScaleGeometry knobScaleGeometry(double width, double height, const KnobDrawnLabels &drawnLabels);

/// Box of a w x h scale label of a knob, centred at the label radius, at \a angle degrees (0 = 12 o'clock, clockwise).
LayoutRect knobLabelBox(const KnobScaleGeometry &geometry, double angle, double w, double h);

/**
 * \brief Free space between two rectangles along the axis that separates them.
 *
 * The larger of the horizontal and the vertical distance between the edges; negative when the
 * rectangles overlap. Non-finite inputs give -infinity (treated as overlapping).
 */
double layoutRectGap(const LayoutRect &a, const LayoutRect &b);

/// Result of layoutRadialScaleLabels().
struct RadialScaleLabels {
    int step{0};            ///< Every step-th major label is drawn (0 when there are none)
    std::vector<int> drawn; ///< Major-tick indices of the drawn labels, in increasing order
};

/**
 * \brief Chooses which major labels a RadialGauge scale draws (#50).
 *
 * \a boxes holds the rectangle of every major label (majorTicks + 1 of them, centred on their
 * label positions). The step is the smallest divisor k of majorTicks for which every pair of drawn
 * labels (indices 0, k, 2k, ..., majorTicks), not only neighbouring ones, keeps at least \a minGap
 * between them (layoutRectGap()). If no smaller divisor fits, the step is majorTicks: the first
 * and last labels are drawn anyway. When the first and last labels themselves are closer than
 * \a minGap (a span of 360 degrees or more), the last one is never drawn and the others are
 * chosen in the same way. With majorTicks = 0 the single label is drawn with step 1; with no
 * boxes nothing is drawn. A non-finite \a minGap is treated as 0.
 */
RadialScaleLabels layoutRadialScaleLabels(const std::vector<LayoutRect> &boxes, double minGap);

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
    double indicatorWidth{0.0};         ///< Width of the state indicator ("---", triangle) at the minimum font
    double scaleInnerRadius{0.0};       ///< Inner radius of the scale ticks and band (0: no scale to avoid)
    double scaleStartAngle{0.0};        ///< Start of the scale arc, degrees, 0 = 12 o'clock, clockwise
    double scaleSpanAngle{360.0};       ///< Span of the scale arc, degrees
};

/// Result of layoutDigitalPod().
struct PodLayout {
    LayoutRect pod;            ///< Digital display pod
    LayoutRect unit;           ///< Band above the pod where the unit text is centred
    int valueFontPx{0};        ///< Pixel size of the value font
    bool lowered{false};       ///< The pod is below the labels instead of at its preferred place
    bool clearOfLabels{false}; ///< The pod and the unit band keep the margin from every label
    bool showUnit{true};       ///< The unit fits; otherwise it is left out
    bool showValue{true};      ///< The number fits; otherwise only the state indicator is drawn
    bool clearOfScale{true};   ///< The pod stays off the scale; false only when not even the indicator has room
    std::vector<int> hiddenLabels; ///< Indices in labelRects of the labels the pod would cut: not drawn (#60)
};

/// Smallest pixel size of a readout: 7 pt at the given logical DPI, rounded up (#60, the rule of #167).
int minimumReadoutPixelSize(double logicalDpi);

/// Free space kept between the pod (or the unit) and a scale label: max(2 px, 0.02 r).
double podLabelMargin(double radius);

/// True when the box of half width \a half between \a y0 and \a y1, centred on the dial, reaches the scale: the
/// ticks and the threshold band, or the ticks at the ends of the arc.
bool podReachesScale(const PodLayoutInput &in, double half, double y0, double y1);

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
 * Neither the pod nor the unit band may reach the ticks and the threshold band: where the arc of
 * the scale (\a scaleStartAngle, \a scaleSpanAngle) lies, they stay inside \a scaleInnerRadius. Below
 * and beside the arc the limit is the inner dial, so the gap of a 270-degree scale stays usable (#60).
 *
 * When the value does not fit at the minimum font, the following are tried in this order (#60):
 * - a compact pod, as high as the value line, clear of the labels, at the largest font that fits;
 * - the same at the minimum font, with the labels it would cut in \a hiddenLabels: the value has
 *   precedence over the labels (#43; clearOfLabels = false);
 * - without the unit (showUnit = false);
 * - only the state indicator, clear of the labels if it can (showValue = false, showUnit = false);
 * - the same over the labels; with no room there either, over the scale (clearOfScale = false): the
 *   state indicator is never left out.
 *
 * The value is never clipped or abbreviated, and the font is never below \a minimumFontPx.
 * Non-finite inputs or missing text metrics give an empty pod.
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
