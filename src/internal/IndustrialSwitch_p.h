/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of IndustrialSwitch, in a header of its own so that IndustrialSwitchTestAccess can read it (#183).
// Internal: never installed.

#include <QtIndustrialWidgets/IndustrialSwitch.h>

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

#include <functional>

namespace QtIndustrialWidgets {

class IndustrialSwitchPrivate {
public:
    // #44: size hints computed from the font metrics, cached for the inputs they depend on
    mutable QString m_hintKey;
    mutable QSize m_minimumHint;
    mutable QSize m_preferredHint;

    IndustrialSwitch::SwitchType m_switchType = IndustrialSwitch::SwitchType::ToggleLever;
    int m_positionCount = 2; // 2 or 3
    int m_position = 0;      // 0, 1 (or 2 if 3-pos)
    double m_currentPos = 0.0; // for animation: 0.0 to 1.0 (or 2.0)
    Qt::Orientation m_orientation = Qt::Vertical;

    bool m_hasSafetyGuard = false;
    bool m_isGuardOpen = false;
    double m_guardOpenFactor = 0.0; // 0.0 = closed, 1.0 = fully open
    bool m_animated = true;
    bool m_hasLed = true;

    QString m_label;
    QString m_labelOff = QStringLiteral("OFF");
    QString m_labelOn = QStringLiteral("ON");
    QString m_labelCenter = QStringLiteral("AUTO");

    QColor m_plateColor = QColor(42, 45, 52);
    QColor m_leverColor = QColor(220, 225, 230);
    QColor m_ledColor = QColor(46, 204, 113);
    QColor m_textColor = QColor(200, 205, 215);
    QColor m_guardColor = QColor(220, 53, 69); // Industrial crimson safety red

    QVariantAnimation *m_switchAnim = nullptr;
    QVariantAnimation *m_guardAnim = nullptr;

    QPixmap m_cachedBackground;
    bool m_cacheValid = false;
    bool m_isDragging = false;

    // #183: every accessibility event of the switch goes through here. The observer is set by tests only
    // (IndustrialSwitchTestAccess): without an accessibility backend (QT_QPA_PLATFORM=offscreen) Qt drops the events
    // before any update handler, so this is the only point where a test can see them
    std::function<void(QAccessibleEvent *)> m_accessibleObserver;
    void sendAccessible(QAccessibleEvent *event)
    {
        if (m_accessibleObserver) {
            m_accessibleObserver(event);
        }
        // Does nothing while no assistive technology is active. Not covered by the tests: under
        // QT_QPA_PLATFORM=offscreen (the CI) Qt drops every accessibility event here, before any update handler, so
        // removing this call would go unnoticed. The test on a real platform is #214
        QAccessible::updateAccessibility(event);
    }
    // #183: what assistive technology sees as read-only: the guard closed
    bool accessibleReadOnly() const { return m_hasSafetyGuard && !m_isGuardOpen; }
    // #183: a change of position; with two positions the checked state, with three the value
    void notifyPosition(IndustrialSwitch *sw)
    {
        if (m_positionCount == 2) {
            QAccessible::State changed;
            changed.checked = true;
            QAccessibleStateChangeEvent event(sw, changed);
            sendAccessible(&event);
        } else {
            QAccessibleValueChangeEvent event(sw, m_position);
            sendAccessible(&event);
        }
    }
    // #183: a change of the read-only state seen by assistive technology
    void notifyReadOnly(IndustrialSwitch *sw)
    {
        QAccessible::State changed;
        changed.readOnly = true;
        QAccessibleStateChangeEvent event(sw, changed);
        sendAccessible(&event);
    }
};

} // namespace QtIndustrialWidgets
