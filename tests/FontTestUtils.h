/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the font-handling tests (#51).

#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtWidgets/QWidget>

namespace FontTest {

// A font that differs from \a base in family, style and letter spacing, at the same size. The
// letter spacing moves the glyphs even where the platform has no fonts and draws boxes (Windows
// offscreen CI), so the text pixels change on every platform.
inline QFont otherFont(const QFont &base)
{
    QFont f = base;
    f.setFamily(QStringLiteral("DejaVu Serif"));
    f.setItalic(true);
    f.setLetterSpacing(QFont::PercentageSpacing, 150.0);
    return f;
}

inline QImage render(QWidget &w)
{
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

inline int differingPixelCount(const QImage &a, const QImage &b)
{
    if (a.size() != b.size()) {
        return a.width() * a.height();
    }
    int n = 0;
    for (int y = 0; y < a.height(); ++y) {
        const auto *la = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const auto *lb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            n += la[x] != lb[x] ? 1 : 0;
        }
    }
    return n;
}

} // namespace FontTest
