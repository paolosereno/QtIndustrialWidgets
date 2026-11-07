// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "IndustrialKnobAccessible.h"

#include <QtIndustrialWidgets/IndustrialKnob.h>
#include "IndustrialKnob_p.h"

#include <cmath>

namespace QtIndustrialWidgets::internal {

namespace {

QAccessibleInterface *knobFactory(const QString &className, QObject *object)
{
    if (className == QLatin1String("QtIndustrialWidgets::IndustrialKnob")) {
        if (auto *knob = qobject_cast<IndustrialKnob *>(object)) {
            return new IndustrialKnobAccessible(knob);
        }
    }
    return nullptr;
}

} // namespace

IndustrialKnobAccessible::IndustrialKnobAccessible(IndustrialKnob *knob)
    : QAccessibleWidget(knob, QAccessible::Dial)
{
}

void IndustrialKnobAccessible::installFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(knobFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

IndustrialKnob *IndustrialKnobAccessible::knob() const
{
    return static_cast<IndustrialKnob *>(widget());
}

void *IndustrialKnobAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::ValueInterface) {
        return static_cast<QAccessibleValueInterface *>(this);
    }
    return QAccessibleWidget::interface_cast(type);
}

QString IndustrialKnobAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value) {
        return knob()->d_ptr->readoutText(); // As the readout draws it, also when the readout is hidden
    }
    return QAccessibleWidget::text(t); // Name: accessibleName (or the buddy label); no name made up from the unit
}

QAccessible::State IndustrialKnobAccessible::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    s.readOnly = knob()->isReadOnly();
    return s;
}

QVariant IndustrialKnobAccessible::currentValue() const { return knob()->value(); }
QVariant IndustrialKnobAccessible::maximumValue() const { return knob()->maximum(); }
QVariant IndustrialKnobAccessible::minimumValue() const { return knob()->minimum(); }

QVariant IndustrialKnobAccessible::minimumStepSize() const
{
    const IndustrialKnob *k = knob();
    if (k->mode() == IndustrialKnob::KnobMode::Discrete) {
        return (k->maximum() - k->minimum()) / (k->discreteSteps() - 1); // discreteSteps() is at least 2
    }
    return k->step();
}

// Operator input: ignored in read-only, bound by the value limits like a key or the wheel, and committed
void IndustrialKnobAccessible::setCurrentValue(const QVariant &value)
{
    IndustrialKnob *k = knob();
    if (k->isReadOnly()) {
        return;
    }
    bool ok = false;
    const double proposed = value.toDouble(&ok);
    if (!ok || !std::isfinite(proposed)) {
        return;
    }
    const double before = k->value();
    k->setValue(k->d_ptr->operatorTarget(proposed));
    if (!qFuzzyCompare(k->value(), before)) {
        Q_EMIT k->valueCommitted(k->value());
    }
}

} // namespace QtIndustrialWidgets::internal
