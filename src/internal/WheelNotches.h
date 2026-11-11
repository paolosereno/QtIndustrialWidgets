/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace QtIndustrialWidgets {
namespace internal {

/// One notch of a mouse wheel in QWheelEvent::angleDelta() units.
inline constexpr int kWheelNotch = 120;

/**
 * \brief Adds \a angleDelta to \a remainder and takes the whole notches out of it, #182: the controls step once per
 * notch, summed over the smaller events of a high-resolution wheel or a touchpad.
 *
 * Returns the notches taken, with the sign of the movement (positive away from the user); \a remainder keeps the
 * part of a notch not yet reached, with its sign (it never reaches a whole notch).
 */
inline int takeWheelNotches(int &remainder, int angleDelta)
{
    remainder += angleDelta;
    const int notches = remainder / kWheelNotch; // Towards zero: the remainder keeps its sign
    remainder -= notches * kWheelNotch;
    return notches;
}

/**
 * \brief The wheel of a control, #182: adds \a angleDelta to \a remainder; if that reaches a whole notch, calls
 * \a endAction (a drag or a held key running ends before the wheel steps), then \a stepOnce(direction) once per
 * notch, with direction +1 or -1. Each step commits on its own.
 */
template <typename EndAction, typename StepOnce>
void applyWheelNotches(int &remainder, int angleDelta, EndAction endAction, StepOnce stepOnce)
{
    const int notches = takeWheelNotches(remainder, angleDelta);
    if (notches == 0) {
        return;
    }
    endAction();
    const int direction = notches > 0 ? 1 : -1;
    for (int i = 0; i < notches * direction; ++i) {
        stepOnce(direction);
    }
}

} // namespace internal
} // namespace QtIndustrialWidgets
