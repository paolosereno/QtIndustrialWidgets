/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Value model and geometry of ThumbwheelSwitch (#154), shared by the widget and its tests. Internal: never installed.

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

#include <optional>

namespace QtIndustrialWidgets {
namespace internal {

// ---------------------------------------------------------------------------------------------------------------
// Value model. A value is an integer count of the resolution 10^-decimals; the digits are its magnitude, most
// significant first, and the sign is that of the count (0 is +, there is no -0).
// ---------------------------------------------------------------------------------------------------------------

inline constexpr int kThumbwheelMaxDigits = 9;

/// The configuration of the drums.
struct ThumbwheelFormat {
    int digitCount{4}; ///< 1 to kThumbwheelMaxDigits
    int decimals{0};   ///< 0 to digitCount - 1
    bool signDrum{false};

    /// The largest magnitude the drums show, in counts: 10^digitCount - 1.
    [[nodiscard]] qint64 capacity() const;
    /// 10^decimals.
    [[nodiscard]] qint64 scale() const;
    /// True when the drums can show \a count: |count| <= capacity(), and count >= 0 without the sign drum.
    [[nodiscard]] bool canShow(qint64 count) const;
    /// The value of \a count.
    [[nodiscard]] double valueOf(qint64 count) const;
    /// The count nearest to \a value (halves away from zero); none for a non-finite value or one beyond the range
    /// of a count (the caller then reports it as not shown). It is not checked against canShow().
    [[nodiscard]] std::optional<qint64> countOf(double value) const;
    /// The digit of decade \a decade (0 = most significant drum) of the magnitude of \a count.
    [[nodiscard]] int digit(qint64 count, int decade) const;
    /// The count after one step of decade \a decade, \a direction +1 (the digit up) or -1 (the digit down): the
    /// magnitude changes by one unit of that decade, the sign stays. None when the digit is already 9 (up) or 0
    /// (down): no wrap-around, no carry (#154).
    [[nodiscard]] std::optional<qint64> stepDigit(qint64 count, int decade, int direction) const;
    /// The count with the sign \a negative. None for 0 (no -0) or without the sign drum; \a count itself when it
    /// already has that sign.
    [[nodiscard]] std::optional<qint64> withSign(qint64 count, bool negative) const;
};

/// Where a value is against the limits.
enum class ThumbwheelLimitState { Inside, Below, Above };

/// The limits of the operator's input, in values, as read within what the drums show.
struct ThumbwheelLimits {
    double low{0.0};
    double high{0.0};

    [[nodiscard]] ThumbwheelLimitState stateOf(double value) const;
    /// Distance of \a value to [low, high]; 0 inside.
    [[nodiscard]] double distance(double value) const;
    /// #182, #154: an operator step from \a from to \a to is applied when \a to is inside the limits, or, from a value
    /// outside them, when it comes closer to them; otherwise it is rejected.
    [[nodiscard]] bool allows(double from, double to) const;
};

// ---------------------------------------------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------------------------------------------

inline constexpr double kThumbwheelMargin = 4.0;      ///< Free space around the drawing (the focus ring is in it)
inline constexpr double kThumbwheelDrumGap = 3.0;     ///< Between two drums
inline constexpr double kThumbwheelButtonGap = 2.0;   ///< Between a drum and its buttons
inline constexpr double kThumbwheelMinButton = 12.0;  ///< Smallest height of a button
inline constexpr double kThumbwheelUnitGap = 6.0;     ///< Between the last drum and the unit
inline constexpr double kThumbwheelTagGap = 6.0;      ///< Before the tag column

/// Sizes of the texts of a ThumbwheelSwitch, measured by the widget with its fonts.
struct ThumbwheelTextSizes {
    double digitPx{0.0};     ///< Pixel size of the digit font
    double digitWidth{0.0};  ///< Width of the widest digit
    double digitHeight{0.0}; ///< Line height of the digit font
    double unitWidth{0.0};   ///< Width of the unit (0 without a unit)
    double tagWidth{0.0};    ///< Width of the widest mark of the tag column ("OVF", a triangle)
    double tagHeight{0.0};   ///< Line height of the tag font
};

/// Input of layoutThumbwheel().
struct ThumbwheelLayoutInput {
    double width{0.0};
    double height{0.0};
    ThumbwheelFormat format;
    ThumbwheelTextSizes text;
};

/**
 * \brief Geometry of a ThumbwheelSwitch.
 *
 * Left to right: the sign drum (when shown), the digit drums, most significant first, with the decimal point in a
 * wider gap, the unit, and the tag column, which is always reserved so that a mark never moves the drums. Top to
 * bottom: the + buttons, the drums, the − buttons. The group is centred in the widget.
 */
struct ThumbwheelLayout {
    QVector<QRectF> drums; ///< Every drum, left to right: the sign drum first when shown
    QVector<QRectF> plus;  ///< The + button of each drum, same order
    QVector<QRectF> minus; ///< The − button of each drum, same order
    bool signDrum{false};
    QRectF decimalPoint;   ///< The dot (empty without decimals)
    QRectF unit;           ///< The unit (empty without a unit)
    QRectF tag;            ///< The tag column, as high as the drums
    QRectF group;          ///< Everything above

    /// Index in drums of the digit drum of decade \a decade (0 = most significant).
    [[nodiscard]] int drumOfDecade(int decade) const { return decade + (signDrum ? 1 : 0); }
};

/// Padding of a drum around its digit, horizontal and vertical, for a digit font of \a digitPx pixels.
double thumbwheelDrumPadX(double digitPx);
double thumbwheelDrumPadY(double digitPx);

/**
 * \brief Lays out a ThumbwheelSwitch of the given size. The parts keep the size the texts need; a widget smaller
 * than minimumThumbwheelSize() clips them, larger centres them. No value is ever non-finite, no rectangle has a
 * negative size, and the drums, buttons and gaps fall on whole logical pixels.
 */
ThumbwheelLayout layoutThumbwheel(const ThumbwheelLayoutInput &in);

/// \brief Smallest widget size at which nothing of layoutThumbwheel() is clipped or overlapping (whole pixels).
QSizeF minimumThumbwheelSize(const ThumbwheelLayoutInput &in);

} // namespace internal
} // namespace QtIndustrialWidgets
