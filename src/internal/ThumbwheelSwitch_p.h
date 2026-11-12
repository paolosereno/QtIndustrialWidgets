/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of ThumbwheelSwitch (d-pointer), #154. Internal: never installed.

#include <QtIndustrialWidgets/ThumbwheelSwitch.h>

#include "internal/PanelFonts.h"
#include "internal/ThumbwheelSwitchLayout.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QFont>

namespace QtIndustrialWidgets {

class ThumbwheelSwitchPrivate
{
public:
    internal::ThumbwheelFormat m_format;
    qint64 m_count{0};             // The value, in counts of the resolution
    bool m_rejectedOverflow{false}; // The last setValue() got a value the drums cannot show
    double m_minimum{0.0};         // As set; read within what the drums show
    double m_maximum{9999.0};
    QString m_unit;
    QColor m_drumColor{QColor(20, 24, 30)};
    QColor m_digitColor{QColor(240, 244, 248)};
    QColor m_buttonColor{QColor(52, 60, 72)};
    QColor m_textColor{QColor(240, 244, 248)};
    // Logical DPI of the last minimumSizeHint() (0: none since the last updateGeometry()), #177
    mutable double m_minimumDpi{0.0};

    // What the last paint drew, read by the tests (ThumbwheelSwitchTestAccess)
    internal::ThumbwheelLayout m_lastLayout;
    QStringList m_lastDrums;       // What each drum shows: "0"–"9", "+", "−", or "-" (the dash of the overflow)
    QString m_lastTag;             // "OVF", "▲", "▼" or empty
    QVector<bool> m_lastPlusOpen;  // Per drum: the + arrow is filled (its step would be applied)
    QVector<bool> m_lastMinusOpen; // The same for the − arrow
    int m_lastDigitPx{0};

    [[nodiscard]] double value() const { return m_format.valueOf(m_count); }
    [[nodiscard]] bool overflow() const { return m_rejectedOverflow || !m_format.canShow(m_count); }
    /// The limits as read: as set, within what the drums show
    [[nodiscard]] internal::ThumbwheelLimits limits() const;
    /// The count after one operator step of drum \a drum (an index of ThumbwheelLayout::drums), \a direction +1 (the
    /// + button) or -1 (the − button); none when the step is rejected (see the class description)
    [[nodiscard]] std::optional<qint64> stepOf(int drum, int direction) const;

    // Fonts of a widget font at a logical DPI: label = the unit and the tags, readout = the digits
    using Fonts = internal::PanelFonts;
    static Fonts fontsFor(const QFont &widgetFont, double logicalDpi);
    [[nodiscard]] internal::ThumbwheelLayoutInput layoutInput(double w, double h, const Fonts &fonts) const;
};

} // namespace QtIndustrialWidgets
