/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Accessible interface of IndustrialSlider (#180, #183). Internal: never installed, never exported.

#include <QtGui/QAccessible>
#include <QtWidgets/QAccessibleWidget>

namespace QtIndustrialWidgets {

class IndustrialSlider;

namespace internal {

// Role Slider. The value interface carries the setpoint, the range and step(), and takes a value set by assistive
// technology as operator input; the value text also tells the feedback and the deviation state
class IndustrialSliderAccessible : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit IndustrialSliderAccessible(IndustrialSlider *slider);

    // Registers the factory of the interface, once per process
    static void installFactory();

    void *interface_cast(QAccessible::InterfaceType type) override;
    QString text(QAccessible::Text t) const override;
    QAccessible::State state() const override;

    QVariant currentValue() const override;
    void setCurrentValue(const QVariant &value) override;
    QVariant maximumValue() const override;
    QVariant minimumValue() const override;
    QVariant minimumStepSize() const override;

private:
    IndustrialSlider *slider() const;
};

} // namespace internal
} // namespace QtIndustrialWidgets
