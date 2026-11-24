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
     * The low bound. At or above the high bound, the high bound moves by the previous span (#204 item 4); a bound
     * not set counts as the end of the range. When that span is not positive (a bound past the end of a range that
     * shrank, with the other bound not set), only the low bound is set.
     */
    void setLow(double v, double minimum, double maximum)
    {
        const double other = highSet ? high : maximum;
        if (v >= other) {
            const double span = other - (lowSet ? low : minimum);
            if (span > 0) {
                high = v + span;
                highSet = true;
            }
        }
        low = v;
        lowSet = true;
    }
    /// The high bound: the rule of setLow() the other way round
    void setHigh(double v, double minimum, double maximum)
    {
        const double other = lowSet ? low : minimum;
        if (v <= other) {
            const double span = (highSet ? high : maximum) - other;
            if (span > 0) {
                low = v - span;
                lowSet = true;
            }
        }
        high = v;
        highSet = true;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
