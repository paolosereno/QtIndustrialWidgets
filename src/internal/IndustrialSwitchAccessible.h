/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Accessible interface of IndustrialSwitch (#183). Internal: never installed, never exported.

#include <QtGui/QAccessible>
#include <QtWidgets/QAccessibleWidget>

namespace QtIndustrialWidgets {

class IndustrialSwitch;

namespace internal {

// Two positions: role CheckBox, checked in the ON position, action toggle. Three positions: role Slider, value
// interface over the positions, actions increase and decrease. The value text is the label of the position. While the
// guard is closed the switch is read-only and its only action is "Open guard"; open, it also has "Close guard"
class IndustrialSwitchAccessible : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit IndustrialSwitchAccessible(IndustrialSwitch *sw);

    // Registers the factory of the interface, once per process
    static void installFactory();

    static QString openGuardAction();
    static QString closeGuardAction();

    void *interface_cast(QAccessible::InterfaceType type) override;
    QAccessible::Role role() const override;
    QString text(QAccessible::Text t) const override;
    QAccessible::State state() const override;

    // QAccessibleActionInterface, through QAccessibleWidget
    QStringList actionNames() const override;
    void doAction(const QString &actionName) override;
    QString localizedActionDescription(const QString &actionName) const override;

    // QAccessibleValueInterface, three positions only
    QVariant currentValue() const override;
    void setCurrentValue(const QVariant &value) override;
    QVariant maximumValue() const override;
    QVariant minimumValue() const override;
    QVariant minimumStepSize() const override;

private:
    IndustrialSwitch *control() const;
    bool guardClosed() const;
};

} // namespace internal
} // namespace QtIndustrialWidgets
