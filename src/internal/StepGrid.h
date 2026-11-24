/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

// #244: where a drag takes the value of IndustrialKnob (Continuous) and IndustrialSlider. \a proposed is where the
// pointer asks it to go, already within [low, high], the interval the operator's input may reach (the limits, widened
// to a value outside them). The candidates are the grid points minimum + k·step inside that interval, computed in one
// go (never a sum of steps), and its two ends, which a drag reaches as the keys and Home/End do even off the grid; the
// nearest wins, a grid point on a tie. From \a start off the grid (a value read back), the value stays at \a start
// until the candidate lies on the side the pointer moved to, so it never moves against the pointer
[[nodiscard]] inline double dragValueOnTheStep(double proposed, double start, double minimum, double step, double low,
                                               double high)
{
    if (proposed == start) {
        return start;
    }
    double best = proposed - low <= high - proposed ? low : high;
    constexpr double kEpsilon = 1e-9; // An end on a grid point counts as one
    const double first = std::ceil((low - minimum) / step - kEpsilon);
    const double last = std::floor((high - minimum) / step + kEpsilon);
    if (first <= last) {
        const double k = std::clamp(std::round((proposed - minimum) / step), first, last);
        const double grid = std::clamp(minimum + k * step, low, high);
        if (std::abs(grid - proposed) <= std::abs(best - proposed)) {
            best = grid;
        }
    }
    if ((proposed > start && best < start) || (proposed < start && best > start)) {
        return start;
    }
    return best;
}

} // namespace internal
} // namespace QtIndustrialWidgets
