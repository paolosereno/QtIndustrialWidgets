/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the tests of the digital readouts and their value states (#151).

#include <QtGui/QFont>
#include <QtGui/QFontDatabase>
#include <QtGui/QImage>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtWidgets/QApplication>

#include <algorithm>

namespace ReadoutTest {

// Column groups of the readout ink inside \a box: runs of adjacent columns that hold near-white ink, left to
// right, each with the top and bottom ink row of every column
struct InkGroup {
    QVector<int> top;
    QVector<int> bottom;
};

inline QVector<InkGroup> readoutInkGroups(const QImage &image, const QRect &box)
{
    QVector<InkGroup> groups;
    bool inGroup = false;
    for (int x = box.left(); x <= box.right(); ++x) {
        int top = -1;
        int bottom = -1;
        for (int y = box.top(); y <= box.bottom(); ++y) {
            const QRgb p = image.pixel(x, y);
            if (qRed(p) >= 200 && qGreen(p) >= 200 && qBlue(p) >= 200) {
                if (top < 0) {
                    top = y;
                }
                bottom = y;
            }
        }
        if (top < 0) {
            inGroup = false;
            continue;
        }
        if (!inGroup) {
            groups.push_back(InkGroup{});
            inGroup = true;
        }
        groups.back().top.push_back(top);
        groups.back().bottom.push_back(bottom);
    }
    return groups;
}

inline int spread(const QVector<int> &v)
{
    return *std::max_element(v.begin(), v.end()) - *std::min_element(v.begin(), v.end());
}

// Empty when \a dashes are the ink groups of "---": three groups, each flat with its top and bottom edges level
// with those of the others; otherwise what differs. Checked by groups, not by glyph shapes: without installed
// fonts (Windows offscreen CI) every character is drawn as the same box. The dashes sit at fractional
// positions, so their first and last columns are antialiased ends, only partly covered: the edges are compared
// on the interior columns of each dash only (at least 3), and the widths are not compared
inline QString dashesError(const QVector<InkGroup> &dashes)
{
    if (dashes.size() != 3) {
        return QStringLiteral("%1 ink groups for the invalid readout").arg(dashes.size());
    }
    const auto interior = [](const QVector<int> &v) { return v.mid(1, v.size() - 2); };
    if (dashes.first().top.size() < 5) {
        return QStringLiteral("first dash %1 px wide").arg(dashes.first().top.size());
    }
    const int top = interior(dashes.first().top).first();
    const int bottom = interior(dashes.first().bottom).first();
    for (const InkGroup &g : dashes) {
        const QVector<int> t = interior(g.top);
        const QVector<int> b = interior(g.bottom);
        const QString what =
            QStringLiteral("dash %1 px wide, interior top %2..%3, bottom %4..%5 (first dash: top %6, bottom %7)")
                .arg(g.top.size())
                .arg(t.isEmpty() ? -1 : *std::min_element(t.begin(), t.end()))
                .arg(t.isEmpty() ? -1 : *std::max_element(t.begin(), t.end()))
                .arg(b.isEmpty() ? -1 : *std::min_element(b.begin(), b.end()))
                .arg(b.isEmpty() ? -1 : *std::max_element(b.begin(), b.end()))
                .arg(top)
                .arg(bottom);
        if (t.size() < 3 || spread(t) != 0 || spread(b) != 0 || t.first() != top || b.first() != bottom) {
            return what;
        }
    }
    return QString();
}

// The test font of tests/fonts: the readout characters (0-9 . - < > and the space) and no glyph for U+25B2 or
// U+25BC, loaded from the repository so that the test does not depend on the installed fonts. With
// NoFontMerging a mark drawn as one of those characters would come out as a missing-glyph box, or not at all.
// The application font with the family of the test font, so that the size rules of the widgets are unchanged
inline QFont readoutTestFont(QString *error)
{
    const QString path = QStringLiteral(QIW_TEST_FONT_DIR "/QiwReadoutTest-Bold.ttf");
    const int id = QFontDatabase::addApplicationFont(path);
    const QStringList families = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
    if (families.isEmpty()) {
        *error = QStringLiteral("cannot load %1").arg(path);
        return QFont();
    }
    QFont f = QApplication::font();
    f.setFamily(families.first());
    f.setStyleStrategy(QFont::NoFontMerging);
    return f;
}

} // namespace ReadoutTest
