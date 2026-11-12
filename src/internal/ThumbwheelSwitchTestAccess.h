/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of ThumbwheelSwitch (#154): what the last paint drew. Internal: never installed.

#include "internal/ThumbwheelSwitch_p.h"

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
};

} // namespace internal
} // namespace QtIndustrialWidgets
