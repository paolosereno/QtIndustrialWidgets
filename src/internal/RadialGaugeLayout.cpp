// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "RadialGaugeLayout.h"

#include <algorithm>
#include <cmath>

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

double podLabelMargin(double radius)
{
    return std::max(2.0, radius * 0.02);
}

namespace {

// Half-width available to a box centred on cx between y0 and y1: limited by the preferred
// half-width, by the inner dial and by every label that overlaps the band vertically.
double availableHalfWidth(const PodLayoutInput &in, double y0, double y1, double preferredHalf)
{
    const double m = podLabelMargin(in.radius);
    double half = preferredHalf;

    const double innerR = in.radius * 0.95 - m;
    const double farY = std::max(std::abs(y0 - in.centerY), std::abs(y1 - in.centerY));
    if (farY >= innerR) {
        return 0.0;
    }
    half = std::min(half, std::sqrt(innerR * innerR - farY * farY));

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
    return std::max(0.0, half);
}

} // namespace

PodLayout layoutDigitalPod(const PodLayoutInput &in)
{
    PodLayout out;
    if (!std::isfinite(in.centerX) || !std::isfinite(in.centerY) || !std::isfinite(in.radius) || in.radius <= 0.0
        || !std::isfinite(in.minPodWidth) || !std::isfinite(in.minPodHeight) || !std::isfinite(in.unitWidth)) {
        return out;
    }

    const double r = in.radius;
    const double podH = std::max(r * 0.28, in.minPodHeight);
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

    double top = in.centerY + r * 0.28;
    double width = widthAt(top);

    if (width < in.minPodWidth) {
        // Move below the lowest label that lies under the centre and within the preferred width
        const double m = podLabelMargin(r);
        double lowest = top;
        for (const LayoutRect &lr : in.labelRects) {
            const bool below = lr.bottom() > in.centerY;
            const bool inPreferredWidth = lr.x < in.centerX + preferredHalf && lr.right() > in.centerX - preferredHalf;
            if (below && inPreferredWidth) {
                lowest = std::max(lowest, lr.bottom() + m);
            }
        }
        const double loweredWidth = widthAt(lowest);
        if (lowest > top && loweredWidth > width) {
            top = lowest;
            width = loweredWidth;
            out.lowered = true;
        }
    }

    out.clearOfLabels = width > 0.0 && width >= in.minPodWidth;
    if (width < in.minPodWidth) {
        width = in.minPodWidth; // the value has precedence over the labels
    }

    out.pod = {in.centerX - width * 0.5, top, width, width > 0.0 ? podH : 0.0};
    out.unit = {in.centerX - width * 0.5, top - unitH, width, unitH};
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
