/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of ThumbwheelSwitch (#154): what the last paint drew. Internal: never installed.

#include "internal/ThumbwheelSwitch_p.h"

#include <functional>
#include <utility>

namespace QtIndustrialWidgets {
namespace internal {

class ThumbwheelSwitchTestAccess
{
public:
    /// Layout of the last paintEvent() (an empty one before the first)
    static ThumbwheelLayout lastLayout(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastLayout; }
    /// What each drum showed, left to right: "0"–"9", "+", "−", or "-" for the dash of the overflow
    static QStringList lastDrums(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastDrums; }
    /// The mark of the tag column: "OVF", "▲", "▼" or empty
    static QString lastTag(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastTag; }
    /// Per drum, whether the arrow of its + / − button was filled (its step would be applied)
    static QVector<bool> lastPlusOpen(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastPlusOpen; }
    static QVector<bool> lastMinusOpen(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastMinusOpen; }
    /// Pixel size of the digits drawn by the last paint
    static int lastDigitPixelSize(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastDigitPx; }
    /// The drums had the dashed outline of a pending value
    static bool lastPendingOutline(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastPendingOutline; }
    /// The text of the status line ("SET?  now …"), empty without one or when it held the padlock
    static QString lastStatus(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastStatus; }
    static bool lastPadlock(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastPadlock; }
    /// The SET button was drawn, and drawn filled (a value was pending)
    static bool lastSetDrawn(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastSetDrawn; }
    static bool lastSetOpen(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastSetOpen; }
    /// The drum whose bar of the current decade was drawn, -1 for none
    static int lastCursor(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastCursor; }
    static QRectF lastFocusRing(const ThumbwheelSwitch &w) { return w.d_ptr->m_lastFocusRing; }
    /// The current decade (an index of the drums), drawn or not
    static int currentDrum(const ThumbwheelSwitch &w) { return w.d_ptr->m_current; }
    /// #183: \a observer sees every accessibility event the switch sends, before QAccessible::updateAccessibility()
    static void setAccessibleObserver(ThumbwheelSwitch &w, std::function<void(QAccessibleEvent *)> observer)
    {
        w.d_ptr->m_accessibleObserver = std::move(observer);
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
