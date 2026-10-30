// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "FontScale.h"

#include <QtGui/QFontInfo>
#include <QtGui/QFontMetricsF>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QScreen>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

TextInk measureTextInk(const QString &text, const QFont &font, double dpr)
{
    const QFontMetricsF fm(font);
    const double margin = 4.0;
    const QSizeF logical(fm.horizontalAdvance(text) + 2.0 * margin, fm.height() + 2.0 * margin);
    QImage img((logical * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
        return {};
    }
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    const double baseline = std::round((margin + fm.ascent()) * dpr) / dpr;
    {
        QPainter p(&img);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(QPointF(margin, baseline), text);
    }
    int first = -1;
    int last = -1;
    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        if (std::any_of(line, line + img.width(), [](QRgb p) { return qAlpha(p) != 0; })) {
            first = first < 0 ? y : first;
            last = y;
        }
    }
    if (first < 0) {
        return {};
    }
    return {first / dpr - baseline, (last + 1) / dpr - baseline};
}

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
