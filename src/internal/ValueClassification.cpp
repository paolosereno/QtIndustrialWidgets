// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/ValueClassification.h"

#include "internal/NumberFormat.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {
namespace internal {

static_assert(static_cast<int>(InputState::Valid) == static_cast<int>(ValueState::Valid));
static_assert(static_cast<int>(InputState::Underrange) == static_cast<int>(ValueState::Underrange));
static_assert(static_cast<int>(InputState::Overrange) == static_cast<int>(ValueState::Overrange));
static_assert(static_cast<int>(InputState::Invalid) == static_cast<int>(ValueState::Invalid));

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

ValueState publicValueState(InputState state)
{
    return static_cast<ValueState>(static_cast<int>(state));
}

InputState inputStateOf(ValueState state)
{
    return static_cast<InputState>(static_cast<int>(state));
}

double shownValue(double raw, double minimum, double maximum)
{
    if (std::isnan(raw)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::clamp(raw, minimum, maximum);
}

bool sameShownValue(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b);
    }
    return qFuzzyCompare(a, b);
}

QString gaugeReadoutText(InputState state, double raw, double minimum, double maximum, int precision)
{
    if (state == InputState::Invalid) {
        return QStringLiteral("---");
    }
    if (std::isinf(raw)) {
        return raw > 0.0 ? QLatin1Char('>') + formatFixed(maximum, precision)
                         : QLatin1Char('<') + formatFixed(minimum, precision);
    }
    return formatFixed(raw, precision);
}

} // namespace internal
} // namespace QtIndustrialWidgets
