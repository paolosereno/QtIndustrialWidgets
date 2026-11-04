// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/TextMetricsTable.h"

#include <QtGui/QFontMetricsF>

namespace QtIndustrialWidgets {
namespace internal {

TextMetricsTable::TextMetricsTable(const QFont &font)
    : m_font(font)
{
}

QFont TextMetricsTable::fontAt(int pixelSize) const
{
    QFont f = m_font;
    f.setPixelSize(pixelSize);
    return f;
}

QRectF TextMetricsTable::boundingRect(const QString &text, int pixelSize)
{
    const std::pair<QString, int> key(text, pixelSize);
    auto it = m_rects.find(key);
    if (it == m_rects.end()) {
        it = m_rects.emplace(key, QFontMetricsF(fontAt(pixelSize)).boundingRect(text)).first;
        m_measures.push_back({Kind::BoundingRect, text, pixelSize});
    }
    return it->second;
}

double TextMetricsTable::horizontalAdvance(const QString &text, int pixelSize)
{
    const std::pair<QString, int> key(text, pixelSize);
    auto it = m_advances.find(key);
    if (it == m_advances.end()) {
        it = m_advances.emplace(key, QFontMetricsF(fontAt(pixelSize)).horizontalAdvance(text)).first;
        m_measures.push_back({Kind::Advance, text, pixelSize});
    }
    return it->second;
}

double TextMetricsTable::height(int pixelSize)
{
    auto it = m_heights.find(pixelSize);
    if (it == m_heights.end()) {
        it = m_heights.emplace(pixelSize, QFontMetricsF(fontAt(pixelSize)).height()).first;
        m_measures.push_back({Kind::Height, QString(), pixelSize});
    }
    return it->second;
}

double TextMetricsTable::capHeight(int pixelSize)
{
    auto it = m_capHeights.find(pixelSize);
    if (it == m_capHeights.end()) {
        it = m_capHeights.emplace(pixelSize, QFontMetricsF(fontAt(pixelSize)).capHeight()).first;
        m_measures.push_back({Kind::CapHeight, QString(), pixelSize});
    }
    return it->second;
}

} // namespace internal
} // namespace QtIndustrialWidgets
