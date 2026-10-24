// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/CircularBargraphSegments.h"
#include "internal/NumberFormat.h"

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

std::vector<Zone> computeSegmentZones(double minimum, double maximum, double warningThreshold,
                                      double errorThreshold, int segmentCount)
{
    if (segmentCount < 1) {
        return {};
    }
    std::vector<Zone> zones(static_cast<size_t>(segmentCount), Zone::Normal);
    const double span = maximum - minimum;

    // c_i >= t is decided in segment units, i + 0.5 >= pos(t) with pos(t) = (t - min) * N / span,
    // so that no product is followed by a sum: the result does not depend on whether the
    // compiler contracts a * b + c into an FMA (it does on arm64 / with -ffp-contract=fast).
    // For span < 0 the inequality flips; for span == 0 every centre is the minimum.
    const auto atOrAbove = [&](int i, double threshold) {
        if (span == 0.0) {
            return minimum >= threshold;
        }
        const double pos = (threshold - minimum) * segmentCount / span;
        return (span > 0.0) ? (i + 0.5 >= pos) : (i + 0.5 <= pos);
    };
    for (int i = 0; i < segmentCount; ++i) {
        if (atOrAbove(i, errorThreshold)) {
            zones[static_cast<size_t>(i)] = Zone::Error;
        } else if (atOrAbove(i, warningThreshold)) {
            zones[static_cast<size_t>(i)] = Zone::Warning;
        }
    }
    return zones;
}

PeakDecayStep decayPeakStep(double peak, double value, double rate, double dtSeconds)
{
    // Decide on peak - value <= drop rather than on peak - rate * dt <= value: the latter can be
    // contracted into an FMA, which changes the decision near the boundary.
    const double drop = rate * dtSeconds;
    if (peak - value <= drop) {
        return {value, true};
    }
    return {std::max(value, peak - drop), false};
}

QColor unlitTint(const QColor &zoneColor, const QColor &ghostColor)
{
    const auto mix = [](int zone, int ghost) {
        return qRound(kUnlitTintWeight * zone + (1.0 - kUnlitTintWeight) * ghost);
    };
    return QColor(mix(zoneColor.red(), ghostColor.red()),
                  mix(zoneColor.green(), ghostColor.green()),
                  mix(zoneColor.blue(), ghostColor.blue()));
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
        return QLatin1Char('>') + formatFixed(maximum, precision);
    case InputState::Underrange:
        return QLatin1Char('<') + formatFixed(minimum, precision);
    case InputState::Valid:
        break;
    }
    return formatFixed(value, precision);
}

} // namespace internal
} // namespace QtIndustrialWidgets
