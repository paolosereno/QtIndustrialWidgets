/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of IndustrialKnob, in a header of its own so that IndustrialKnobTestAccess can read it (#177).
// Internal: never installed.

#include <QtIndustrialWidgets/IndustrialKnob.h>

#include "FontScale.h"
#include "RadialGaugeLayout.h"

#include <QtCore/QPointF>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QtGlobal>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace QtIndustrialWidgets {

class IndustrialKnobPrivate {
public:
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{25.0};
    double m_step{1.0};
    int m_precision{0};
    QString m_unit{QStringLiteral("%")};

    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{3};

    IndustrialKnob::KnobMode m_mode{IndustrialKnob::KnobMode::Continuous};
    int m_discreteSteps{5};
    bool m_trackVisible{true};
    bool m_valueDisplayVisible{true};

    QColor m_knobColor{QColor(42, 48, 60)};
    QColor m_pointerColor{QColor(0, 229, 255)};
    QColor m_scaleColor{QColor(190, 200, 215)};
    QColor m_trackColor{QColor(0, 229, 255)};
    QColor m_textColor{QColor(240, 244, 250)};

    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    bool m_isDragging{false};
    QPointF m_lastMousePos;
    // Pointer during a drag, in degrees from the start of the scale. Not wrapped: out of [0, span] it has gone
    // past an end, and the value stays there
    double m_dragAngle{0.0};
    int m_minimumWidth{0};   // minimumSizeHint().width(), computed when asked for; 0: to compute
    double m_minimumWidthDpr{0.0};
    // #170: ink of each label text at each font size, measured on a raster. Kept across resizes and changes of the
    // scale and never measured by a repaint of the same scale: dropped when the font or the DPR changes
    mutable std::map<std::pair<QString, int>, internal::TextInkBox> m_labelInk;
    mutable double m_labelInkDpr{0.0};

    // Scale at the current configuration: the same for the static scale, the paint and the mouse (#170)
    struct ScaleLayout {
        internal::KnobScaleGeometry geometry;
        QFont font;
        std::vector<QString> texts;
        std::vector<internal::LayoutRect> boxes;
        std::vector<internal::LayoutRect> ink; // Where each label really draws, for the edge of the widget
        internal::RadialScaleLabels labels;
    };
    ScaleLayout m_scale;
    QSize m_scaleSize;   // Size m_scale was computed for
    double m_scaleDpr{0.0};
    bool m_scaleValid{false};

    static constexpr int kMinimumWidth = 80; // minimumSizeHint() never goes below 80 x 90
    static constexpr size_t kMaximumLabelInks = 1024; // A range that keeps changing must not grow m_labelInk for ever

    // Height at a width, in the proportions of the floor and of sizeHint()
    static constexpr int minimumHeightOf(int width) { return (width * 9 + 7) / 8; }

    double angleOf(double value) const;
    double valueAtScaleAngle(double angle) const;
    int tickCount() const;
    // Texts, boxes and selection of the labels at a geometry
    ScaleLayout labelsAt(const internal::KnobScaleGeometry &geometry, const QFont &widgetFont, double dpr) const;
    static std::vector<internal::LayoutRect> drawnInk(const ScaleLayout &layout);
    // The geometry the labels of this configuration need at a size
    internal::KnobScaleGeometry fittedGeometry(const QSize &size, const QFont &widgetFont, double dpr) const;
    const ScaleLayout &scaleFor(const QSize &size, const QFont &widgetFont, double dpr);
    bool widthKeepsScaleEnds(int width, bool lastLabelRequired, const QFont &widgetFont, double dpr) const;
    int computeMinimumWidth(int ceiling, const QFont &widgetFont, double dpr) const;
    // Clockwise from the start of the scale, in [0, 360), around the dial as it is drawn
    double scaleAngleAt(const QPointF &pos, const QSize &size, const QFont &widgetFont, double dpr);

    // After a setter minimumSizeHint() depends on (#170)
    void minimumSizeChanged(QWidget *widget)
    {
        m_minimumWidth = 0;
        widget->updateGeometry();
    }
};

} // namespace QtIndustrialWidgets
