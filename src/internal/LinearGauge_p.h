/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of LinearGauge (moved out of LinearGauge.cpp for its test hook, #274). Internal: never installed.

#include <QtIndustrialWidgets/LinearGauge.h>

#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/ValueClassification.h"

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <vector>

namespace QtIndustrialWidgets {

class LinearGaugePrivate {
public:
    Qt::Orientation m_orientation{Qt::Vertical};
    bool m_thermometerMode{true};
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};    // m_rawValue clamped to the range, NaN when Invalid
    double m_rawValue{0.0}; // Last input, unclamped (#151)
    bool m_hasInput{false}; // setValue() called: before it, m_rawValue is the initial value (#151)
    ValueState m_valueState{ValueState::Valid};
    int m_precision{1};
    QString m_unit{QStringLiteral("°C")};
    int m_majorTicks{10};
    int m_minorTicks{4};
    double m_warningThreshold{70.0};
    double m_errorThreshold{90.0};
    bool m_dynamicLiquidColor{true};
    bool m_gradientLiquid{false};
    bool m_digitalDisplayVisible{true};
    bool m_scaleVisible{true};
    QColor m_liquidColor{QColor(235, 59, 90)};
    QColor m_normalColor{QColor(46, 204, 113)};
    QColor m_warningColor{QColor(241, 196, 15)};
    QColor m_errorColor{QColor(231, 76, 60)};
    QColor m_troughColor{QColor(30, 36, 45)};
    QColor m_scaleColor{QColor(200, 208, 218)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_readoutColor{QColor(240, 244, 248)}; // "---" on the dark pod (#159)
    QColor m_bezelColor{QColor(44, 53, 64)};
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
    QRectF m_tubeRect;
    QPainterPath m_troughPath; // Inside of the tube (and bulb), hatched for an invalid input (#151)
    QPointF m_bulbCenter;
    double m_bulbRadius{0.0};
    bool m_wasWarning{false};
    bool m_wasError{false};
    // #274: the scale labels drawn by the last renderStaticScale(), with their extent along the scale in widget
    // coordinates (x from the left for a horizontal gauge, y from the top for a vertical one, start < end); read by
    // the tests through LinearGaugeTestAccess
    std::vector<internal::ScaleLabelSlot> m_lastScaleLabels;

    // Text of every major scale label, minimum first
    QStringList majorLabelTexts() const
    {
        std::vector<double> values; // The decimals are those of the whole scale (#208)
        const double valRange = m_maximum - m_minimum;
        for (int i = 0; i <= m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / m_majorTicks;
            values.push_back(m_minimum + frac * valRange);
        }
        return internal::scaleLabelTexts(values, m_precision);
    }

    struct Changes {
        bool value{false};
        bool state{false};
    };
    // Recomputes value and state from m_rawValue and the range. Emits nothing.
    Changes applyInput()
    {
        const double newValue = internal::shownValue(m_rawValue, m_minimum, m_maximum);
        const ValueState newState =
            internal::publicValueState(internal::classifyInput(m_rawValue, m_minimum, m_maximum));
        Changes changes;
        changes.value = !internal::sameShownValue(newValue, m_value);
        changes.state = (newState != m_valueState);
        m_value = newValue;
        m_valueState = newState;
        return changes;
    }
};

} // namespace QtIndustrialWidgets
