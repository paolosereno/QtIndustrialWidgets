// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "ThumbwheelSwitchAccessible.h"

#include <QtIndustrialWidgets/ThumbwheelSwitch.h>
#include "ThumbwheelSwitch_p.h"

#include <QtCore/QStringList>

#include <cmath>

namespace QtIndustrialWidgets::internal {

namespace {

QAccessibleInterface *thumbwheelFactory(const QString &className, QObject *object)
{
    if (className == QLatin1String("QtIndustrialWidgets::ThumbwheelSwitch")) {
        if (auto *w = qobject_cast<ThumbwheelSwitch *>(object)) {
            return new ThumbwheelSwitchAccessible(w);
        }
    }
    return nullptr;
}

// A count as the drums show it (sign, every digit, the decimal point), with the unit
QString withUnit(const ThumbwheelSwitchPrivate &d, qint64 count)
{
    return QStringLiteral("%1 %2").arg(d.m_format.text(count), d.m_unit).trimmed();
}

// The value text: the value the drums show, then where it is against the limits when it is outside them (the
// triangle of the tag column), then, while a value is pending, the applied one: "pending 0130.0 V, set 0125.0 V".
// In overflow the drums show no value: "overflow". In words, not the marks drawn ("OVF", "▲", "▼", "SET?")
QString valueText(const ThumbwheelSwitchPrivate &d)
{
    if (d.overflow()) {
        return QStringLiteral("overflow");
    }
    QStringList parts{withUnit(d, d.shownCount())};
    if (d.m_pending) {
        parts.front().prepend(QStringLiteral("pending "));
    }
    switch (d.limits().stateOf(d.shownValue())) {
    case ThumbwheelLimitState::Above:
        parts << QStringLiteral("above maximum");
        break;
    case ThumbwheelLimitState::Below:
        parts << QStringLiteral("below minimum");
        break;
    case ThumbwheelLimitState::Inside:
        break;
    }
    if (d.m_pending) {
        parts << QStringLiteral("set %1").arg(withUnit(d, d.m_count));
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace

ThumbwheelSwitchAccessible::ThumbwheelSwitchAccessible(ThumbwheelSwitch *wheel)
    : QAccessibleWidget(wheel, QAccessible::SpinBox)
{
}

void ThumbwheelSwitchAccessible::installFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(thumbwheelFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

QString ThumbwheelSwitchAccessible::applyAction() { return QStringLiteral("Apply"); }
QString ThumbwheelSwitchAccessible::discardAction() { return QStringLiteral("Discard"); }

ThumbwheelSwitch *ThumbwheelSwitchAccessible::wheel() const
{
    return static_cast<ThumbwheelSwitch *>(widget());
}

void *ThumbwheelSwitchAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::ValueInterface) {
        return static_cast<QAccessibleValueInterface *>(this);
    }
    return QAccessibleWidget::interface_cast(type); // The action interface, among others
}

QString ThumbwheelSwitchAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value) {
        return valueText(*wheel()->d_ptr);
    }
    return QAccessibleWidget::text(t); // Name: accessibleName (or the buddy label); none made up from the unit
}

QAccessible::State ThumbwheelSwitchAccessible::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    s.readOnly = wheel()->isReadOnly();
    return s;
}

// Apply and Discard while a value is pending (never in read-only or in overflow, where none is), then those of
// QAccessibleWidget (setFocus)
QStringList ThumbwheelSwitchAccessible::actionNames() const
{
    QStringList names;
    if (wheel()->hasPendingValue()) {
        names = QStringList{applyAction(), discardAction()};
    }
    return names + QAccessibleWidget::actionNames();
}

// The actions do what Enter (or the SET button) and Escape do, and only while they are available
void ThumbwheelSwitchAccessible::doAction(const QString &actionName)
{
    if (!actionNames().contains(actionName)) {
        return;
    }
    if (actionName == applyAction()) {
        wheel()->applyPending();
    } else if (actionName == discardAction()) {
        wheel()->discardPending();
    } else {
        QAccessibleWidget::doAction(actionName);
    }
}

QString ThumbwheelSwitchAccessible::localizedActionDescription(const QString &actionName) const
{
    if (actionName == applyAction()) {
        return QStringLiteral("Applies the pending value");
    }
    if (actionName == discardAction()) {
        return QStringLiteral("Discards the pending value; the set value stays");
    }
    return QAccessibleWidget::localizedActionDescription(actionName);
}

QVariant ThumbwheelSwitchAccessible::currentValue() const { return wheel()->value(); }
QVariant ThumbwheelSwitchAccessible::maximumValue() const { return wheel()->maximum(); }
QVariant ThumbwheelSwitchAccessible::minimumValue() const { return wheel()->minimum(); }
QVariant ThumbwheelSwitchAccessible::minimumStepSize() const
{
    return std::pow(10.0, -wheel()->decimals()); // The resolution
}

// Operator input, as the drums take it: rounded to the resolution, rejected (never bound to a limit) when the drums
// cannot show it or when it is outside the limits or further out from a value outside them, ignored in read-only and
// in overflow. Immediate applies and commits it; OnConfirm makes it the pending value
void ThumbwheelSwitchAccessible::setCurrentValue(const QVariant &value)
{
    ThumbwheelSwitch *w = wheel();
    ThumbwheelSwitchPrivate &priv = *w->d_ptr; // Not "d": QAccessibleWidget has a member d (MSVC C4458)
    bool ok = false;
    const double proposed = value.toDouble(&ok);
    if (!ok || !std::isfinite(proposed)) {
        return;
    }
    const std::optional<qint64> target = priv.operatorTarget(priv.m_format.countOf(proposed));
    if (!target || *target == priv.shownCount()) {
        return;
    }
    priv.setShown(w, *target);
    if (w->applyMode() == ThumbwheelSwitch::ApplyMode::Immediate) {
        Q_EMIT w->valueCommitted(w->value());
    }
}

} // namespace QtIndustrialWidgets::internal
