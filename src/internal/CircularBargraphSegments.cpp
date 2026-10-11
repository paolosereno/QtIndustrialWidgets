// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/CircularBargraphSegments.h"

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

InputState classifyInput(double raw, double minimum, double maximum)
{
    if (std::isnan(raw)) {
        return InputState::Invalid;
    }
    if (raw > maximum) {
        return InputState::Overrange;
    }
    if (raw < minimum) {
        return InputState::Underrange;
    }
    return InputState::Valid;
}

std::vector<SegmentState> computeSegmentStates(const SegmentInput &in)
{
    const int N = in.segmentCount;
    if (N < 1) {
        return {};
    }
    if (in.state == InputState::Invalid) {
        return std::vector<SegmentState>(static_cast<size_t>(N), SegmentState::Ghost);
    }
    std::vector<SegmentState> states(static_cast<size_t>(N), SegmentState::Off);

    if (!(in.maximum > in.minimum)) {
        return states;
    }

    // Single value -> segment mapping for bar, dot and peak: a value lights the first
    // round(f * N) segments, and its "top" segment is the last of them.
    const auto litCount = [&](double v) {
        double f = (v - in.minimum) / (in.maximum - in.minimum);
        f = std::isfinite(f) ? std::clamp(f, 0.0, 1.0) : 0.0; // never cast NaN to int
        return std::clamp(static_cast<int>(std::round(f * N)), 0, N);
    };

    const int n = litCount(in.value);
    int topLit = n - 1; // -1: nothing lit
    if (in.dotMode) {
        // A valid value always shows its cursor, even at the minimum.
        topLit = std::max(n - 1, 0);
        states[static_cast<size_t>(topLit)] = SegmentState::Lit;
    } else {
        std::fill(states.begin(), states.begin() + n, SegmentState::Lit);
    }

    if (in.peakHoldEnabled) {
        const int peakIndex = litCount(in.peakValue) - 1;
        if (peakIndex > topLit) {
            states[static_cast<size_t>(peakIndex)] = SegmentState::Peak;
        }
    }

    // Out of range: the end segment blinks, over anything computed above (bar, dot, peak).
    int blinkIndex = -1;
    if (in.state == InputState::Overrange) {
        blinkIndex = N - 1;
    } else if (in.state == InputState::Underrange) {
        blinkIndex = 0;
    }
    if (blinkIndex >= 0) {
        states[static_cast<size_t>(blinkIndex)] = in.blinkOn ? SegmentState::Lit : SegmentState::Off;
    }
    return states;
}

QString formatReadout(InputState state, double value, double minimum, double maximum, int precision)
{
    switch (state) {
    case InputState::Invalid:
        return QStringLiteral("---");
    case InputState::Overrange:
        return QLatin1Char('>') + QString::number(maximum, 'f', precision);
    case InputState::Underrange:
        return QLatin1Char('<') + QString::number(minimum, 'f', precision);
    case InputState::Valid:
        break;
    }
    return QString::number(value, 'f', precision);
}

} // namespace internal
} // namespace QtIndustrialWidgets
