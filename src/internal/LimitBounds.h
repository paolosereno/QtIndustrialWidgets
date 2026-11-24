/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief The limits of the operator's input of IndustrialKnob and IndustrialSlider (#182, #246).
 *
 * Each bound is set or not. A bound is kept as set and read within the range [minimum, maximum]; a bound not set
 * reads the end of the range on its side, so it follows the range. The callers ignore non-finite values.
 */
struct LimitBounds
{
    bool lowSet{false};
    bool highSet{false};
    double low{0.0};
    double high{0.0};

    [[nodiscard]] double readLow(double minimum, double maximum) const
    {
        return lowSet ? std::clamp(low, minimum, maximum) : minimum;
    }
    [[nodiscard]] double readHigh(double minimum, double maximum) const
    {
        return highSet ? std::clamp(high, minimum, maximum) : maximum;
    }

    /// Both bounds, as setValueLimits() / setSetpointLimits() (the caller checks \a l < \a h)
    void setBoth(double l, double h)
    {
        lowSet = highSet = true;
        low = l;
        high = h;
    }
    void clear() { lowSet = highSet = false; }

    /**
     * The low bound. A high bound not set stays not set and follows the range, whatever \a v. A high bound set, at
     * or below \a v, moves by the previous span (#204 item 4), with minimum for a low bound not set; when that span
     * is not positive (the high bound kept below a minimum that went up), the high bound goes to \a v.
     */
    void setLow(double v, double minimum, double maximum)
    {
        (void)maximum;
        if (highSet && v >= high) high = v + std::max(high - (lowSet ? low : minimum), 0.0);
        low = v;
        lowSet = true;
    }
    /// The high bound: the rule of setLow() the other way round
    void setHigh(double v, double minimum, double maximum)
    {
        (void)minimum;
        if (lowSet && v <= low) low = v - std::max((highSet ? high : maximum) - low, 0.0);
        high = v;
        highSet = true;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
