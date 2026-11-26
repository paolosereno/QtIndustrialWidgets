// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "QtIndustrialWidgets/Compass.h"

#include <QtCore/QHash>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtGui/QPolygonF>
#include <QtWidgets/QWidget>

#include <map>
#include <vector>

namespace QtIndustrialWidgets {

// #226: the texts of the card at one size, chosen so that they fit. In this order, until everything fits: the fonts go
// down to 7 pt; the numbers 030 … 330 are thinned (every other one, then none); the intercardinals NE … NW go; the
// cardinals N, E, S, W always stay. The readout font goes down to 7 pt in the same way. Below minimumSizeHint() the
// last layout (cardinals and readout at 7 pt) is drawn even if it does not fit
struct CompassCardLayout {
    enum class Numbers { All, EveryOther, None };

    // Point sizes at the logical DPI of the widget
    double cardinalPt{0.0};
    double intercardinalPt{0.0};
    double numberPt{0.0};
    double readoutPt{0.0};
    Numbers numbers{Numbers::All};
    bool intercardinals{true};
    // Whether the layout fits at its size; false only below minimumSizeHint()
    bool fits{false};

    // The geometry it was checked on, around the centre of the card, in logical px: the circle of the major ticks, the
    // pod and the flag (without their borders), and the ink box of each label drawn, rotated to its angle
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

// #226: what the layout measures, kept for one widget font at one logical DPI: the layout of each side drawn, and the
// ink of the widest readout (360 headings and "---°") at each point size measured
struct CompassLayoutCache {
    QFont font;
    double dpi{0.0};
    std::map<int, CompassCardLayout> layouts;
    std::map<double, QRectF> readoutInks;

    // Drops what was measured with another font or DPI
    void use(const QFont &widgetFont, double logicalDpi)
    {
        if (font != widgetFont || dpi != logicalDpi || layouts.size() > 512) {
            font = widgetFont;
            dpi = logicalDpi;
            layouts.clear();
            readoutInks.clear();
        }
    }
};

// #226: where each readout text is drawn so that its ink is centred, measured once for a font, DPR and logical DPI
struct CompassReadoutOrigin {
    QFont font;
    qreal dpr{0.0};
    int dpi{0};
    QHash<QString, double> inkCentres; // device px from the baseline origin
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

    // Card cache pixmap, drawn at m_cacheDpi with m_cardLayout (#226)
    QPixmap m_cardCache;
    bool m_cacheDirty{true};
    double m_cacheDpi{0.0};
    CompassCardLayout m_cardLayout;
    CompassFlag m_flag;
    CompassReadoutOrigin m_readoutOrigin;
    mutable CompassLayoutCache m_layoutCache;

    // #226: the minimum side, computed from the font at m_minimumSideDpi; 0 when to compute again
    mutable int m_minimumSide{0};
    mutable double m_minimumSideDpi{0.0};

    // #151: NaN and ±Inf have no direction (m_heading is then NaN)
    ValueState m_valueState{ValueState::Valid};

    [[nodiscard]] bool isInvalid() const { return m_valueState == ValueState::Invalid; }
    // The rotation of the card: the heading, or 0° (North up) while the heading is invalid
    [[nodiscard]] double cardRotation() const { return isInvalid() ? 0.0 : m_heading; }

    // #226: the layout of the card for a widget of \a side px, with \a widgetFont measured on \a device (the widget:
    // its logical DPI). \a cache holds the measures of that font and DPI
    [[nodiscard]] static CompassCardLayout cardLayout(int side, const QFont &widgetFont, const QPaintDevice *device,
                                                      CompassLayoutCache &cache);
    // #226: the smallest side from which the last layout fits, at that side and at every larger one
    [[nodiscard]] static int computeMinimumSide(const QFont &widgetFont, const QPaintDevice *device,
                                                CompassLayoutCache &cache);

    // To call when what minimumSizeHint() depends on changes
    void minimumSizeChanged(QWidget *widget)
    {
        m_minimumSide = 0;
        widget->updateGeometry();
    }
};

} // namespace QtIndustrialWidgets
