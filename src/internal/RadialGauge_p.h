/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of RadialGauge. Internal: never installed.

#include <QtIndustrialWidgets/RadialGauge.h>

#include "internal/RadialGaugeLayout.h"
#include "internal/TextMetricsTable.h"

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <vector>

namespace QtIndustrialWidgets {

class RadialGaugePrivate {
public:
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};    // m_rawValue clamped to the range, NaN when Invalid
    double m_rawValue{0.0}; // Last input, unclamped (#151)
    bool m_hasInput{false}; // setValue() called: before it, m_rawValue is the initial value (#151)
    ValueState m_valueState{ValueState::Valid};
    int m_precision{1};
    QString m_unit{QStringLiteral("bar")};

    // Angles: 0° is 12 o'clock, clockwise. Default: -135° to +135° (270° span)
    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{4};

    double m_warningThreshold{70.0};
    double m_errorThreshold{85.0};
    bool m_thresholdBandsVisible{true};
    bool m_digitalDisplayVisible{true};

    // Colors
    QColor m_needleColor{QColor(235, 59, 90)};       // Industrial Crimson
    QColor m_normalColor{QColor(38, 222, 129)};       // Neon Emerald Green
    QColor m_warningColor{QColor(254, 211, 48)};      // Amber Gold
    QColor m_errorColor{QColor(235, 59, 90)};         // Danger Red
    QColor m_dialColor{QColor(24, 28, 36)};           // Slate Black
    QColor m_scaleColor{QColor(210, 218, 226)};       // Light Silver
    QColor m_textColor{QColor(245, 246, 250)};        // Crisp White
    QColor m_readoutColor{QColor(245, 246, 250)};     // Readout on the dark pod (#159)
    QColor m_bezelColor{QColor(53, 59, 72)};          // Gunmetal Gray

    // High performance static scale cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
    double m_cacheDpi{0.0}; // Logical DPI the static scale was drawn at: its texts are sized in points (#177)

    // Digital display geometry, computed with the static scale (see internal/RadialGaugeLayout.h)
    QRectF m_podRect;
    int m_valueFontPx{8};
    bool m_valueShown{true};

    // minimumSizeHint(), computed once per configuration (0: to be computed)
    int m_minimumSide{0};
    double m_minimumSideDpi{0.0};

    // Threshold state tracking
    bool m_wasWarning{false};
    bool m_wasError{false};

    struct Changes {
        bool value{false};
        bool state{false};
    };
    // Recomputes value and state from m_rawValue and the range. Emits nothing.
    Changes applyInput();

    // Scale labels and digital display at a size: the same for renderStaticScale() and minimumSizeHint()
    struct ScaleLayout {
        QFont labelFont;
        QFont unitFont;
        std::vector<QString> labelTexts;     // One for each major tick
        internal::RadialScaleLabels labels;  // The labels that keep the minimum gap
        std::vector<internal::LayoutRect> labelRects; // Boxes of labels.drawn
        internal::PodLayout pod;             // Empty without the digital display
    };
    // #184: what one computation of minimumSizeHint() measures, each text once at each font size: the side search
    // lays out the same texts at every side tried, and they only take a few font sizes
    struct MinimumMetrics {
        internal::TextMetricsTable bold;  // Labels and value: the widget font, bold
        internal::TextMetricsTable plain; // Unit: the widget font, not bold
        std::vector<QString> labelTexts;  // scaleLabelText() of each major tick
        QString minText;                  // formatFixed() of the range ends, for the width of the value
        QString maxText;
    };
    MinimumMetrics minimumMetrics(const QFont &widgetFont) const;
    // With \a metrics (minimumSizeHint()), the texts are measured through it; without (paint), directly
    ScaleLayout scaleLayout(double width, double height, const QFont &widgetFont, double logicalDpi,
                            MinimumMetrics *metrics = nullptr) const;
    double angleOf(double value) const;
    // Both ends of the scale and the state indicator at 7 pt are in place at this side
    bool sideKeepsScaleEnds(int side, bool lastLabelRequired, const QFont &widgetFont, double logicalDpi,
                            MinimumMetrics *metrics) const;
    int computeMinimumSide(int ceiling, const QFont &widgetFont, double logicalDpi) const;

    // RadialGaugeTestAccess (#184): the reference that measures every text each time, as before the tables, and the
    // real measures of each table in the last computation of minimumSizeHint() (bold, plain)
    bool m_minimumWithoutTable{false};
    mutable std::vector<std::vector<internal::TextMetricsTable::Measure>> m_lastMinimumMeasures;
    // To call when what minimumSizeHint() depends on changes
    void minimumSizeChanged(QWidget *widget)
    {
        m_minimumSide = 0;
        widget->updateGeometry();
    }
};


} // namespace QtIndustrialWidgets
