// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "NumberFormat.h"

#include <algorithm>
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

namespace {

constexpr int kMaxExactDecimals = 6;   // #208, rule 3
constexpr int kMaxLabelDecimals = 12;  // The bound of readoutValueText()

QStringList textsAt(const std::vector<double> &values, int decimals)
{
    QStringList texts;
    texts.reserve(static_cast<int>(values.size()));
    for (double v : values) texts << formatFixed(v, decimals);
    return texts;
}

bool adjacentDistinct(const QStringList &texts)
{
    for (int i = 1; i < texts.size(); ++i) {
        if (texts.at(i) == texts.at(i - 1)) return false;
    }
    return true;
}

// Every value written with \a decimals digits is itself, within \a tolerance
bool exactAt(const std::vector<double> &values, int decimals, double tolerance)
{
    const double scale = std::pow(10.0, decimals);
    for (double v : values) {
        if (std::abs(std::round(v * scale) / scale - v) > tolerance) return false;
    }
    return true;
}

} // namespace

QStringList scaleLabelTexts(const std::vector<double> &values, int precision)
{
    precision = std::clamp(precision, 0, kMaxLabelDecimals);
    // The step between adjacent values: the smallest gap, so that the tolerance holds for every label
    double step = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double gap = std::abs(values[i] - values[i - 1]);
        if (gap > 0.0 && (step == 0.0 || gap < step)) step = gap;
    }
    const double tolerance = 1e-9 * step;
    for (int d = 0; d <= precision; ++d) { // Rule 1
        if (exactAt(values, d, tolerance)) return textsAt(values, d);
    }
    QStringList texts = textsAt(values, precision);
    if (adjacentDistinct(texts)) return texts; // Rule 2
    for (int d = precision + 1; d <= kMaxExactDecimals; ++d) { // Rule 3
        if (exactAt(values, d, tolerance)) return textsAt(values, d);
    }
    for (int d = precision + 1; d <= kMaxLabelDecimals; ++d) { // Rules 4 and 5
        texts = textsAt(values, d);
        if (adjacentDistinct(texts)) return texts;
    }
    return texts;
}

} // namespace internal
} // namespace QtIndustrialWidgets
