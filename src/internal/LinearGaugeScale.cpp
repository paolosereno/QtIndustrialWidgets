/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
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

} // namespace internal
} // namespace QtIndustrialWidgets
