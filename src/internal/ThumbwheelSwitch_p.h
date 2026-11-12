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
#include <QtGui/QAccessible>
#include <QtGui/QColor>
#include <QtGui/QFont>

#include <functional>
#include <optional>

namespace QtIndustrialWidgets {

class ThumbwheelSwitchPrivate
{
public:
    internal::ThumbwheelFormat m_format;
    qint64 m_count{0};             // The value (applied), in counts of the resolution
    std::optional<qint64> m_pending; // The pending value (OnConfirm), never equal to m_count, none in overflow
    bool m_rejectedOverflow{false}; // The last setValue() got a value the drums cannot show
    double m_minimum{0.0};         // As set; read within what the drums show
    double m_maximum{9999.0};
    QString m_unit;
    QColor m_drumColor{QColor(20, 24, 30)};
    QColor m_digitColor{QColor(240, 244, 248)};
    QColor m_buttonColor{QColor(52, 60, 72)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_pendingColor{QColor(255, 176, 0)};
    ThumbwheelSwitch::ApplyMode m_applyMode{ThumbwheelSwitch::ApplyMode::OnConfirm};
    bool m_readOnly{false};
    bool m_wheelEnabled{false};
    int m_wheelRemainder{0}; // Part of a wheel notch not yet reached (internal::takeWheelNotches)
    int m_current{0};        // The current decade: an index of ThumbwheelLayout::drums

    // A button armed by a press: its release on the same button acts (#182)
    enum class Part { None, Plus, Minus, Set };
    Part m_armed{Part::None};
    int m_armedDrum{-1};

    // A key action (#182): from the first press of a key to its release that is not an auto-repeat
    bool m_keyAction{false};
    int m_actionKey{0};
    qint64 m_actionStartCount{0};
    // Logical DPI of the last minimumSizeHint() (0: none since the last updateGeometry()), #177
    mutable double m_minimumDpi{0.0};

    // What the last paint drew, read by the tests (ThumbwheelSwitchTestAccess)
    internal::ThumbwheelLayout m_lastLayout;
    QStringList m_lastDrums;       // What each drum shows: "0"–"9", "+", "−", or "-" (the dash of the overflow)
    QString m_lastTag;             // "OVF", "▲", "▼" or empty
    QVector<bool> m_lastPlusOpen;  // Per drum: the + arrow is filled (its step would be applied)
    QVector<bool> m_lastMinusOpen; // The same for the − arrow
    int m_lastDigitPx{0};
    bool m_lastPendingOutline{false}; // The drums had the dashed outline of a pending value
    QString m_lastStatus;             // The text of the status line (empty without one, or for the padlock)
    bool m_lastPadlock{false};        // The padlock was drawn
    bool m_lastSetOpen{false};        // The SET button was drawn filled (a value is pending)
    bool m_lastSetDrawn{false};
    int m_lastCursor{-1};             // The drum whose bar of the current decade was drawn (-1: none)
    QRectF m_lastFocusRing;

    [[nodiscard]] double value() const { return m_format.valueOf(m_count); }
    /// The count the drums show: the pending one, or the applied one
    [[nodiscard]] qint64 shownCount() const { return m_pending.value_or(m_count); }
    [[nodiscard]] double shownValue() const { return m_format.valueOf(shownCount()); }
    [[nodiscard]] int drumCount() const { return m_format.digitCount + (m_format.signDrum ? 1 : 0); }
    [[nodiscard]] bool isSignDrum(int drum) const { return m_format.signDrum && drum == 0; }
    [[nodiscard]] bool overflow() const { return m_rejectedOverflow || !m_format.canShow(m_count); }
    /// The limits as read: as set, within what the drums show
    [[nodiscard]] internal::ThumbwheelLimits limits() const;
    /// The count after one operator step of drum \a drum (an index of ThumbwheelLayout::drums), \a direction +1 (the
    /// + button) or -1 (the − button); none when the step is rejected (see the class description)
    [[nodiscard]] std::optional<qint64> stepOf(int drum, int direction) const;
    /// The count with the digit of drum \a drum typed as \a digit; none when it is rejected as a step would be
    [[nodiscard]] std::optional<qint64> digitOf(int drum, int digit) const;
    /// The count after an operator change to \a next is checked (shown, inside the limits or closer to them); none
    /// when it is rejected, in read-only and in overflow
    [[nodiscard]] std::optional<qint64> operatorTarget(std::optional<qint64> next) const;

    /// Operator input: the drums take \a count; applied at once (Immediate) or pending (OnConfirm)
    void setShown(ThumbwheelSwitch *q, qint64 count);
    /// Applies the pending value, with valueChanged() and valueCommitted(); false when none is pending
    bool apply(ThumbwheelSwitch *q);
    /// Ends a key action running; in Immediate, commits if it changed the value
    void endAction(ThumbwheelSwitch *q);
    /// The layout at the size and fonts of \a q now
    [[nodiscard]] internal::ThumbwheelLayout currentLayout(const ThumbwheelSwitch *q) const;

    // Fonts of a widget font at a logical DPI: label = the unit and the tags, readout = the digits
    using Fonts = internal::PanelFonts;
    static Fonts fontsFor(const QFont &widgetFont, double logicalDpi);
    [[nodiscard]] internal::ThumbwheelLayoutInput layoutInput(double w, double h, const Fonts &fonts) const;
    /// The text of the status line while a value is pending: "SET?  now " and the applied value with its unit
    [[nodiscard]] QString statusText(QChar digitChar = QChar()) const;

    // #183: every accessibility event of the switch goes through here. The observer is set by tests only
    // (ThumbwheelSwitchTestAccess): under QT_QPA_PLATFORM=offscreen Qt drops the events before any update handler
    std::function<void(QAccessibleEvent *)> m_accessibleObserver;
    void sendAccessible(QAccessibleEvent *event) const
    {
        if (m_accessibleObserver) {
            m_accessibleObserver(event);
        }
        QAccessible::updateAccessibility(event); // Nothing while no assistive technology is active (#214)
    }
    // #183: the value, the pending value, the overflow or the limits changed: the value text changed
    void notifyValue(ThumbwheelSwitch *q) const
    {
        QAccessibleValueChangeEvent event(q, value());
        sendAccessible(&event);
    }
    // #183: the read-only state changed
    void notifyReadOnly(ThumbwheelSwitch *q) const
    {
        QAccessible::State changed;
        changed.readOnly = true;
        QAccessibleStateChangeEvent event(q, changed);
        sendAccessible(&event);
    }
};

} // namespace QtIndustrialWidgets
