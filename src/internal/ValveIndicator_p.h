/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Private state of ValveIndicator (d-pointer), #153. Internal: never installed.

#include <QtIndustrialWidgets/ValveIndicator.h>

#include "internal/ValveIndicatorLayout.h"

#include <QtGui/QColor>
#include <QtGui/QFont>

#include <limits>

namespace QtIndustrialWidgets {

class ValveIndicatorPrivate
{
public:
    ValveIndicator::ValveType m_type{ValveIndicator::ValveType::Gate};
    ValveIndicator::ValveStatus m_status{ValveIndicator::ValveStatus::Unknown};
    // No opening until the first setOpening(): Invalid
    double m_opening{std::numeric_limits<double>::quiet_NaN()};    // m_rawOpening clamped to [0, 100], NaN when Invalid
    double m_rawOpening{std::numeric_limits<double>::quiet_NaN()}; // Last input, unclamped (#151)
    ValueState m_openingState{ValueState::Invalid};
    bool m_openingVisible{false};
    int m_precision{0};
    Qt::Orientation m_orientation{Qt::Horizontal};
    bool m_pipeStubsVisible{false};
    QColor m_openColor{QColor(46, 204, 113)};
    QColor m_closedColor{QColor(231, 76, 60)};
    QColor m_transitColor{QColor(241, 196, 15)};
    QColor m_faultColor{QColor(217, 70, 239)};
    QColor m_unknownColor{QColor(149, 165, 166)};
    QColor m_outlineColor{QColor(200, 208, 218)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_readoutColor{QColor(240, 244, 248)};
    // Logical DPI of the last minimumSizeHint() (0: none since the last updateGeometry()), #177
    mutable double m_minimumDpi{0.0};
    // Geometry of the last paint, read by the tests (ValveIndicatorTestAccess)
    internal::ValveLayout m_lastLayout;

    [[nodiscard]] bool openingShown() const
    {
        return m_type == ValveIndicator::ValveType::Control || m_openingVisible;
    }

    // Fonts of a widget font at a logical DPI
    struct Fonts {
        QFont tag;
        QFont readout;
        int minimumPx{0};
    };

    static Fonts fontsFor(const QFont &widgetFont, double logicalDpi);

    [[nodiscard]] internal::ValveLayoutInput layoutInput(double w, double h, const Fonts &fonts) const;
};

} // namespace QtIndustrialWidgets
