// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "FontScale.h"

#include <QtGui/QFontInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

double effectivePixelSize(const QFont &font)
{
    const int resolved = QFontInfo(font).pixelSize();
    if (resolved > 0) {
        return resolved;
    }
    if (font.pixelSize() > 0) {
        return font.pixelSize();
    }
    if (font.pointSizeF() > 0.0) {
        const QScreen *screen = QGuiApplication::primaryScreen();
        const double dpi = screen ? screen->logicalDotsPerInchY() : 96.0;
        return font.pointSizeF() * dpi / 72.0;
    }
    return -1.0;
}

double fontScale(const QFont &widgetFont, const QFont &applicationFont)
{
    const double widgetPx = effectivePixelSize(widgetFont);
    const double applicationPx = effectivePixelSize(applicationFont);
    if (!(widgetPx > 0.0) || !(applicationPx > 0.0) || widgetPx == applicationPx) {
        return 1.0;
    }
    return widgetPx / applicationPx;
}

int scalePixelSize(int px, double scale)
{
    if (scale == 1.0 || !std::isfinite(scale) || scale <= 0.0) {
        return px;
    }
    return std::max(1, static_cast<int>(std::lround(px * scale)));
}

double scalePointSize(double pt, double scale)
{
    if (scale == 1.0 || !std::isfinite(scale) || scale <= 0.0) {
        return pt;
    }
    return pt * scale;
}

} // namespace internal
} // namespace QtIndustrialWidgets
