/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "internal/FontScale.h"
#include "internal/RadialGaugeLayout.h"

#include <QtGui/QFont>
#include <QtWidgets/QApplication>

#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

/// The fonts of a widget with scale labels and readouts on opaque panels (TankDisplay, IndustrialSlider).
struct PanelFonts {
    QFont label;
    QFont readout;
    int minimumPx{0};
};

/**
 * \brief The fonts of \a widgetFont at \a logicalDpi: bold, \a labelPx and \a readoutPx scaled as the widget font
 * against the application font (#51, rule (b)), never below 7 pt (#60). The readout has greyscale antialiasing, for
 * the opaque panel (#159).
 */
inline PanelFonts panelFonts(const QFont &widgetFont, double logicalDpi, int labelPx, int readoutPx)
{
    PanelFonts f;
    const double scale = fontScale(widgetFont, QApplication::font());
    f.minimumPx = minimumReadoutPixelSize(logicalDpi);
    f.label = widgetFont;
    f.label.setBold(true);
    f.label.setPixelSize(std::max(f.minimumPx, scalePixelSize(labelPx, scale)));
    f.readout = widgetFont;
    f.readout.setBold(true);
    f.readout.setPixelSize(std::max(f.minimumPx, scalePixelSize(readoutPx, scale)));
    f.readout.setStyleStrategy(QFont::StyleStrategy(f.readout.styleStrategy() | QFont::NoSubpixelAntialias));
    return f;
}

} // namespace internal
} // namespace QtIndustrialWidgets
