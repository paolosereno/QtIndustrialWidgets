/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the tests of the text-metrics tables of minimumSizeHint() (#184).

#include "internal/TextMetricsTable.h"

#include <QtCore/QString>
#include <QtGui/QFont>

#include <set>
#include <tuple>
#include <vector>

namespace TextMetricsTest {

using Measure = QtIndustrialWidgets::internal::TextMetricsTable::Measure;
using Kind = QtIndustrialWidgets::internal::TextMetricsTable::Kind;

inline QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::BoundingRect: return QStringLiteral("boundingRect");
    case Kind::Advance: return QStringLiteral("horizontalAdvance");
    case Kind::Height: return QStringLiteral("height");
    case Kind::CapHeight: return QStringLiteral("capHeight");
    }
    return QString();
}

// Empty when every measure of \a log is of a different (kind, text, pixel size); otherwise the first repeated one
inline QString repeatedMeasure(const std::vector<Measure> &log)
{
    std::set<std::tuple<int, QString, int>> seen;
    for (const Measure &m : log) {
        if (!seen.emplace(static_cast<int>(m.kind), m.text, m.pixelSize).second) {
            return QStringLiteral("%1(\"%2\") at %3 px measured more than once in one computation (%4 measures)")
                .arg(kindName(m.kind), m.text)
                .arg(m.pixelSize)
                .arg(log.size());
        }
    }
    return QString();
}

// \a base with 3 px more between the letters, at the same pixel size: wider labels, the same glyphs. Unlike a
// change of family it changes the measures where no font is installed either (every glyph a box)
inline QFont spacedFont(const QFont &base)
{
    QFont f = base;
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
    return f;
}

} // namespace TextMetricsTest
