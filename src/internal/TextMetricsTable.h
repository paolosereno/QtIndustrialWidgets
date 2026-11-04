/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtGui/QFont>

#include <map>
#include <utility>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Text metrics of one font at several pixel sizes, each measured once (#184).
 *
 * The size searches of minimumSizeHint() lay out the same texts at many sizes, and the texts only take a few font
 * sizes: the table measures each (text, pixel size) the first time it is asked for and gives the same values
 * afterwards. Built from the complete font of the widget (family, weight, style, letter and word spacing, stretch,
 * capitalization, kerning, style strategy, ...); only the pixel size changes from one lookup to another. It measures
 * as QFontMetricsF(font) does, with no paint device, so the values are those of a direct measure.
 *
 * A table lives for one computation and is dropped at its end: it does not follow a change of the font.
 */
class TextMetricsTable
{
public:
    /// What a real measure was for (the log of measures(), for the tests)
    enum class Kind { BoundingRect, Advance, Height, CapHeight };
    struct Measure {
        Kind kind;
        QString text; ///< Empty for Height and CapHeight
        int pixelSize;
    };

    explicit TextMetricsTable(const QFont &font);

    /// QFontMetricsF::boundingRect(text) of the font at \a pixelSize
    QRectF boundingRect(const QString &text, int pixelSize);
    /// QFontMetricsF::horizontalAdvance(text) of the font at \a pixelSize
    double horizontalAdvance(const QString &text, int pixelSize);
    /// QFontMetricsF::height() of the font at \a pixelSize
    double height(int pixelSize);
    /// QFontMetricsF::capHeight() of the font at \a pixelSize
    double capHeight(int pixelSize);

    /// The real measures taken so far, in order (a value read again from the table is not in the log)
    const std::vector<Measure> &measures() const { return m_measures; }

private:
    QFont fontAt(int pixelSize) const;

    QFont m_font;
    std::map<std::pair<QString, int>, QRectF> m_rects;
    std::map<std::pair<QString, int>, double> m_advances;
    std::map<int, double> m_heights;
    std::map<int, double> m_capHeights;
    std::vector<Measure> m_measures;
};

} // namespace internal
} // namespace QtIndustrialWidgets
