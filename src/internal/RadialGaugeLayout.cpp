// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "RadialGaugeLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

    double width = chosen.width;
    out.clearOfLabels = chosen.fit.fits;
    if (!chosen.fit.fits) {
        width = std::max(width, minWidth); // the value has precedence over the labels
    }

    out.pod = {in.centerX - width * 0.5, chosen.top, width, width > 0.0 ? podH : 0.0};
    out.unit = {in.centerX - width * 0.5, chosen.top - unitH, width, unitH};
    out.valueFontPx = chosen.fit.pixelSize;
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
