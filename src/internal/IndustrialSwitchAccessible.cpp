// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "IndustrialSwitchAccessible.h"

#include <QtIndustrialWidgets/IndustrialSwitch.h>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets::internal {

namespace {

QAccessibleInterface *switchFactory(const QString &className, QObject *object)
{
    if (className == QLatin1String("QtIndustrialWidgets::IndustrialSwitch")) {
        if (auto *sw = qobject_cast<IndustrialSwitch *>(object)) {
            return new IndustrialSwitchAccessible(sw);
        }
    }
    return nullptr;
}

} // namespace

IndustrialSwitchAccessible::IndustrialSwitchAccessible(IndustrialSwitch *sw)
    : QAccessibleWidget(sw, QAccessible::CheckBox)
{
}

void IndustrialSwitchAccessible::installFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(switchFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

QString IndustrialSwitchAccessible::openGuardAction() { return QStringLiteral("Open guard"); }
QString IndustrialSwitchAccessible::closeGuardAction() { return QStringLiteral("Close guard"); }

IndustrialSwitch *IndustrialSwitchAccessible::control() const
{
    return static_cast<IndustrialSwitch *>(widget());
}

bool IndustrialSwitchAccessible::guardClosed() const
{
    return control()->hasSafetyGuard() && !control()->isGuardOpen();
}

void *IndustrialSwitchAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::ValueInterface) {
        return control()->positionCount() == 3 ? static_cast<QAccessibleValueInterface *>(this) : nullptr;
    }
    return QAccessibleWidget::interface_cast(type); // The action interface, among others
}

QAccessible::Role IndustrialSwitchAccessible::role() const
{
    return control()->positionCount() == 3 ? QAccessible::Slider : QAccessible::CheckBox;
}

QString IndustrialSwitchAccessible::text(QAccessible::Text t) const
{
    const IndustrialSwitch *sw = control();
    if (t == QAccessible::Name && sw->accessibleName().isEmpty() && !sw->label().isEmpty()) {
        return sw->label(); // The title engraved on the plate
    }
    if (t == QAccessible::Value) {
        const int position = sw->position();
        const QString label = position == 0                        ? sw->labelOff()
                              : position == sw->positionCount() - 1 ? sw->labelOn()
                                                                    : sw->labelCenter();
        return label.isEmpty() ? QString::number(position) : label;
    }
    return QAccessibleWidget::text(t);
}

QAccessible::State IndustrialSwitchAccessible::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    if (control()->positionCount() == 2) {
        s.checkable = true;
        s.checked = control()->isChecked();
    }
    s.readOnly = guardClosed();
    return s;
}

// The actions of the switch, then those of QAccessibleWidget (setFocus)
QStringList IndustrialSwitchAccessible::actionNames() const
{
    QStringList names;
    if (guardClosed()) {
        names = QStringList{openGuardAction()};
    } else {
        names = control()->positionCount() == 2 ? QStringList{toggleAction()}
                                                : QStringList{increaseAction(), decreaseAction()};
        if (control()->hasSafetyGuard()) {
            names.push_back(closeGuardAction());
        }
    }
    return names + QAccessibleWidget::actionNames();
}

// The actions do what the keys and the click do, and only while they are available
void IndustrialSwitchAccessible::doAction(const QString &actionName)
{
    if (!actionNames().contains(actionName)) {
        return;
    }
    IndustrialSwitch *sw = control();
    if (actionName == setFocusAction()) {
        QAccessibleWidget::doAction(actionName);
    } else if (actionName == openGuardAction()) {
        sw->setGuardOpen(true);
    } else if (actionName == closeGuardAction()) {
        sw->setGuardOpen(false);
    } else if (actionName == toggleAction()) {
        sw->toggle();
    } else if (actionName == increaseAction()) {
        sw->setPosition(std::min(sw->position() + 1, sw->positionCount() - 1));
    } else if (actionName == decreaseAction()) {
        sw->setPosition(std::max(sw->position() - 1, 0));
    }
}

QString IndustrialSwitchAccessible::localizedActionDescription(const QString &actionName) const
{
    if (actionName == openGuardAction()) {
        return QStringLiteral("Opens the safety guard, so that the switch can be moved");
    }
    if (actionName == closeGuardAction()) {
        return QStringLiteral("Closes the safety guard over the switch");
    }
    return QAccessibleWidget::localizedActionDescription(actionName);
}

QVariant IndustrialSwitchAccessible::currentValue() const { return control()->position(); }
QVariant IndustrialSwitchAccessible::minimumValue() const { return 0; }
QVariant IndustrialSwitchAccessible::maximumValue() const { return control()->positionCount() - 1; }
QVariant IndustrialSwitchAccessible::minimumStepSize() const { return 1; }

// Operator input: ignored while the guard is closed, rounded to a position and bound to the positions
void IndustrialSwitchAccessible::setCurrentValue(const QVariant &value)
{
    IndustrialSwitch *sw = control();
    if (guardClosed() || sw->positionCount() != 3) {
        return;
    }
    bool ok = false;
    const double proposed = value.toDouble(&ok);
    if (!ok || !std::isfinite(proposed)) {
        return;
    }
    const double bounded = std::clamp(std::round(proposed), 0.0, static_cast<double>(sw->positionCount() - 1));
    sw->setPosition(static_cast<int>(bounded));
}

} // namespace QtIndustrialWidgets::internal
