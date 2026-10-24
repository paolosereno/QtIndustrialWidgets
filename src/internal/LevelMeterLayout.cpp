// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "LevelMeterLayout.h"
#include "LinearGaugeScale.h"
#include "NumberFormat.h"

#include <QtGui/QFontMetricsF>

#include <algorithm>
#include <cmath>
#include <limits>

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
    if (!ticks.empty()) {
        m.endsWidth = 0.5 * (inkWidth(levelMeterTickText(ticks.front(), minimum, maximum, unit), fm)
                             + inkWidth(levelMeterTickText(ticks.back(), minimum, maximum, unit), fm));
        m.endsFraction = (ticks.back() - ticks.front()) / (maximum - minimum);
    }
    m.lineHeight = fm.height();
    m.minGap = scaleLabelMinGap(m.lineHeight);
    m.unitWidth = unit.isEmpty() ? 0.0 : fm.horizontalAdvance(unit);
    return m;
}

double levelMeterVerticalScaleMargin(const LevelMeterScaleMetrics &m)
{
    const double labels = std::max(36.0, 7.0 + 3.0 + m.widest + 2.0);
    return m.unitWidth > 0.0 ? std::max(labels, 4.0 + m.unitWidth + 2.0) : labels;
}

double levelMeterVerticalUnitBand(const LevelMeterScaleMetrics &m)
{
    // The top label is centred on the top of the bars (half a line above it); the unit line sits
    // minGap above that label, inside the content area
    return m.unitWidth > 0.0 ? 1.5 * m.lineHeight + m.minGap : 0.0;
}

double levelMeterHorizontalScaleMargin(const LevelMeterScaleMetrics &m)
{
    // Tick 3..7 px below the bars, label rectangle 2 px lower and levelMeterHorizontalLabelHeight()
    // high, then 2 px
    return std::max(36.0, 9.0 + levelMeterHorizontalLabelHeight(m) + 2.0);
}

double levelMeterHorizontalLabelHeight(const LevelMeterScaleMetrics &m)
{
    return std::max(14.0, m.lineHeight);
}

double levelMeterHorizontalUnitOffset(const LevelMeterScaleMetrics &m, double barHeight)
{
    if (m.unitWidth <= 0.0) {
        return 0.0;
    }
    // Free space between the unit line (centred on the last bar) and the ticks 3 px below the bar
    const double below = barHeight * 0.5 + 3.0 - m.lineHeight * 0.5;
    return below >= m.minGap ? 4.0 : std::max(4.0, m.last * 0.5 + m.minGap);
}

LevelMeterInsets levelMeterHorizontalInsets(const LevelMeterScaleMetrics &m, double channelLabelMargin, double barHeight)
{
    // The end labels are centred on the bar ends; keep them inside the chassis (8 px content margin)
    LevelMeterInsets insets{std::max(channelLabelMargin, m.first * 0.5 - 7.0), std::max(0.0, m.last * 0.5 - 7.0)};
    if (m.unitWidth > 0.0) {
        insets.right = std::max(insets.right, levelMeterHorizontalUnitOffset(m, barHeight) + m.unitWidth);
    }
    return insets;
}

namespace {

double labelGap(const std::vector<double> &centres, const std::vector<double> &extents, int i, int j)
{
    const double gap = std::abs(centres[static_cast<size_t>(i)] - centres[static_cast<size_t>(j)])
        - 0.5 * (extents[static_cast<size_t>(i)] + extents[static_cast<size_t>(j)]);
    return std::isfinite(gap) ? gap : -std::numeric_limits<double>::infinity();
}

} // namespace

