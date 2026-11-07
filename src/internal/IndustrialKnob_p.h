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
#include "NumberFormat.h"
#include "RadialGaugeLayout.h"
#include "TextMetricsTable.h"

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QtGlobal>
#include <QtGui/QAccessible>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
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

    double m_pageStep{0.0};      // pageStep() once set; until then 5 x m_step (#182)
    bool m_pageStepSet{false};

    bool m_isDragging{false};
    QPointF m_lastMousePos;
    // Angle of the value during a relative drag (#182), in degrees from the start of the scale. Not wrapped: out of
    // [0, span] it has gone past an end, and the value stays there (#175)
    double m_dragAngle{0.0};
    double m_lastPointerAngle{0.0}; // Pointer at the last press or move, in [0, 360) from the start of the scale

    // #182: an operator action (a drag or a key) runs from its start to its end, where valueCommitted() is emitted
    // if the value is not the one it started from
    bool m_keyAction{false};
    int m_actionKey{0};           // The key whose release that is not an auto-repeat ends the key action
    double m_actionStartValue{0.0};

    // #182, PR 2: read-only, mouse wheel and value limits
    bool m_readOnly{false};
    bool m_wheelEnabled{false};
    int m_wheelRemainder{0};      // angleDelta() summed towards the next notch (120); dropped at focus out
    bool m_limitsSet{false};
    double m_limitLow{0.0};       // As given to setValueLimits(); read within the range
    double m_limitHigh{0.0};
    int m_minimumWidth{0};   // minimumSizeHint().width(), computed when asked for; 0: to compute
    double m_minimumWidthDpr{0.0};
    // #170: ink of each label text at each font size, measured on a raster. Kept across resizes and changes of the
    // scale and never measured by a repaint of the same scale: dropped when the font or the DPR changes
    mutable std::map<std::pair<QString, int>, internal::TextInkBox> m_labelInk;
    mutable double m_labelInkDpr{0.0};
    mutable quint64 m_labelInkMeasures{0}; // Calls of measureCenteredTextInk(), for IndustrialKnobTestAccess (#177)

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
    // Text of the label of tick \a index (0 to tickCount()); counted in m_labelFormats
    QString labelText(int index) const;
    // What the size search of minimumSizeHint() keeps for one computation (#184): the label texts, formatted once,
    // and the table of their measures, each taken once at each font size
    struct MinimumMetrics {
        explicit MinimumMetrics(const QFont &labelFont) : table(labelFont) {}
        internal::TextMetricsTable table; // The widget font, bold, as labelsAt() draws the labels
        std::vector<QString> labelTexts;  // labelText() of each tick
    };
    MinimumMetrics minimumMetrics(const QFont &widgetFont) const;
    // Texts, boxes and selection of the labels at a geometry. With \a metrics (minimumSizeHint(), #184) the texts
    // come from it and are measured through it; without (paint), they are formatted and measured here
    ScaleLayout labelsAt(const internal::KnobScaleGeometry &geometry, const QFont &widgetFont, double dpr,
                         MinimumMetrics *metrics = nullptr) const;
    static std::vector<internal::LayoutRect> drawnInk(const ScaleLayout &layout);
    // The geometry the labels of this configuration need at a size
    internal::KnobScaleGeometry fittedGeometry(const QSize &size, const QFont &widgetFont, double dpr,
                                               MinimumMetrics *metrics = nullptr) const;
    const ScaleLayout &scaleFor(const QSize &size, const QFont &widgetFont, double dpr);
    bool widthKeepsScaleEnds(int width, bool lastLabelRequired, const QFont &widgetFont, double dpr,
                             MinimumMetrics *metrics) const;
    int computeMinimumWidth(int ceiling, const QFont &widgetFont, double dpr) const;

    // IndustrialKnobTestAccess (#184): the reference that measures every label each time, as before the table, and
    // the real measures of the table and the label texts formatted in the last computation of minimumSizeHint()
    bool m_minimumWithoutTable{false};
    mutable std::vector<internal::TextMetricsTable::Measure> m_lastMinimumMeasures;
    mutable quint64 m_labelFormats{0}; // Calls of labelText() so far
    mutable quint64 m_lastMinimumLabelFormats{0};
    // Clockwise from the start of the scale, in [0, 360), around the dial as it is drawn
    double scaleAngleAt(const QPointF &pos, const QSize &size, const QFont &widgetFont, double dpr);

    // #182: ends the drag or the key action that is running, if any, and emits valueCommitted() if the value is not
    // the one it started from
    void endAction(IndustrialKnob *knob)
    {
        if (!m_isDragging && !m_keyAction) return;
        m_isDragging = false;
        m_keyAction = false;
        m_actionKey = 0;
        if (!qFuzzyCompare(m_value, m_actionStartValue)) {
            Q_EMIT knob->valueCommitted(m_value);
        }
    }

    // #182: inner end of the pointer, as paintEvent() draws it: no angle of the pointer reaches inside this radius
    static double pointerInnerRadius(const internal::KnobScaleGeometry &g)
    {
        return g.radius * 0.65 * 0.84 * 0.90 * 0.32; // knob, face, inner face, inner end of the pointer
    }
    // #182: the padlock of read-only, centred on the cap: its corners stay at 0.82 of pointerInnerRadius()
    static QRectF padlockRect(const internal::KnobScaleGeometry &g)
    {
        const double r = pointerInnerRadius(g);
        const double w = 0.95 * r;
        const double h = 1.33 * r;
        return QRectF(g.centerX - w * 0.5, g.centerY - h * 0.5, w, h);
    }

    // #182: the value limits as read: those set, within the range; without limits, the range
    double limitLow() const { return m_limitsSet ? std::clamp(m_limitLow, m_minimum, m_maximum) : m_minimum; }
    double limitHigh() const { return m_limitsSet ? std::clamp(m_limitHigh, m_minimum, m_maximum) : m_maximum; }

    // #182: where the operator's input that asks for \a proposed takes the value. Inside the limits; from a value
    // outside them (set with setValue()), only towards them: the interval allowed is the limits widened to the value,
    // so input further out leaves it where it is. In Discrete mode, the nearest step inside that interval
    double operatorTarget(double proposed) const
    {
        const double from = m_value;
        const double low = std::min(limitLow(), from);
        const double high = std::max(limitHigh(), from);
        double target = std::clamp(proposed, low, high);
        if (m_mode == IndustrialKnob::KnobMode::Discrete && m_discreteSteps > 1) {
            const double stepSize = (m_maximum - m_minimum) / (m_discreteSteps - 1);
            constexpr double kEpsilon = 1e-9; // A limit on a step counts as inside
            const int first = static_cast<int>(std::ceil((low - m_minimum) / stepSize - kEpsilon));
            const int last = static_cast<int>(std::floor((high - m_minimum) / stepSize + kEpsilon));
            if (first > last) {
                return from;
            }
            const int nearest = static_cast<int>(std::round((target - m_minimum) / stepSize));
            target = m_minimum + std::clamp(nearest, first, last) * stepSize;
        }
        return target;
    }

    // The text of the readout, as paintEvent() draws it; also the value text of the accessible interface (#183)
    QString readoutText() const
    {
        if (m_mode == IndustrialKnob::KnobMode::Discrete) {
            int stepIdx = static_cast<int>(std::round((m_value - m_minimum) / ((m_maximum - m_minimum) / (m_discreteSteps - 1))));
            return QStringLiteral("POS %1").arg(stepIdx + 1);
        }
        return QStringLiteral("%1 %2").arg(internal::formatFixed(m_value, m_precision), m_unit).trimmed();
    }

    // #183: every accessibility event of the knob goes through here. The observer is set by tests only
    // (IndustrialKnobTestAccess): without an accessibility backend (QT_QPA_PLATFORM=offscreen) Qt drops the events
    // before any update handler, so this is the only point where a test can see them
    std::function<void(QAccessibleEvent *)> m_accessibleObserver;
    void sendAccessible(QAccessibleEvent *event)
    {
        if (m_accessibleObserver) {
            m_accessibleObserver(event);
        }
        QAccessible::updateAccessibility(event); // Does nothing while no assistive technology is active
    }
    // #183: tells assistive technology that the value (or the range around it) changed
    void notifyValue(IndustrialKnob *knob)
    {
        QAccessibleValueChangeEvent event(knob, m_value);
        sendAccessible(&event);
    }
    // #183: tells assistive technology that the read-only state changed
    void notifyReadOnly(IndustrialKnob *knob)
    {
        QAccessible::State changed;
        changed.readOnly = true;
        QAccessibleStateChangeEvent event(knob, changed);
        sendAccessible(&event);
    }

    // After a setter minimumSizeHint() depends on (#170)
    void minimumSizeChanged(QWidget *widget)
    {
        m_minimumWidth = 0;
        widget->updateGeometry();
    }
};

} // namespace QtIndustrialWidgets
