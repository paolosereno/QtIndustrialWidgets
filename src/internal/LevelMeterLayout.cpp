// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "LevelMeterLayout.h"
#include "NumberFormat.h"

#include <QtGui/QFontMetricsF>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

constexpr double kChassisAndContentMargin = 11.0; // chassis 3 px + content 8 px, on each side

bool isDbLadder(double minimum, double maximum, const QString &unit)
{
    return minimum < 0 && maximum > 0 && unit == QLatin1String("dB");
}

// Width of a text covering both its advance and its glyph ink (tightBoundingRect(): see #44)
double inkWidth(const QString &text, const QFontMetricsF &fm)
{
    const double advance = fm.horizontalAdvance(text);
    const QRectF ink = fm.tightBoundingRect(text);
    return std::max(advance, ink.right()) - std::min(0.0, ink.left());
}

} // namespace

QFont levelMeterScaleFont(const QFont &widgetFont)
{
    QFont f = widgetFont;
    f.setPointSizeF(std::max(6.0, widgetFont.pointSizeF() * 0.7));
    return f;
}

std::vector<double> levelMeterTicks(double minimum, double maximum, const QString &unit)
{
    std::vector<double> ticks;
    if (isDbLadder(minimum, maximum, unit)) {
        for (double t : {-60.0, -40.0, -30.0, -20.0, -12.0, -6.0, -3.0, 0.0, 3.0, 6.0}) {
            if (t >= minimum && t <= maximum) ticks.push_back(t);
        }
    } else {
        const int divisions = 6;
        const double step = (maximum - minimum) / divisions;
        for (int i = 0; i <= divisions; ++i) {
            ticks.push_back(minimum + i * step);
        }
    }
    return ticks;
}

QString levelMeterTickText(double tick, double minimum, double maximum, const QString &unit)
{
    if (unit == QLatin1String("dB")) {
        return (tick > 0 ? QStringLiteral("+") : QString()) + QString::number(static_cast<int>(tick));
    }
    return formatFixed(tick, (std::abs(maximum - minimum) < 10) ? 1 : 0);
}

LevelMeterScaleMetrics levelMeterScaleMetrics(double minimum, double maximum, const QString &unit, const QFont &widgetFont)
{
    LevelMeterScaleMetrics m;
    m.linear = !isDbLadder(minimum, maximum, unit);
    const QFontMetricsF fm(levelMeterScaleFont(widgetFont));
    const std::vector<double> ticks = levelMeterTicks(minimum, maximum, unit);
    for (size_t i = 0; i < ticks.size(); ++i) {
        const double w = inkWidth(levelMeterTickText(ticks[i], minimum, maximum, unit), fm);
        m.widest = std::max(m.widest, w);
        if (ticks[i] <= minimum) m.first = w;
        if (ticks[i] >= maximum) m.last = w;
    }
    return m;
}

double levelMeterVerticalScaleMargin(const LevelMeterScaleMetrics &m)
{
    return std::max(36.0, 7.0 + 3.0 + m.widest + 2.0);
}

LevelMeterInsets levelMeterHorizontalInsets(const LevelMeterScaleMetrics &m, double channelLabelMargin)
{
    // The end labels are centred on the bar ends; keep them inside the chassis (8 px content margin)
    return {std::max(channelLabelMargin, m.first * 0.5 - 7.0), std::max(0.0, m.last * 0.5 - 7.0)};
}

QSize levelMeterMinimumSize(Qt::Orientation orientation, int channels, bool scaleVisible, bool channelLabels,
                            const LevelMeterScaleMetrics &m, const QSize &base)
{
    const int n = std::max(1, channels);
    if (orientation == Qt::Vertical) {
        const double bars = n * 6.0 + (n - 1) * 4.0;
        const double width = 2 * kChassisAndContentMargin + bars + (scaleVisible ? levelMeterVerticalScaleMargin(m) : 0.0);
        return base.expandedTo(QSize(static_cast<int>(std::ceil(width)), base.height()));
    }
    if (!scaleVisible) {
        return base;
    }
    const LevelMeterInsets insets = levelMeterHorizontalInsets(m, channelLabels ? 16.0 : 0.0);
    double bars = 0.0;
    if (m.linear) {
        bars = 6.0 * (m.widest + kLevelMeterLabelGap); // 6 divisions: neighbouring labels never touch
    }
    const double width = 2 * kChassisAndContentMargin + insets.left + insets.right + bars;
    return base.expandedTo(QSize(static_cast<int>(std::ceil(width)), base.height()));
}

} // namespace internal
} // namespace QtIndustrialWidgets
