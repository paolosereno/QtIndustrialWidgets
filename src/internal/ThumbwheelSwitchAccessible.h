/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Accessible interface of ThumbwheelSwitch (#154, #183). Internal: never installed, never exported.

#include <QtGui/QAccessible>
#include <QtWidgets/QAccessibleWidget>

namespace QtIndustrialWidgets {

class ThumbwheelSwitch;

namespace internal {

// Role SpinBox. The value interface carries value(), the limits and the resolution, and takes a value set by assistive
// technology as operator input. The value text is the value as the drums show it, with the pending value, the limit
// state or the overflow in words. While a value is pending, the actions Apply and Discard apply or discard it
class ThumbwheelSwitchAccessible : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit ThumbwheelSwitchAccessible(ThumbwheelSwitch *wheel);

    // Registers the factory of the interface, once per process
    static void installFactory();

    static QString applyAction();
    static QString discardAction();

    void *interface_cast(QAccessible::InterfaceType type) override;
    QString text(QAccessible::Text t) const override;
    QAccessible::State state() const override;

    // QAccessibleActionInterface, through QAccessibleWidget
    QStringList actionNames() const override;
    void doAction(const QString &actionName) override;
    QString localizedActionDescription(const QString &actionName) const override;

    QVariant currentValue() const override;
    void setCurrentValue(const QVariant &value) override;
    QVariant maximumValue() const override;
    QVariant minimumValue() const override;
    QVariant minimumStepSize() const override;

private:
    ThumbwheelSwitch *wheel() const;
};

} // namespace internal
} // namespace QtIndustrialWidgets
