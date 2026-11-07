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
#include <QtGui/QColor>
#include <QtGui/QPixmap>

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
};

} // namespace QtIndustrialWidgets
