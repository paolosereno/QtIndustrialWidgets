// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/CircularBargraphSegments.h"

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

std::vector<SegmentState> computeSegmentStates(const SegmentInput &in)
{
    const int N = in.segmentCount;
    if (N < 1) {
        return {};
    }
    std::vector<SegmentState> states(static_cast<size_t>(N), SegmentState::Off);

    const double rangeSpan = in.maximum - in.minimum;
    const double factor = (rangeSpan > 0.0)
        ? std::clamp((in.value - in.minimum) / rangeSpan, 0.0, 1.0)
        : 0.0;

    const int litSegments = (rangeSpan > 0.0)
        ? std::clamp(static_cast<int>(std::round(factor * N)), 0, N)
        : 0;

    const int dotIndex = (rangeSpan > 0.0)
        ? std::clamp(static_cast<int>(std::floor(factor * (N - 1) + 0.5)), 0, N - 1)
        : 0;

    int peakIndex = -1;
    if (in.peakHoldEnabled && rangeSpan > 0.0) {
        const double peakFactor = std::clamp((in.peakValue - in.minimum) / rangeSpan, 0.0, 1.0);
        peakIndex = std::clamp(static_cast<int>(std::floor(peakFactor * (N - 1) + 0.5)), 0, N - 1);
    }

    for (int i = 0; i < N; ++i) {
        bool isLit = false;
        if (!in.dotMode) {
            isLit = (i < litSegments);
        } else {
            isLit = (i == dotIndex && in.value >= in.minimum);
        }

        if (in.peakHoldEnabled && i == peakIndex) {
            states[static_cast<size_t>(i)] = SegmentState::Peak;
        } else if (isLit) {
            states[static_cast<size_t>(i)] = SegmentState::Lit;
        }
    }
    return states;
}

} // namespace internal
} // namespace QtIndustrialWidgets
