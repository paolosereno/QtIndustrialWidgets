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

// Vertical gauge: top of the tube with and without the digital display
constexpr double kVerticalTopWithDisplay = 36.0;
constexpr double kVerticalTopWithoutDisplay = 16.0;
// Free space kept between a scale label and the digital display (#37)
constexpr double kDisplayClearance = 1.0;

double finiteOrZero(double v)
{
    return std::isfinite(v) ? v : 0.0;
}

// Keeps the labels of the smallest divisor of n that keeps the gap; n itself (first and last only)
// if none does
ScaleLabelLayout selectLabels(const std::vector<ScaleLabelSlot> &slots, double gap)
{
    const int n = static_cast<int>(slots.size()) - 1;
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

    ScaleLabelLayout layout;
    layout.step = step;
    for (int i = 0; i <= n; i += step) {
        layout.labels.push_back(slots[static_cast<size_t>(i)]);
    }
    return layout;
}

} // namespace

double scaleLabelMinGap(double fontHeight)
{
    return std::max(4.0, 0.5 * finiteOrZero(fontHeight));
}

ScaleLabelLayout layoutScaleLabels(const std::vector<double> &extents, double scaleLength, double minGap)
{
    if (extents.empty()) {
        return {};
    }

    const int n = static_cast<int>(extents.size()) - 1;
    const double length = std::max(0.0, finiteOrZero(scaleLength));

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
    return selectLabels(slots, finiteOrZero(minGap));
}

ScaleLabelLayout layoutVerticalScaleLabels(const std::vector<VerticalLabelMetrics> &labels, double scaleLength,
                                           double minGap, const VerticalScaleRoom &room)
{
    if (labels.empty()) {
        return {};
    }

    const int n = static_cast<int>(labels.size()) - 1;
    const double length = std::max(0.0, finiteOrZero(scaleLength));

    std::vector<ScaleLabelSlot> slots(labels.size());
    for (int i = 0; i <= n; ++i) {
        const VerticalLabelMetrics &label = labels[static_cast<size_t>(i)];
        const double extent = std::max(0.0, finiteOrZero(label.extent));
        double centre = (n == 0) ? 0.0 : length * i / n;
        if (i == 0) {
            centre = std::max(centre, finiteOrZero(label.inkBelow) - finiteOrZero(room.below));
        }
        if (i == n && n > 0) {
            centre = std::min(centre, length + finiteOrZero(room.above) - finiteOrZero(label.inkAbove));
        }
        slots[static_cast<size_t>(i)] = ScaleLabelSlot{i, centre - extent * 0.5, centre + extent * 0.5};
    }
    return selectLabels(slots, finiteOrZero(minGap));
}

VerticalLabelMetrics verticalLabelMetrics(double lineHeight, double ascent, double inkTop, double inkBottom)
{
    // The line box is centred on the tick: the baseline lies ascent - lineHeight / 2 below the centre
    const double baseline = ascent - lineHeight * 0.5;
    return {lineHeight, -(baseline + inkTop), baseline + inkBottom};
}

VerticalScaleRoom linearGaugeVerticalRoom(double width, double height, bool thermometer, bool displayVisible)
{
    const double tubeTop = displayVisible ? kVerticalTopWithDisplay : kVerticalTopWithoutDisplay;
    VerticalScaleRoom room;
    if (displayVisible) {
        const GaugeRect pod = linearGaugeDisplayRect(true, width, height);
        room.above = tubeTop - (pod.y + pod.h) - kDisplayClearance;
    } else {
        room.above = tubeTop;
    }
    room.below = height - (tubeTop + linearGaugeScaleLength(true, width, height, thermometer, displayVisible));
    return room;
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

int chooseVerticalLabelPixelSize(int preferredPx, int minimumPx, double scaleLength, const VerticalScaleRoom &room,
                                 const std::function<std::vector<VerticalLabelMetrics>(int)> &metricsAt,
                                 const std::function<double(int)> &minGapAt)
{
    if (!metricsAt || !minGapAt) {
        return preferredPx;
    }
    for (int px = preferredPx; px >= minimumPx && px > 0; --px) {
        if (layoutVerticalScaleLabels(metricsAt(px), scaleLength, minGapAt(px), room).step <= 2) {
            return px;
        }
    }
    return preferredPx;
}

double linearGaugeScaleLength(bool vertical, double width, double height, bool thermometer, bool displayVisible)
{
    if (vertical) {
        const double tubeWidth = std::clamp(width * 0.16, 10.0, 26.0);
        const double top = displayVisible ? kVerticalTopWithDisplay : kVerticalTopWithoutDisplay;
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
