// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

// #241: ending an operator action of a control (IndustrialKnob, IndustrialSlider, ThumbwheelSwitch) by a hide.
//
// A focused widget that is hidden loses the focus first, and its focus out ends the action before the hide does: a
// test that hides a focused control checks the focus out, not the hide (missed in #205, #233 and #241). A widget
// that was never shown does not get a hide event either. So a test of the hide gives the focus to another widget, or
// uses a control with Qt::NoFocus, sends the events to the control directly, and hides it with hideWithoutFocus().

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtWidgets/QWidget>

namespace ActionTest {

// Hides \a widget and returns true when only the hide can have ended an action: the widget was visible, had no focus,
// received a hide event and no focus out. Returns false, without hiding it, when it is not visible or has the focus;
// false also when a focus out reached it during the hide. Use it as QVERIFY(hideWithoutFocus(w)).
[[nodiscard]] inline bool hideWithoutFocus(QWidget &widget)
{
    if (!widget.isVisible() || widget.hasFocus()) {
        return false;
    }
    struct Watch : QObject {
        bool hidden{false};
        bool focusOut{false};
        bool eventFilter(QObject *, QEvent *event) override
        {
            hidden = hidden || event->type() == QEvent::Hide;
            focusOut = focusOut || event->type() == QEvent::FocusOut;
            return false;
        }
    } watch;
    widget.installEventFilter(&watch);
    widget.hide();
    widget.removeEventFilter(&watch);
    return watch.hidden && !watch.focusOut;
}

} // namespace ActionTest
