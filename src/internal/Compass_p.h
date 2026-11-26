// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "QtIndustrialWidgets/Compass.h"

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtGui/QPolygonF>

#include <vector>

namespace QtIndustrialWidgets {

// #226: the texts of the card at one size, as the last render of the card drew them
struct CompassCardLayout {
    enum class Numbers { All, EveryOther, None };

    // Point sizes at the logical DPI of the card
    double cardinalPt{0.0};
    double intercardinalPt{0.0};
    double numberPt{0.0};
    double readoutPt{0.0};
    Numbers numbers{Numbers::All};
    bool intercardinals{true};
    // Whether the layout fits at its size
    bool fits{false};

    // Around the centre of the card, in logical px: the circle of the major ticks, the pod and the flag (without their
    // borders), and the ink box of each label drawn, rotated to its angle
    struct Label {
        int angle{0};
        QPolygonF ink;
    };
    double majorTickR{0.0};
    QRectF pod;
    QRectF flag;
    std::vector<Label> labels;
};

// #151, #226: the HDG flag of an invalid heading as the last paintEvent() laid it out, in widget coordinates
struct CompassFlag {
    QRectF rect;        // the flag, on the middle of its 1 px border
    QFont textFont;     // of "HDG"
    QPointF textOrigin; // the baseline origin of "HDG", centred on the flag
    int textPixelSize{0};
    QColor fill;
    QColor text;
};

class CompassPrivate {
public:
    double m_heading{0.0};
    double m_targetHeading{0.0};
    Compass::DisplayMode m_displayMode{Compass::DisplayMode::HeadingUp};
    bool m_bugVisible{true};
    bool m_bugInteractive{true};
    bool m_lubberVisible{true};
    bool m_digitalVisible{true};
    bool m_isDraggingBug{false};

    // Styling colors
    QColor m_dialColor{QColor(20, 24, 32)};       // Deep naval slate
    QColor m_bezelColor{QColor(48, 56, 70)};      // Machined metallic rim
    QColor m_textColor{QColor(225, 231, 236)};    // Card ticks and labels
    QColor m_readoutColor{QColor(225, 231, 236)}; // Readout on the dark pod (#159)
    QColor m_cardinalColor{QColor(0, 229, 255)};  // Cyan / amber highlights for N/E/S/W
    QColor m_needleColor{QColor(235, 59, 90)};    // Vivid red North arrow
    QColor m_needleTailColor{QColor(160, 175, 195)}; // Slate South arrow
    QColor m_bugColor{QColor(254, 130, 40)};      // High-visibility orange bug
    QColor m_lubberColor{QColor(254, 211, 48)};   // Safety amber lubber line

    // Card cache pixmap, drawn with m_cardLayout
    QPixmap m_cardCache;
    bool m_cacheDirty{true};
    CompassCardLayout m_cardLayout;
    CompassFlag m_flag;

    // #151: NaN and ±Inf have no direction (m_heading is then NaN)
    ValueState m_valueState{ValueState::Valid};

    [[nodiscard]] bool isInvalid() const { return m_valueState == ValueState::Invalid; }
    // The rotation of the card: the heading, or 0° (North up) while the heading is invalid
    [[nodiscard]] double cardRotation() const { return isInvalid() ? 0.0 : m_heading; }
};

} // namespace QtIndustrialWidgets
