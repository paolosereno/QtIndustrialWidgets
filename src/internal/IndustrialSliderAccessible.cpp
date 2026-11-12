// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "IndustrialSliderAccessible.h"

#include <QtIndustrialWidgets/IndustrialSlider.h>
#include "IndustrialSlider_p.h"
#include "NumberFormat.h"

#include <QtCore/QStringList>

#include <cmath>

namespace QtIndustrialWidgets::internal {

namespace {

QAccessibleInterface *sliderFactory(const QString &className, QObject *object)
{
    if (className == QLatin1String("QtIndustrialWidgets::IndustrialSlider")) {
        if (auto *s = qobject_cast<IndustrialSlider *>(object)) {
            return new IndustrialSliderAccessible(s);
        }
    }
    return nullptr;
}

// A number of the slider with its unit, as the readouts print it
QString withUnit(const IndustrialSliderPrivate &d, double v)
{
    return QStringLiteral("%1 %2").arg(internal::formatFixed(v, d.m_precision), d.m_unit).trimmed();
}

// The value text: the setpoint; with a feedback, its position; then the deviation state when it is not Off or
// Normal. In words, not the marks the readouts draw ("---", the triangles, "Δ", "?"), which a screen reader reads
// badly
QString valueText(const IndustrialSliderPrivate &d)
{
    QStringList parts{withUnit(d, d.m_setpoint)};
    if (d.m_hasFeedback) {
        switch (d.m_feedbackState) {
        case ValueState::Valid:
            parts << QStringLiteral("position %1").arg(withUnit(d, d.m_rawFeedback));
            break;
        case ValueState::Overrange:
        case ValueState::Underrange: {
            const QString where = d.m_feedbackState == ValueState::Overrange ? QStringLiteral("over range")
                                                                             : QStringLiteral("under range");
            // The input itself when it is finite, as the readout shows it; ±Inf has no number
            parts << (std::isfinite(d.m_rawFeedback)
                          ? QStringLiteral("position %1 %2").arg(where, withUnit(d, d.m_rawFeedback))
                          : QStringLiteral("position %1").arg(where));
            break;
        }
        case ValueState::Invalid:
            parts << QStringLiteral("position invalid");
            break;
        }
    }
    using DS = IndustrialSlider::DeviationState;
    if (d.m_deviationState == DS::Deviating) {
        parts << QStringLiteral("deviation");
    } else if (d.m_deviationState == DS::Unknown) {
        parts << QStringLiteral("deviation unknown");
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace

IndustrialSliderAccessible::IndustrialSliderAccessible(IndustrialSlider *slider)
    : QAccessibleWidget(slider, QAccessible::Slider)
{
}

void IndustrialSliderAccessible::installFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(sliderFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

IndustrialSlider *IndustrialSliderAccessible::slider() const
{
    return static_cast<IndustrialSlider *>(widget());
}

void *IndustrialSliderAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::ValueInterface) {
        return static_cast<QAccessibleValueInterface *>(this);
    }
    return QAccessibleWidget::interface_cast(type);
}

QString IndustrialSliderAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value) {
        return valueText(*slider()->d_ptr); // Also when the readouts are hidden
    }
    return QAccessibleWidget::text(t); // Name: accessibleName (or the buddy label); none made up from the tags
}

QAccessible::State IndustrialSliderAccessible::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    s.readOnly = slider()->isReadOnly();
    return s;
}

QVariant IndustrialSliderAccessible::currentValue() const { return slider()->setpoint(); }
QVariant IndustrialSliderAccessible::maximumValue() const { return slider()->maximum(); }
QVariant IndustrialSliderAccessible::minimumValue() const { return slider()->minimum(); }
QVariant IndustrialSliderAccessible::minimumStepSize() const { return slider()->step(); }

// Operator input, as a key: ignored in read-only, bound by the setpoint limits, and committed
void IndustrialSliderAccessible::setCurrentValue(const QVariant &value)
{
    IndustrialSlider *s = slider();
    if (s->isReadOnly()) {
        return;
    }
    bool ok = false;
    const double proposed = value.toDouble(&ok);
    if (!ok || !std::isfinite(proposed)) {
        return;
    }
    const double before = s->setpoint();
    s->setSetpoint(s->d_ptr->operatorTarget(proposed));
    if (s->setpoint() != before) {
        Q_EMIT s->setpointCommitted(s->setpoint());
    }
}

} // namespace QtIndustrialWidgets::internal
