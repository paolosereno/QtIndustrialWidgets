// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

// #183, part of #231: what a control sends to assistive technology, through the test hook of its *TestAccess
// (offscreen Qt drops the events before any handler). Each event as one line: "value 40" for a value change of the
// control, "readOnly" for a change of its read-only state, "other" for anything else. Used by the tests of
// IndustrialSlider and ThumbwheelSwitch.

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QAccessible>

#include <utility>

template <typename Access, typename Widget>
class AccessibleEventLog
{
public:
    explicit AccessibleEventLog(Widget &widget)
        : m_widget(widget)
    {
        Access::setAccessibleObserver(widget, [this](QAccessibleEvent *e) { m_lines << describe(e); });
    }
    ~AccessibleEventLog() { Access::setAccessibleObserver(m_widget, {}); }
    AccessibleEventLog(const AccessibleEventLog &) = delete;
    AccessibleEventLog &operator=(const AccessibleEventLog &) = delete;

    // The lines since the last call, joined by "; "
    QString flush() { return std::exchange(m_lines, {}).join(QStringLiteral("; ")); }

private:
    QString describe(QAccessibleEvent *e) const
    {
        if (e->object() != &m_widget) {
            return QStringLiteral("other");
        }
        if (e->type() == QAccessible::ValueChanged) {
            return QStringLiteral("value %1").arg(static_cast<QAccessibleValueChangeEvent *>(e)->value().toDouble());
        }
        if (e->type() == QAccessible::StateChanged
            && static_cast<QAccessibleStateChangeEvent *>(e)->changedStates().readOnly) {
            return QStringLiteral("readOnly");
        }
        return QStringLiteral("other");
    }

    Widget &m_widget;
    QStringList m_lines;
};
