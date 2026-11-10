/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of IndustrialSlider (d-pointer), #180. Internal: never installed.

#include <QtIndustrialWidgets/IndustrialSlider.h>

#include "internal/IndustrialSliderLayout.h"

#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QFont>

#include <algorithm>
#include <limits>

namespace QtIndustrialWidgets {

class IndustrialSliderPrivate
{
public:
    Qt::Orientation m_orientation{Qt::Horizontal};
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_setpoint{0.0};
    bool m_hasFeedback{false};
    double m_feedback{std::numeric_limits<double>::quiet_NaN()};    // m_rawFeedback clamped to the range
    double m_rawFeedback{std::numeric_limits<double>::quiet_NaN()}; // Last input, unclamped (#151)
    ValueState m_feedbackState{ValueState::Invalid};
    double m_deviationThreshold{0.0};
    IndustrialSlider::DeviationState m_deviationState{IndustrialSlider::DeviationState::Off};
    int m_precision{1};
    QString m_unit{QStringLiteral("%")};
    int m_majorTicks{5};
    int m_minorTicks{4};
    double m_warningThreshold{std::numeric_limits<double>::quiet_NaN()};
    double m_errorThreshold{std::numeric_limits<double>::quiet_NaN()};
    bool m_readoutVisible{true};
    bool m_feedbackReadoutVisible{true};
    QColor m_handleColor{QColor(0, 188, 212)};
    QColor m_feedbackColor{QColor(255, 159, 67)};
    QColor m_trackColor{QColor(30, 36, 45)};
    QColor m_scaleColor{QColor(200, 208, 218)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_readoutColor{QColor(240, 244, 248)};
    QColor m_normalColor{QColor(46, 204, 113)};
    QColor m_warningColor{QColor(241, 196, 15)};
    QColor m_errorColor{QColor(231, 76, 60)};
    // Logical DPI of the last minimumSizeHint() (0: none since the last updateGeometry()), #177
    mutable double m_minimumDpi{0.0};
    // Minimum width of a readout panel used by the last minimumSizeHint(), read by the tests
    mutable double m_minimumPodWidth{0.0};

    // #180 part 2, the rules of #182: steps, read-only, wheel, setpoint limits
    double m_step{1.0};
    double m_pageStep{0.0}; // pageStep() once set; until then 5 x m_step
    bool m_pageStepSet{false};
    bool m_readOnly{false};
    bool m_wheelEnabled{false};
    int m_wheelRemainder{0}; // angleDelta() summed towards the next notch (120); dropped at focus out
    bool m_limitsSet{false};
    double m_limitLow{0.0}; // As given to setSetpointLimits(); read within the range
    double m_limitHigh{0.0};
    // An operator action (a drag or a key) runs from its start to its end, where setpointCommitted() is emitted
    bool m_dragging{false};
    double m_dragStartAlong{0.0}; // Along coordinate of the press
    internal::SliderLayout m_dragLayout; // The layout at the press, used for the whole drag
    double m_dragStartSetpoint{0.0};
    bool m_keyAction{false};
    int m_actionKey{0}; // The key whose release that is not an auto-repeat ends the key action
    double m_actionStartSetpoint{0.0};

    // What the last paint drew, read by the tests (IndustrialSliderTestAccess)
    internal::SliderLayout m_lastLayout;
    QString m_lastSetpointReadout; // The texts of the setpoint panel, in order, joined by spaces; empty: not drawn
    QString m_lastFeedbackReadout; // The same for the feedback panel; "Δ" and "?" stand for the deviation marks
    int m_lastReadoutPx{0};        // Pixel size of the setpoint readout as drawn
    int m_lastFeedbackReadoutPx{0}; // Pixel size of the feedback readout as drawn
    QRectF m_lastFocusRing;          // The focus ring drawn (its centre line); empty: not drawn

    struct Changes {
        bool setpoint{false};
        bool feedback{false};
        bool feedbackState{false};
        bool deviation{false};
    };

    // Recomputes the setpoint clamp, the feedback value and state and the deviation state. Emits nothing.
    Changes apply();
    // Emits the signals of \a changes, in the documented order
    void emitChanges(IndustrialSlider *q, const Changes &changes) const;

    [[nodiscard]] IndustrialSlider::DeviationState evaluateDeviation() const;

    // The setpoint limits as read: those set, within the range; without limits, the range
    [[nodiscard]] double limitLow() const { return m_limitsSet ? std::clamp(m_limitLow, m_minimum, m_maximum) : m_minimum; }
    [[nodiscard]] double limitHigh() const
    {
        return m_limitsSet ? std::clamp(m_limitHigh, m_minimum, m_maximum) : m_maximum;
    }
    // Where operator input that asks for \a proposed takes the setpoint (#182): inside the limits; from a setpoint
    // outside them (set with setSetpoint()), only towards them: the interval allowed is the limits widened to the
    // setpoint, so input further out leaves it where it is
    [[nodiscard]] double operatorTarget(double proposed) const
    {
        const double low = std::min(limitLow(), m_setpoint);
        const double high = std::max(limitHigh(), m_setpoint);
        return std::clamp(proposed, low, high);
    }
    // Ends the drag or the key action that is running, if any, and emits setpointCommitted() if the setpoint is not
    // the one it started from
    void endAction(IndustrialSlider *q);
    [[nodiscard]] QStringList majorLabelTexts() const;

    // Fonts of a widget font at a logical DPI
    struct Fonts {
        QFont label;
        QFont readout;
        int minimumPx{0};
    };

    static Fonts fontsFor(const QFont &widgetFont, double logicalDpi);

    [[nodiscard]] internal::SliderLayoutInput layoutInput(double w, double h, const Fonts &fonts) const;
};

} // namespace QtIndustrialWidgets