std::vector<int> levelMeterSelectLabels(const std::vector<double> &ticks, const std::vector<double> &centres,
                                        const std::vector<double> &extents, bool linear, double minGap)
{
    const int n = static_cast<int>(std::min({ticks.size(), centres.size(), extents.size()}));
    if (n == 0) {
        return {};
    }
    if (n == 1) {
        return {0};
    }
    const double gap = std::isfinite(minGap) ? minGap : 0.0;
    const bool endsClash = labelGap(centres, extents, 0, n - 1) < gap - 1e-6;
    auto keepsGap = [&](const std::vector<int> &taken, int candidate) {
        // 1e-6 px: rounding must not drop a label that keeps exactly the gap (e.g. at minimumSizeHint())
        return std::all_of(taken.begin(), taken.end(), [&](int t) { return labelGap(centres, extents, t, candidate) >= gap - 1e-6; });
    };

    std::vector<int> drawn;
    if (linear) {
        // Smallest divisor of the number of intervals whose labels all keep the gap; with d = n - 1
        // only the ends remain (or the first one when they clash), which always fits
        const int intervals = n - 1;
        for (int d = 1; d <= intervals; ++d) {
            if (intervals % d != 0) {
                continue;
            }
            std::vector<int> set;
            bool fits = true;
            for (int i = 0; i <= intervals && fits; i += d) {
                if (i == intervals && endsClash) {
                    continue;
                }
                fits = keepsGap(set, i);
                set.push_back(i);
            }
            if (fits || d == intervals) {
                drawn = d == intervals && !fits ? std::vector<int>{0} : set;
                break;
            }
        }
    } else {
        std::vector<int> order{0};
        if (!endsClash) {
            order.push_back(n - 1);
        }
        auto indexOf = [&](double v) {
            for (int i = 1; i < n - 1; ++i) {
                if (ticks[static_cast<size_t>(i)] == v) return i;
            }
            return -1;
        };
        for (double v : {0.0, -20.0, -6.0, -40.0, -12.0, -30.0, -3.0, 3.0}) {
            const int i = indexOf(v);
            if (i >= 0) order.push_back(i);
        }
        for (int i = 1; i < n - 1; ++i) {
            if (std::find(order.begin(), order.end(), i) == order.end()) order.push_back(i);
        }
        for (int i : order) {
            // The ends are always drawn (the second one only when it does not clash with the first)
            if (i == 0 || i == n - 1 || keepsGap(drawn, i)) {
                drawn.push_back(i);
            }
        }
        std::sort(drawn.begin(), drawn.end());
    }
    return drawn;
}

std::vector<double> levelMeterLabelExtents(Qt::Orientation orientation, double minimum, double maximum,
                                           const QString &unit, const QFont &widgetFont)
{
    const QFontMetricsF fm(levelMeterScaleFont(widgetFont));
    std::vector<double> out;
    for (double t : levelMeterTicks(minimum, maximum, unit)) {
        out.push_back(orientation == Qt::Vertical ? fm.height() : inkWidth(levelMeterTickText(t, minimum, maximum, unit), fm));
    }
    return out;
}

QSize levelMeterMinimumSize(Qt::Orientation orientation, int channels, bool scaleVisible, bool channelLabels,
                            bool title, const LevelMeterScaleMetrics &m, const QSize &base)
{
    const int n = std::max(1, channels);
    const double bars = n * 6.0 + (n - 1) * 4.0;
    if (orientation == Qt::Vertical) {
        const double width = 2 * kChassisAndContentMargin + bars + (scaleVisible ? levelMeterVerticalScaleMargin(m) : 0.0);
        double height = 0.0;
        if (scaleVisible) {
            // #51: bars long enough that every linear label (6 divisions) and both dB ends keep minGap
            // (labels one line high, centred on their ticks), plus the title, the unit band and the
            // channel labels; the end labels stick out half a line past the bars
            const double step = m.lineHeight + m.minGap;
            const double ends = m.endsFraction > 0.0 ? step / m.endsFraction : 0.0;
            const double length = m.linear ? std::max(6.0 * step, ends) : ends;
            const double unitBand = levelMeterVerticalUnitBand(m);
            const double below = channelLabels ? 16.0 : 0.0;
            const double half = m.lineHeight * 0.5;
            height = 2 * kChassisAndContentMargin + (title ? 18.0 : 0.0) + unitBand + below + length
                + (unitBand > 0.0 ? 0.0 : std::max(0.0, half - 8.0)) + std::max(0.0, half - (below > 0.0 ? 16.0 : 8.0));
        }
        return base.expandedTo(QSize(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height))));
    }
    // Height (#57): margins, title, bars at their 6 px minimum and the scale
    const double height = 2 * kChassisAndContentMargin + (title ? 18.0 : 0.0) + bars
        + (scaleVisible ? levelMeterHorizontalScaleMargin(m) : 0.0);
    double width = 0.0;
    if (scaleVisible) {
        const LevelMeterInsets insets = levelMeterHorizontalInsets(m, channelLabels ? 16.0 : 0.0, 6.0);
        // The end labels, centred on the end ticks, keep minGap (both ends drawn); on a linear scale
        // the 6 divisions also keep it between neighbours, so all of them are drawn (#57)
        const double ends = m.endsFraction > 0.0 ? (m.endsWidth + m.minGap) / m.endsFraction : 0.0;
        const double length = m.linear ? std::max(6.0 * (m.widest + m.minGap), ends) : ends;
        width = 2 * kChassisAndContentMargin + insets.left + insets.right + length;
    }
    return base.expandedTo(QSize(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height))));
}

} // namespace internal
} // namespace QtIndustrialWidgets
