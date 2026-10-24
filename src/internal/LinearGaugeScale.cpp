/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal/LinearGaugeScale.h"

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

double finiteOrZero(double v)
{
    return std::isfinite(v) ? v : 0.0;
}

} // namespace

double scaleLabelMinGap(double fontHeight)
{
    return std::max(4.0, 0.5 * finiteOrZero(fontHeight));
}

ScaleLabelLayout layoutScaleLabels(const std::vector<double> &extents, double scaleLength, double minGap)
{
    ScaleLabelLayout layout;
    if (extents.empty()) {
        return layout;
    }

    const int n = static_cast<int>(extents.size()) - 1;
    const double length = std::max(0.0, finiteOrZero(scaleLength));
    const double gap = finiteOrZero(minGap);

    // Every label centred on its tick; the end labels pushed inside [0, length]
    std::vector<ScaleLabelSlot> slots(extents.size());
    for (int i = 0; i <= n; ++i) {
        const double extent = std::max(0.0, finiteOrZero(extents[static_cast<size_t>(i)]));
        const double centre = (n == 0) ? 0.0 : length * i / n;
        ScaleLabelSlot slot{i, centre - extent * 0.5, centre + extent * 0.5};
        if (i == 0 && slot.start < 0.0) {
            slot.end -= slot.start;
            slot.start = 0.0;
        }
        if (i == n && n > 0 && slot.end > length) {
            slot.start -= slot.end - length;
            slot.end = length;
        }
        slots[static_cast<size_t>(i)] = slot;
    }

    // Smallest divisor of n that keeps the gap; n itself (first and last only) if none does
    int step = std::max(1, n);
    for (int d = 1; d < n; ++d) {
        if (n % d != 0) {
            continue;
        }
        bool fits = true;
        for (int i = 0; i + d <= n; i += d) {
            if (slots[static_cast<size_t>(i + d)].start - slots[static_cast<size_t>(i)].end < gap) {
                fits = false;
                break;
            }
        }
        if (fits) {
            step = d;
            break;
        }
    }

    layout.step = step;
    for (int i = 0; i <= n; i += step) {
        layout.labels.push_back(slots[static_cast<size_t>(i)]);
    }
    return layout;
}

int linearGaugeBaseLabelPixelSize(bool vertical, double height)
{
    return std::clamp(static_cast<int>(finiteOrZero(height) * (vertical ? 0.04 : 0.12)), 8, 12);
}

int chooseLabelPixelSize(int preferredPx, int minimumPx, double scaleLength,
                         const std::function<std::vector<double>(int)> &extentsAt,
                         const std::function<double(int)> &minGapAt)
{
    if (!extentsAt || !minGapAt) {
        return preferredPx;
    }
    for (int px = preferredPx; px >= minimumPx && px > 0; --px) {
        if (layoutScaleLabels(extentsAt(px), scaleLength, minGapAt(px)).step <= 2) {
            return px;
        }
    }
    return preferredPx;
}

double linearGaugeScaleLength(bool vertical, double width, double height, bool thermometer, bool displayVisible)
{
    if (vertical) {
        const double tubeWidth = std::clamp(width * 0.16, 10.0, 26.0);
        const double top = displayVisible ? 36.0 : 16.0;
        const double bottom = thermometer ? tubeWidth * 1.35 * 2.0 + 12.0 : 16.0;
        return std::max(10.0, (height - bottom) - top);
    }
    const double tubeHeight = std::clamp(height * 0.16, 10.0, 26.0);
    const double left = thermometer ? tubeHeight * 1.35 * 2.0 + 12.0 : 16.0;
    const double right = displayVisible ? 55.0 : 16.0;
    return std::max(10.0, (width - right) - left);
}

GaugeRect linearGaugeDisplayRect(bool vertical, double width, double height)
{
    if (vertical) {
        return {6.0, 6.0, width - 12.0, 24.0};
    }
    // The tube leaves 55 px on the right for the display
    return {width - 55.0 + 4.0, 6.0, 55.0 - 10.0, height - 12.0};
}

} // namespace internal
} // namespace QtIndustrialWidgets
