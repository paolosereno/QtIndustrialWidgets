/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the font-handling tests (#51).

#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtGui/QFontInfo>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <vector>

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

// The application font at twice its effective pixel size (#51, size rule): a widget with this font
// must draw its geometry-sized texts twice as large
inline QFont doubledFont()
{
    QFont f = QApplication::font();
    f.setPixelSize(2 * QFontInfo(QApplication::font()).pixelSize());
    return f;
}

// Median height (device px) of the 8-connected ink components of a set of pixels: about the height
// of the glyphs, whatever their number and placement
inline int medianGlyphHeight(const QImage &a, const QImage &b)
{
    const int w = a.width();
    const int h = a.height();
    std::vector<char> on(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            on[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = a.pixel(x, y) != b.pixel(x, y) ? 1 : 0;
        }
    }
    std::vector<int> heights;
    std::vector<int> stack;
    for (int start = 0; start < w * h; ++start) {
        if (!on[static_cast<size_t>(start)]) continue;
        int top = h, bottom = -1;
        stack.push_back(start);
        on[static_cast<size_t>(start)] = 0;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            const int x = i % w, y = i / w;
            top = std::min(top, y);
            bottom = std::max(bottom, y);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    const size_t j = static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx);
                    if (on[j]) {
                        on[j] = 0;
                        stack.push_back(static_cast<int>(j));
                    }
                }
            }
        }
        heights.push_back(bottom - top + 1);
    }
    if (heights.empty()) return 0;
    std::nth_element(heights.begin(), heights.begin() + static_cast<long>(heights.size() / 2), heights.end());
    return heights[heights.size() / 2];
}

// Median glyph height of the text a widget paints in textColor (two renders, white and red)
template <class Widget>
int textGlyphHeight(Widget &w)
{
    w.setTextColor(Qt::white);
    const QImage a = render(w);
    w.setTextColor(Qt::red);
    return medianGlyphHeight(a, render(w));
}

} // namespace FontTest
