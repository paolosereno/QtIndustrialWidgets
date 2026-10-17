// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "FontScale.h"

#include <QtGui/QFontInfo>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

double fontScale(const QFont &widgetFont, const QFont &applicationFont)
{
    const int widgetPx = QFontInfo(widgetFont).pixelSize();
    const int applicationPx = QFontInfo(applicationFont).pixelSize();
    if (widgetPx <= 0 || applicationPx <= 0 || widgetPx == applicationPx) {
        return 1.0;
    }
    return static_cast<double>(widgetPx) / static_cast<double>(applicationPx);
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
