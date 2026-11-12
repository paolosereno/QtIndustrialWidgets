// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "ThumbwheelSwitchLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

qint64 power10(int n)
{
    qint64 p = 1;
    for (int i = 0; i < n; ++i) {
        p *= 10;
    }
    return p;
}

double finiteOrZero(double v)
{
    return std::isfinite(v) ? v : 0.0;
}

double nonNegative(double v)
{
    return std::max(0.0, finiteOrZero(v));
}

} // namespace

// ----- Value model -----

qint64 ThumbwheelFormat::capacity() const
{
    return power10(std::clamp(digitCount, 1, kThumbwheelMaxDigits)) - 1;
}

qint64 ThumbwheelFormat::scale() const
{
    return power10(std::clamp(decimals, 0, kThumbwheelMaxDigits));
}

bool ThumbwheelFormat::canShow(qint64 count) const
{
    if (count < 0 && !signDrum) {
        return false;
    }
    return (count < 0 ? -count : count) <= capacity();
}

double ThumbwheelFormat::valueOf(qint64 count) const
{
    return static_cast<double>(count) / static_cast<double>(scale());
}

std::optional<qint64> ThumbwheelFormat::countOf(double value) const
{
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    const double scaled = std::round(value * static_cast<double>(scale())); // std::round: halves away from zero
    // Far beyond any capacity (10^9): out of the range of a count, and never shown
    if (std::abs(scaled) > 1e15) {
        return std::nullopt;
    }
    return static_cast<qint64>(scaled);
}

int ThumbwheelFormat::digit(qint64 count, int decade) const
{
    const qint64 magnitude = count < 0 ? -count : count;
    const int fromRight = std::clamp(digitCount, 1, kThumbwheelMaxDigits) - 1 - decade;
    if (fromRight < 0) {
        return 0;
    }
    return static_cast<int>((magnitude / power10(fromRight)) % 10);
}

std::optional<qint64> ThumbwheelFormat::stepDigit(qint64 count, int decade, int direction) const
{
    const int n = std::clamp(digitCount, 1, kThumbwheelMaxDigits);
    if (decade < 0 || decade >= n || direction == 0) {
        return std::nullopt;
    }
    const int d = digit(count, decade);
    if ((direction > 0 && d == 9) || (direction < 0 && d == 0)) {
        return std::nullopt; // Saturation: no wrap-around, no carry
    }
    const qint64 unit = power10(n - 1 - decade);
    const qint64 magnitude = (count < 0 ? -count : count) + (direction > 0 ? unit : -unit);
    return count < 0 ? -magnitude : magnitude; // 0 has no sign: a negative value that reaches 0 is 0
}

std::optional<qint64> ThumbwheelFormat::withSign(qint64 count, bool negative) const
{
    if (!signDrum || count == 0) {
        return std::nullopt;
    }
    const qint64 magnitude = count < 0 ? -count : count;
    return negative ? -magnitude : magnitude;
}

ThumbwheelLimitState ThumbwheelLimits::stateOf(double value) const
{
    if (value < low) {
        return ThumbwheelLimitState::Below;
    }
    if (value > high) {
        return ThumbwheelLimitState::Above;
    }
    return ThumbwheelLimitState::Inside;
}

double ThumbwheelLimits::distance(double value) const
{
    if (value < low) {
        return low - value;
    }
    if (value > high) {
        return value - high;
    }
    return 0.0;
}

bool ThumbwheelLimits::allows(double from, double to) const
{
    const double after = distance(to);
    return after == 0.0 || after < distance(from);
}

// ----- Geometry -----

double thumbwheelDrumPadX(double digitPx)
{
    return std::ceil(std::max(3.0, nonNegative(digitPx) * 0.25));
}

double thumbwheelDrumPadY(double digitPx)
{
    return std::ceil(std::max(2.0, nonNegative(digitPx) * 0.12));
}

