/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Accessible interface of IndustrialKnob (#183). Internal: never installed, never exported.

#include <QtGui/QAccessible>
#include <QtWidgets/QAccessibleWidget>

namespace QtIndustrialWidgets {

class IndustrialKnob;

namespace internal {

// Role Dial; the value text is the readout of the knob; the value interface exposes value, range and step, and takes
// a value set by assistive technology as operator input
class IndustrialKnobAccessible : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit IndustrialKnobAccessible(IndustrialKnob *knob);

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
    IndustrialKnob *knob() const;
};

} // namespace internal
} // namespace QtIndustrialWidgets
