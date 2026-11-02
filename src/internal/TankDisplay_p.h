/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of TankDisplay (d-pointer), #152. Internal: never installed.

#include <QtIndustrialWidgets/TankDisplay.h>

#include "internal/TankDisplayLayout.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QFont>

#include <array>
#include <limits>

namespace QtIndustrialWidgets {

class TankDisplayPrivate
{
public:
    TankDisplay::TankShape m_shape{TankDisplay::TankShape::VerticalCylinder};
    TankDisplay::Content m_content{TankDisplay::Content::Liquid};
    double m_minimum{0.0};
    double m_maximum{100.0};
    // No value until the first setValue(): Invalid, unlike the gauges (#152)
    double m_value{std::numeric_limits<double>::quiet_NaN()};    // m_rawValue clamped to the range, NaN when Invalid
    double m_rawValue{std::numeric_limits<double>::quiet_NaN()}; // Last input, unclamped (#151)
    ValueState m_valueState{ValueState::Invalid};
    int m_precision{1};
    QString m_unit{QStringLiteral("%")};
    int m_majorTicks{5};
    int m_minorTicks{4};
    bool m_digitalDisplayVisible{true};
    bool m_scaleVisible{true};
    // lowLow, low, high, highHigh; NaN when disabled
    std::array<double, 4> m_limits{{std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}};
    double m_hysteresis{0.0};
    TankDisplay::LimitState m_limitState{TankDisplay::LimitState::Unknown};
    QColor m_contentColor{QColor(52, 152, 219)};
    QColor m_vesselColor{QColor(120, 134, 150)};
    QColor m_scaleColor{QColor(200, 208, 218)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_readoutColor{QColor(240, 244, 248)};
    QColor m_limitColor{QColor(231, 76, 60)};
    // Logical DPI of the last minimumSizeHint() (0: none since the last updateGeometry()), #177
    mutable double m_minimumDpi{0.0};
    // Geometry of the last paint, read by the tests (TankDisplayTestAccess)
    internal::TankLayout m_lastLayout;

    struct Changes {
        bool value{false};
        bool state{false};
        bool limit{false};
    };

    // Recomputes value, state and limit state from m_rawValue, the range and the limits. Emits nothing.
    Changes applyInput();

    [[nodiscard]] bool anyLimitEnabled() const;

    // A single limit setter: a value that breaks the rules is rejected with a warning that names the limit and why
    void setOneLimit(TankDisplay *q, int index, double limit);

    [[nodiscard]] QStringList majorLabelTexts() const;

    // Fonts of a widget font at a logical DPI
    struct Fonts {
        QFont label;
        QFont readout;
        int minimumPx{0};
    };

    static Fonts fontsFor(const QFont &widgetFont, double logicalDpi);

    [[nodiscard]] internal::TankLayoutInput layoutInput(double w, double h, const Fonts &fonts) const;
};

} // namespace QtIndustrialWidgets
