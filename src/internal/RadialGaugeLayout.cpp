// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "RadialGaugeLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {
namespace internal {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

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

namespace {

KnobScaleGeometry knobGeometryOfRadius(double centerX, double centerY, double radius)
{
    KnobScaleGeometry g;
    g.centerX = centerX;
    g.centerY = centerY;
    g.radius = radius;
    g.scaleRadius = g.radius * 0.92;
    g.labelRadius = g.scaleRadius + g.radius * 0.12;
    g.labelFontPx = std::clamp(static_cast<int>(g.radius * 0.12), 7, 11);
    return g;
}

} // namespace

KnobScaleGeometry knobScaleGeometry(double width, double height)
{
    const double side = std::min(width, height * 0.82);
    return knobGeometryOfRadius(width * 0.5, side * 0.5 + 4.0, side * 0.5 - 4.0);
}

bool knobLabelsInside(const std::vector<LayoutRect> &boxes, double width, double height)
{
    return std::all_of(boxes.begin(), boxes.end(), [&](const LayoutRect &box) {
        return box.x >= kKnobEdgeMargin && box.y >= kKnobEdgeMargin && box.right() <= width - kKnobEdgeMargin
               && box.bottom() <= height - kKnobEdgeMargin;
    });
}

KnobScaleGeometry knobScaleGeometry(double width, double height, const KnobDrawnLabels &drawnLabels)
{
    const KnobScaleGeometry full = knobScaleGeometry(width, height);
    if (!drawnLabels) {
        return full;
    }
    if (knobLabelsInside(drawnLabels(full), width, height)) {
        return full;
    }
    // From the top: the font and the labels drawn change in steps with the radius. On multiples of 0.5 px, not on
    // steps from the full radius: the room for the end labels can be narrower than a step, and two heights of the
    // same width would then give different labels
    const double smallest = full.radius * kKnobMinimumRadiusRatio;
    for (double radius = std::floor(full.radius * 2.0) * 0.5; radius >= smallest; radius -= 0.5) {
        const KnobScaleGeometry g = knobGeometryOfRadius(full.centerX, full.centerY, radius);
        if (knobLabelsInside(drawnLabels(g), width, height)) {
            return g;
        }
    }
    return knobGeometryOfRadius(full.centerX, full.centerY, smallest);
}

LayoutRect knobLabelBox(const KnobScaleGeometry &geometry, double angle, double w, double h)
{
    constexpr double degrees = 3.14159265358979323846 / 180.0;
    const double rad = (angle - 90.0) * degrees;
    return {geometry.centerX + geometry.labelRadius * std::cos(rad) - w * 0.5,
            geometry.centerY + geometry.labelRadius * std::sin(rad) - h * 0.5, w, h};
}

int minimumReadoutPixelSize(double logicalDpi)
{
    const double dpi = std::isfinite(logicalDpi) && logicalDpi > 0.0 ? logicalDpi : 96.0;
    return static_cast<int>(std::ceil(7.0 * dpi / 72.0));
}

double layoutRectGap(const LayoutRect &a, const LayoutRect &b)
{
    const double gapX = std::max(b.x - a.right(), a.x - b.right());
    const double gapY = std::max(b.y - a.bottom(), a.y - b.bottom());
    const double gap = std::max(gapX, gapY);
    return std::isfinite(gap) ? gap : -std::numeric_limits<double>::infinity();
}

RadialScaleLabels layoutRadialScaleLabels(const std::vector<LayoutRect> &boxes, double minGap)
{
    RadialScaleLabels out;
    if (boxes.empty()) {
        return out;
    }
    const int n = static_cast<int>(boxes.size()) - 1;
    if (n == 0) {
        out.step = 1;
        out.drawn = {0};
        return out;
    }
    const double gap = std::isfinite(minGap) ? minGap : 0.0;
    auto box = [&](int i) { return boxes[static_cast<size_t>(i)]; };
    // On a span of 360 degrees or more the last label lies on (or near) the first one
    const bool endsClash = layoutRectGap(box(0), box(n)) < gap;

    // Smallest divisor of n whose labels all keep the gap; with d = n only the first label (and
    // the last one unless it clashes with it) remains, which always fits
    for (int d = 1; d <= n; ++d) {
        if (n % d != 0) {
            continue;
        }
        std::vector<int> drawn;
        for (int i = 0; i <= n; i += d) {
            if (i == n && endsClash) {
                continue;
            }
            drawn.push_back(i);
        }
        bool fits = true;
        for (size_t a = 0; a < drawn.size() && fits; ++a) {
            for (size_t b = a + 1; b < drawn.size(); ++b) {
                if (layoutRectGap(box(drawn[a]), box(drawn[b])) < gap) {
                    fits = false;
                    break;
                }
            }
        }
        if (fits || d == n) {
            out.step = d;
            out.drawn = drawn;
            return out;
        }
    }
    return out;
}

double podLabelMargin(double radius)
{
    return std::max(2.0, radius * 0.02);
}

namespace {

// True when the box of half-width half between y0 and y1 reaches the scale: a point of the box at
// scaleInnerRadius or beyond lies within the arc. The part of the box beyond the radius is bounded
// by its corners and by the crossings of the circle with its sides; the ends of the arc are checked
// too, for an arc narrower than that part.
// True if the box of half width \a half between \a top and \a bottom (relative to the centre) has a point at
// radius \a R or more inside the arc from \a start, \a span degrees long
bool boxReachesArc(double half, double top, double bottom, double R, double start, double span)
{
    auto inArc = [&](double x, double y) {
        if (span >= 360.0) {
            return true;
        }
        const double angle = std::atan2(x, -y) * 180.0 / kPi - start;
        const double wrapped = std::fmod(std::fmod(angle, 360.0) + 360.0, 360.0);
        return wrapped <= span;
    };
    auto hits = [&](double x, double y) { return std::hypot(x, y) >= R - 1e-9 && inArc(x, y); };

    for (double x : {-half, half}) {
        for (double y : {top, bottom}) {
            if (hits(x, y)) {
                return true;
            }
        }
        if (std::abs(x) < R) {
            const double y = std::sqrt(R * R - x * x);
            for (double c : {-y, y}) {
                if (c >= top && c <= bottom && hits(x, c)) {
                    return true;
                }
            }
        }
    }
    for (double y : {top, bottom}) {
        if (std::abs(y) < R) {
            const double x = std::sqrt(R * R - y * y);
            for (double c : {-x, x}) {
                if (std::abs(c) <= half && hits(c, y)) {
                    return true;
                }
            }
        }
    }
    for (double angle : {start, start + span}) {
        const double rad = angle * kPi / 180.0;
        const double x = R * std::sin(rad);
        const double y = -R * std::cos(rad);
        if (std::abs(x) <= half && y >= top && y <= bottom) {
            return true;
        }
    }
    return false;
}

} // namespace

bool podReachesScale(const PodLayoutInput &in, double half, double y0, double y1)
{
    const double R = in.scaleInnerRadius;
    if (R <= 0.0) {
        return false;
    }
    const double top = y0 - in.centerY;
    const double bottom = y1 - in.centerY;
    // The ticks at the ends of the arc have a width: the arc to avoid goes past them by the label margin, from
    // half of it beyond scaleInnerRadius, which holds their round cap and the rim of the pod (#60)
    const double m = podLabelMargin(in.radius);
    const double tickStart = R + m * 0.5;
    const double past = m / tickStart * 180.0 / kPi;
    return boxReachesArc(half, top, bottom, R, in.scaleStartAngle, in.scaleSpanAngle)
           || boxReachesArc(half, top, bottom, tickStart, in.scaleStartAngle - past, in.scaleSpanAngle + 2.0 * past);
}

namespace {

// Half-width available to a box centred on cx between y0 and y1: limited by the preferred
// half-width, by the inner dial, by the scale and, with avoidLabels, by every label that overlaps
// the band vertically.
double availableHalfWidth(const PodLayoutInput &in, double y0, double y1, double preferredHalf, bool avoidLabels = true)
{
    const double m = podLabelMargin(in.radius);
    double half = preferredHalf;

    const double innerR = in.radius * 0.95 - m;
    const double farY = std::max(std::abs(y0 - in.centerY), std::abs(y1 - in.centerY));
    if (farY >= innerR) {
        return 0.0;
    }
    half = std::min(half, std::sqrt(innerR * innerR - farY * farY));

    if (avoidLabels) {
        for (const LayoutRect &r : in.labelRects) {
            if (r.bottom() + m <= y0 || r.y - m >= y1) {
                continue; // not in the band
            }
            const double left = r.x - m;
            const double right = r.right() + m;
            if (left < in.centerX && right > in.centerX) {
                return 0.0; // a label straddles the centre line
            }
            half = std::min(half, left >= in.centerX ? left - in.centerX : in.centerX - right);
        }
    }
    half = std::max(0.0, half);
    if (podReachesScale(in, half, y0, y1)) {
        if (podReachesScale(in, 0.0, y0, y1)) {
            return 0.0;
        }
        double fits = 0.0;
        for (int i = 0; i < 16; ++i) { // the larger the box, the more of it reaches the scale
            const double middle = (fits + half) * 0.5;
            (podReachesScale(in, middle, y0, y1) ? half : fits) = middle;
        }
        half = fits;
    }
    return half;
}

} // namespace

PodLayout layoutDigitalPod(const PodLayoutInput &in)
{
    PodLayout out;
    if (!std::isfinite(in.centerX) || !std::isfinite(in.centerY) || !std::isfinite(in.radius) || in.radius <= 0.0
        || !std::isfinite(in.unitWidth) || !std::isfinite(in.padding) || !in.valueWidth || !in.valueHeight) {
        return out;
    }

    const double r = in.radius;
    const int minimumPx = std::max(1, in.minimumFontPx);
    const int preferredPx = std::max(minimumPx, in.preferredFontPx);
    const double minWidth = in.valueWidth(minimumPx) + 2.0 * in.padding;
    const double minHeight = in.valueHeight(minimumPx);
    if (!std::isfinite(minWidth) || !std::isfinite(minHeight)) {
        return out;
    }
    const double podH = std::max(r * 0.28, minHeight);
    const double unitH = r * 0.14;
    const double preferredHalf = r * 0.34;

    // Width of a pod whose top is at y, limited also by the unit text above it
    auto widthAt = [&](double top) {
        double half = availableHalfWidth(in, top, top + podH, preferredHalf);
        if (in.unitWidth > 0.0 && availableHalfWidth(in, top - unitH, top, preferredHalf) * 2.0 < in.unitWidth) {
            return 0.0;
        }
        return half * 2.0;
    };

    struct Place {
        double top{0.0};
        double width{0.0};
        FontFit fit;
    };
    auto placeAt = [&](double top) {
        Place p;
        p.top = top;
        p.width = widthAt(top);
        p.fit = fitFontPixelSize(preferredPx, minimumPx, p.width - 2.0 * in.padding, podH, in.valueWidth, in.valueHeight);
        return p;
    };

    const Place preferred = placeAt(in.centerY + r * 0.28);
    Place chosen = preferred;

    // Below the lowest label that lies under the centre and within the preferred width
    const double m = podLabelMargin(r);
    double lowest = preferred.top;
    for (const LayoutRect &lr : in.labelRects) {
        const bool below = lr.bottom() > in.centerY;
        const bool inPreferredWidth = lr.x < in.centerX + preferredHalf && lr.right() > in.centerX - preferredHalf;
        if (below && inPreferredWidth) {
            lowest = std::max(lowest, lr.bottom() + m);
        }
    }
    if (lowest > preferred.top) {
        const Place lowered = placeAt(lowest);
        const bool bothFit = preferred.fit.fits && lowered.fit.fits;
        const bool neitherFits = !preferred.fit.fits && !lowered.fit.fits;
        if ((bothFit && lowered.fit.pixelSize > preferred.fit.pixelSize) || (lowered.fit.fits && !preferred.fit.fits)
            || (neitherFits && lowered.width > preferred.width)) {
            chosen = lowered;
            out.lowered = true;
        }
    }

    if (chosen.fit.fits) {
        out.pod = {in.centerX - chosen.width * 0.5, chosen.top, chosen.width, chosen.width > 0.0 ? podH : 0.0};
        out.unit = {in.centerX - chosen.width * 0.5, chosen.top - unitH, chosen.width, unitH};
        out.valueFontPx = chosen.fit.pixelSize;
        out.clearOfLabels = true;
        return out;
    }

    // A pod as high as the text, of the given width: true when it fits at one of the two places
    auto compactAt = [&](int px, double width, bool avoidLabels, bool withUnit) {
        const double height = in.valueHeight(px);
        const double half = std::max(preferredHalf, width * 0.5);
        for (double top : {preferred.top, lowest}) {
            const bool unitFits = !withUnit || in.unitWidth <= 0.0
                                  || availableHalfWidth(in, top - unitH, top, half, avoidLabels) * 2.0 >= in.unitWidth;
            if (unitFits && availableHalfWidth(in, top, top + height, half, avoidLabels) * 2.0 >= width) {
                out.pod = {in.centerX - width * 0.5, top, width, height};
                out.unit = {in.centerX - width * 0.5, top - unitH, width, unitH};
                out.valueFontPx = px;
                out.lowered = top > preferred.top;
                out.showUnit = withUnit;
                return true;
            }
        }
        return false;
    };
    // The labels a pod placed over them would cut
    auto hideCutLabels = [&]() {
        const double margin = podLabelMargin(r);
        for (size_t i = 0; i < in.labelRects.size(); ++i) {
            const LayoutRect box{in.labelRects[i].x - margin, in.labelRects[i].y - margin,
                                 in.labelRects[i].w + 2.0 * margin, in.labelRects[i].h + 2.0 * margin};
            if (box.intersects(out.pod) || (out.showUnit && in.unitWidth > 0.0 && box.intersects(out.unit))) {
                out.hiddenLabels.push_back(static_cast<int>(i));
            }
        }
        out.clearOfLabels = out.hiddenLabels.empty();
    };

    for (int px = preferredPx; px >= minimumPx; --px) {
        if (compactAt(px, in.valueWidth(px) + 2.0 * in.padding, true, true)) {
            out.clearOfLabels = true;
            return out;
        }
    }
    if (compactAt(minimumPx, minWidth, false, true) || compactAt(minimumPx, minWidth, false, false)) {
        hideCutLabels();
        return out;
    }

    // Only the state indicator, also if it reaches the scale: it is never left out
    const double indicatorWidth = in.indicatorWidth + 2.0 * in.padding;
    if (!compactAt(minimumPx, indicatorWidth, true, false) && !compactAt(minimumPx, indicatorWidth, false, false)) {
        out.pod = {in.centerX - indicatorWidth * 0.5, preferred.top, indicatorWidth, minHeight};
        out.unit = {};
        out.valueFontPx = minimumPx;
        out.showUnit = false;
        out.clearOfScale = false;
    }
    out.showValue = false;
    hideCutLabels();
    return out;
}

FontFit fitFontPixelSize(int preferred, int minimum, double maxWidth, double maxHeight,
                         const std::function<double(int)> &textWidth,
                         const std::function<double(int)> &textHeight)
{
    minimum = std::max(1, minimum);
    preferred = std::max(minimum, preferred);
    for (int px = preferred; px >= minimum; --px) {
        if (textWidth(px) <= maxWidth && textHeight(px) <= maxHeight) {
            return {px, true};
        }
    }
    return {minimum, false};
}

} // namespace internal
} // namespace QtIndustrialWidgets
