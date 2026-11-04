// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #184: the text-metrics table of minimumSizeHint() gives the values of a direct measure with the complete font at
// each pixel size, and measures each (text, pixel size) once

#include <QtTest/QtTest>

#include "internal/TextMetricsTable.h"

#include <QtGui/QFontMetricsF>
#include <QtWidgets/QApplication>

#include <functional>

using QtIndustrialWidgets::internal::TextMetricsTable;

class tst_TextMetricsTable : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sameAsADirectMeasure_data();
    void sameAsADirectMeasure();
    void eachMeasureTakenOnce();
};

void tst_TextMetricsTable::sameAsADirectMeasure_data()
{
    using Change = std::function<void(QFont &)>;
    QTest::addColumn<Change>("change");
    QTest::addRow("application font, bold") << Change([](QFont &f) { f.setBold(true); });
    QTest::addRow("letter spacing") << Change([](QFont &f) { f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0); });
    QTest::addRow("letter spacing in percent") << Change([](QFont &f) { f.setLetterSpacing(QFont::PercentageSpacing, 150.0); });
    QTest::addRow("word spacing") << Change([](QFont &f) { f.setWordSpacing(6.0); });
    QTest::addRow("stretch") << Change([](QFont &f) { f.setStretch(QFont::ExtraExpanded); });
    QTest::addRow("capitals") << Change([](QFont &f) { f.setCapitalization(QFont::AllUppercase); });
    QTest::addRow("small capitals") << Change([](QFont &f) { f.setCapitalization(QFont::SmallCaps); });
    QTest::addRow("no kerning") << Change([](QFont &f) { f.setKerning(false); });
    QTest::addRow("no antialiasing") << Change([](QFont &f) { f.setStyleStrategy(QFont::NoAntialias); });
    QTest::addRow("italic serif") << Change([](QFont &f) {
        f.setFamily(QStringLiteral("DejaVu Serif"));
        f.setItalic(true);
    });
}

// Every value equals QFontMetricsF of the same font with only the pixel size set, for several texts and sizes,
// asked in an order that mixes the sizes and repeats the texts
void tst_TextMetricsTable::sameAsADirectMeasure()
{
    using Change = std::function<void(QFont &)>;
    QFETCH(Change, change);
    QFont font = QApplication::font();
    change(font);
    TextMetricsTable table(font);
    const QStringList texts = {QStringLiteral("0"), QStringLiteral("100.0"), QStringLiteral("-1000000"),
                               QStringLiteral("bar"), QStringLiteral("m3 per h"), QStringLiteral("---")};
    for (int round = 0; round < 2; ++round) {
        for (int px : {11, 7, 30, 8, 7, 11}) {
            QFont f = font;
            f.setPixelSize(px);
            const QFontMetricsF fm(f);
            for (const QString &text : texts) {
                const QString where = QStringLiteral("\"%1\" at %2 px").arg(text).arg(px);
                QVERIFY2(table.boundingRect(text, px) == fm.boundingRect(text), qPrintable(where));
                QVERIFY2(table.horizontalAdvance(text, px) == fm.horizontalAdvance(text), qPrintable(where));
            }
            QCOMPARE(table.height(px), fm.height());
            QCOMPARE(table.capHeight(px), fm.capHeight());
        }
    }
}

// The log holds one entry per (kind, text, pixel size): a value asked again is read from the table
void tst_TextMetricsTable::eachMeasureTakenOnce()
{
    using Kind = TextMetricsTable::Kind;
    TextMetricsTable table(QApplication::font());
    QVERIFY(table.measures().empty());
    (void)table.boundingRect(QStringLiteral("10"), 9);
    (void)table.boundingRect(QStringLiteral("10"), 9);
    QCOMPARE(table.measures().size(), size_t(1));
    (void)table.boundingRect(QStringLiteral("10"), 10); // another size
    (void)table.boundingRect(QStringLiteral("20"), 9);  // another text
    (void)table.horizontalAdvance(QStringLiteral("10"), 9); // another kind
    (void)table.horizontalAdvance(QStringLiteral("10"), 9);
    (void)table.height(9);
    (void)table.height(9);
    (void)table.capHeight(9);
    (void)table.capHeight(10);
    (void)table.capHeight(9);
    const std::vector<TextMetricsTable::Measure> &log = table.measures();
    QCOMPARE(log.size(), size_t(7));
    const struct {
        Kind kind;
        const char *text;
        int px;
    } expected[] = {{Kind::BoundingRect, "10", 9}, {Kind::BoundingRect, "10", 10}, {Kind::BoundingRect, "20", 9},
                    {Kind::Advance, "10", 9},      {Kind::Height, "", 9},          {Kind::CapHeight, "", 9},
                    {Kind::CapHeight, "", 10}};
    for (size_t i = 0; i < log.size(); ++i) {
        QCOMPARE(log[i].kind, expected[i].kind);
        QCOMPARE(log[i].text, QString::fromLatin1(expected[i].text));
        QCOMPARE(log[i].pixelSize, expected[i].px);
    }
}

QTEST_MAIN(tst_TextMetricsTable)
#include "tst_TextMetricsTable.moc"
