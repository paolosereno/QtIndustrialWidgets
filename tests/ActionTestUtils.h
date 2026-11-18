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

// #243: disables \a target (the control itself or one of its parents) and returns true when the control received
// what the case means. With \a focused, the control had the focus, and Qt took it away while the control was already
// disabled, before the EnabledChange: that focus out is part of the disable, not an ordinary loss of focus. Without
// \a focused, the control had no focus and got the EnabledChange only. Returns false, without disabling anything, when
// the control is not visible, already disabled, or has the focus when it should not (or the other way round). Use it
// as QVERIFY(disable(control, target, focused)).
[[nodiscard]] inline bool disable(QWidget &control, QWidget &target, bool focused)
{
    if (!control.isVisible() || !control.isEnabled() || control.hasFocus() != focused) {
        return false;
    }
    struct Watch : QObject {
        QWidget *control{nullptr};
        bool enabledChange{false};
        int focusOuts{0};
        bool focusOutWhileEnabled{false};
        bool focusOutAfterEnabledChange{false};
        bool eventFilter(QObject *, QEvent *event) override
        {
            if (event->type() == QEvent::EnabledChange) {
                enabledChange = true;
            } else if (event->type() == QEvent::FocusOut) {
                ++focusOuts;
                focusOutWhileEnabled = focusOutWhileEnabled || control->isEnabled();
                focusOutAfterEnabledChange = focusOutAfterEnabledChange || enabledChange;
            }
            return false;
        }
    } watch;
    watch.control = &control;
    control.installEventFilter(&watch);
    target.setEnabled(false);
    control.removeEventFilter(&watch);
    if (control.isEnabled() || !watch.enabledChange) {
        return false;
    }
    if (!focused) {
        return watch.focusOuts == 0;
    }
    return watch.focusOuts == 1 && !watch.focusOutWhileEnabled && !watch.focusOutAfterEnabledChange;
}

} // namespace ActionTest
