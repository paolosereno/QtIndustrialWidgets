// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #274, probe only: the fonts the tests get on the offscreen platform
#include "FontTestUtils.h"
#include "ReadoutTestUtils.h"

#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QFontMetricsF>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <cstdio>
#include <string>
#include <utility>

namespace {
void print(const char *name, const QFont &f)
{
    const QFontInfo info(f);
    const QFontMetricsF fm(f);
    std::printf("PROBE274 font|%s|requested %s|family %s|style %s|exactMatch %d|pointSizeF %.3f|pixelSize %d|bold %d|"
                "italic %d|ascent %.3f|descent %.3f|averageCharWidth %.3f",
                name, qPrintable(f.family()), qPrintable(info.family()), qPrintable(info.styleName()),
                info.exactMatch() ? 1 : 0, info.pointSizeF(), info.pixelSize(), info.bold() ? 1 : 0,
                info.italic() ? 1 : 0, fm.ascent(), fm.descent(), fm.averageCharWidth());
    for (const char *text : {"8000", "N"}) {
        const QRectF r = fm.tightBoundingRect(QString::fromLatin1(text));
        std::printf("|advance(%s) %.3f|tight(%s) %.3f %.3f %.3f %.3f", text,
                    fm.horizontalAdvance(QString::fromLatin1(text)), text, r.x(), r.y(), r.width(), r.height());
    }
    std::printf("\n");
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QScreen *screen = QGuiApplication::primaryScreen();
    std::printf("PROBE274 screen|logicalDpi %.3f %.3f|physicalDpi %.3f %.3f|dpr %.3f|platform %s\n",
                screen->logicalDotsPerInchX(), screen->logicalDotsPerInchY(), screen->physicalDotsPerInchX(),
                screen->physicalDotsPerInchY(), screen->devicePixelRatio(), qPrintable(QGuiApplication::platformName()));
    std::printf("PROBE274 families|%s\n", qPrintable(QFontDatabase::families().join(QLatin1Char(';'))));
    const QFont base = QApplication::font();
    QFont bold = base;
    bold.setBold(true);
    QFont serifBold = FontTest::otherFont(base);
    serifBold.setItalic(false);
    serifBold.setBold(true);
    QFont noItalic = FontTest::otherFont(base);
    noItalic.setItalic(false);
    QString error;
    const QFont readout = ReadoutTest::readoutTestFont(&error);
    for (const auto &[hname, pref] : {std::pair<const char *, QFont::HintingPreference>{"vertical hinting", QFont::PreferVerticalHinting},
                                      std::pair<const char *, QFont::HintingPreference>{"no hinting", QFont::PreferNoHinting}}) {
        for (const auto &[fname, font] : {std::pair<const char *, QFont>{"default", base}, std::pair<const char *, QFont>{"default bold", bold},
                                          std::pair<const char *, QFont>{"otherFont without italic", FontTest::otherFont(base)}}) {
            QFont f = font;
            if (std::string(fname) == "otherFont without italic") f.setItalic(false);
            f.setHintingPreference(pref);
            print(qPrintable(QStringLiteral("%1, %2").arg(QLatin1String(fname), QLatin1String(hname))), f);
        }
    }
    print("default", base);
    print("default bold", bold);
    print("otherFont (DejaVu Serif italic 150%)", FontTest::otherFont(base));
    print("otherFont without italic", noItalic);
    print("otherFont without italic, bold", serifBold);
    print("readout test font", readout);
    QFont px11 = bold;
    px11.setPixelSize(11);
    print("default bold 11 px", px11);
    QFont px7 = bold;
    px7.setPixelSize(7);
    print("default bold 7 px", px7);
    return 0;
}
