// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "NumberFormat.h"

#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

QString formatFixed(double value, int decimals)
{
    QString text = QString::number(value, 'f', decimals);
    if (text.startsWith(QLatin1Char('-'))) {
        bool zero = true;
        for (int i = 1; i < text.size() && zero; ++i) {
            zero = text.at(i) == QLatin1Char('0') || text.at(i) == QLatin1Char('.');
        }
        if (zero) {
            text.remove(0, 1);
        }
    }
    return text;
}

QString scaleLabelText(double value, int precision)
{
    if (precision == 0) {
        return QString::number(static_cast<qint64>(std::round(value)));
    }
    return formatFixed(value, (value == std::floor(value)) ? 0 : 1);
}

} // namespace internal
} // namespace QtIndustrialWidgets