namespace {

struct Sizes {
    double drumW{0.0};
    double drumH{0.0};
    double buttonH{0.0};
    double pointGap{0.0}; // The gap that holds the decimal point
    double dot{0.0};      // Diameter of the decimal point
    double unitW{0.0};
    double tagW{0.0};
    int drums{0};
    double groupW{0.0};
    double groupH{0.0};
};

Sizes sizesOf(const ThumbwheelLayoutInput &in)
{
    Sizes s;
    const ThumbwheelTextSizes &t = in.text;
    const double px = nonNegative(t.digitPx);
    s.drumW = std::ceil(nonNegative(t.digitWidth) + 2.0 * thumbwheelDrumPadX(px));
    s.drumH = std::ceil(nonNegative(t.digitHeight) + 2.0 * thumbwheelDrumPadY(px));
    s.buttonH = std::max(kThumbwheelMinButton, std::ceil(s.drumH * 0.6));
    s.dot = std::max(3.0, std::round(px * 0.18));
    s.pointGap = std::max(kThumbwheelDrumGap, s.dot + 4.0);
    s.unitW = std::ceil(nonNegative(t.unitWidth));
    s.tagW = std::ceil(nonNegative(t.tagWidth));
    const int digits = std::clamp(in.format.digitCount, 1, kThumbwheelMaxDigits);
    s.drums = digits + (in.format.signDrum ? 1 : 0);
    const int decimals = std::clamp(in.format.decimals, 0, digits - 1);
    double w = s.drums * s.drumW + (s.drums - 1) * kThumbwheelDrumGap;
    if (decimals > 0) {
        w += s.pointGap - kThumbwheelDrumGap;
    }
    if (s.unitW > 0.0) {
        w += kThumbwheelUnitGap + s.unitW;
    }
    w += kThumbwheelTagGap + s.tagW;
    s.groupW = w;
    s.groupH = std::max(s.drumH + 2.0 * (kThumbwheelButtonGap + s.buttonH), nonNegative(t.tagHeight));
    return s;
}

} // namespace

ThumbwheelLayout layoutThumbwheel(const ThumbwheelLayoutInput &in)
{
    const Sizes s = sizesOf(in);
    ThumbwheelLayout l;
    l.signDrum = in.format.signDrum;
    const double w = nonNegative(in.width);
    const double h = nonNegative(in.height);
    // Centred, on whole pixels; never left of or above the margin
    const double x0 = std::max(kThumbwheelMargin, std::floor((w - s.groupW) * 0.5));
    const double y0 = std::max(kThumbwheelMargin, std::floor((h - s.groupH) * 0.5));
    const double drumTop = y0 + std::floor((s.groupH - (s.drumH + 2.0 * (kThumbwheelButtonGap + s.buttonH))) * 0.5)
                           + s.buttonH + kThumbwheelButtonGap;
    const int digits = std::clamp(in.format.digitCount, 1, kThumbwheelMaxDigits);
    const int decimals = std::clamp(in.format.decimals, 0, digits - 1);
    // The decimal point follows the drum of this decade
    const int pointAfterDecade = decimals > 0 ? digits - decimals - 1 : -1;

    double x = x0;
    for (int i = 0; i < s.drums; ++i) {
        const QRectF drum(x, drumTop, s.drumW, s.drumH);
        l.drums.push_back(drum);
        l.plus.push_back(QRectF(x, drumTop - kThumbwheelButtonGap - s.buttonH, s.drumW, s.buttonH));
        l.minus.push_back(QRectF(x, drum.bottom() + kThumbwheelButtonGap, s.drumW, s.buttonH));
        x += s.drumW;
        if (i + 1 < s.drums) {
            const int decade = i - (in.format.signDrum ? 1 : 0);
            if (decade >= 0 && decade == pointAfterDecade) {
                const double cx = x + s.pointGap * 0.5;
                l.decimalPoint = QRectF(cx - s.dot * 0.5, drum.bottom() - thumbwheelDrumPadY(in.text.digitPx) - s.dot,
                                        s.dot, s.dot);
                x += s.pointGap;
            } else {
                x += kThumbwheelDrumGap;
            }
        }
    }
    const double midY = drumTop + s.drumH * 0.5;
    if (s.unitW > 0.0) {
        x += kThumbwheelUnitGap;
        const double uh = std::max(nonNegative(in.text.tagHeight), 1.0);
        l.unit = QRectF(x, midY - uh * 0.5, s.unitW, uh);
        x += s.unitW;
    }
    x += kThumbwheelTagGap;
    l.tag = QRectF(x, drumTop, s.tagW, s.drumH);
    l.group = QRectF(x0, y0, s.groupW, s.groupH);
    return l;
}

QSizeF minimumThumbwheelSize(const ThumbwheelLayoutInput &in)
{
    const Sizes s = sizesOf(in);
    return QSizeF(std::ceil(s.groupW + 2.0 * kThumbwheelMargin), std::ceil(s.groupH + 2.0 * kThumbwheelMargin));
}

} // namespace internal
} // namespace QtIndustrialWidgets
