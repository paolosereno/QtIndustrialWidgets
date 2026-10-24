// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/SevenSegmentDisplay.h>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_SevenSegmentDisplay : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void numericalFormatting();
    void textDisplay();
    void leadingZeros();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void noNegativeZero_data();
    void noNegativeZero();
};

void tst_SevenSegmentDisplay::defaultValues()
{
    SevenSegmentDisplay disp;
    QCOMPARE(disp.digitCount(), 5);
    QCOMPARE(disp.decimalPlaces(), 1);
    QCOMPARE(disp.showLeadingZeros(), false);
}

void tst_SevenSegmentDisplay::numericalFormatting()
{
    SevenSegmentDisplay disp;
    disp.setDigitCount(5);
    disp.setDecimalPlaces(1);
    disp.setValue(123.4);

    QCOMPARE(disp.value(), 123.4);
    QVERIFY(disp.text().contains(QStringLiteral("123.4")));
}

void tst_SevenSegmentDisplay::textDisplay()
{
    SevenSegmentDisplay disp;
    disp.setText(QStringLiteral("ERR-1"));
    QCOMPARE(disp.text(), QStringLiteral("ERR-1"));

    disp.display(QStringLiteral("READY"));
    QCOMPARE(disp.text(), QStringLiteral("READY"));
}

void tst_SevenSegmentDisplay::leadingZeros()
{
    SevenSegmentDisplay disp;
    disp.setDigitCount(4);
    disp.setDecimalPlaces(0);
    disp.setShowLeadingZeros(true);
    disp.setValue(7);

    QCOMPARE(disp.text(), QStringLiteral("0007"));
}

void tst_SevenSegmentDisplay::signalEmission()
{
    SevenSegmentDisplay disp;
    QSignalSpy valSpy(&disp, &SevenSegmentDisplay::valueChanged);
    QSignalSpy textSpy(&disp, &SevenSegmentDisplay::textChanged);

    disp.setValue(42.0);
    QCOMPARE(valSpy.count(), 1);
    QCOMPARE(textSpy.count(), 1);
}

void tst_SevenSegmentDisplay::renderOffscreen()
{
    SevenSegmentDisplay disp;
    disp.setDigitCount(6);
    disp.setDecimalPlaces(2);
    disp.setValue(-42.75);
    disp.resize(200, 60);

    QPixmap pix(disp.size());
    disp.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_SevenSegmentDisplay::extremeResizeNoCrash()
{
    SevenSegmentDisplay disp;
    disp.resize(1, 1);
    QPixmap pixSmall(disp.size());
    disp.render(&pixSmall);

    disp.resize(2000, 500);
    QPixmap pixLarge(disp.size());
    disp.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_SevenSegmentDisplay::nanAndInfinityResilience()
{
    SevenSegmentDisplay disp;

    // NaN must format to " Err "
    disp.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(disp.text(), QStringLiteral(" Err "));

    // +Inf must format to " oFL "
    disp.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(disp.text(), QStringLiteral(" oFL "));

    // -Inf must format to "-oFL "
    disp.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(disp.text(), QStringLiteral("-oFL "));

    // Offscreen render of diagnostic messages
    disp.resize(200, 60);
    QPixmap pix(disp.size());
    disp.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_SevenSegmentDisplay::noNegativeZero_data()
{
    QTest::addColumn<int>("decimals");
    QTest::addColumn<double>("magnitude");
    QTest::addRow("1 decimal, 0.04") << 1 << 0.04;
    QTest::addRow("0 decimals, 0.3") << 0 << 0.3;
    QTest::addRow("2 decimals, 0.004") << 2 << 0.004;
}

// #46: a negative value that rounds to zero is displayed like the positive one, without the sign
void tst_SevenSegmentDisplay::noNegativeZero()
{
    QFETCH(int, decimals);
    QFETCH(double, magnitude);
    SevenSegmentDisplay display;
    display.setDecimalPlaces(decimals);
    display.setValue(magnitude);
    const QString positive = display.text();
    display.setValue(-magnitude);
    const QString negative = display.text();
    QVERIFY2(!negative.contains(QLatin1Char('-')), qPrintable(QStringLiteral("text '%1'").arg(negative)));
    QCOMPARE(negative, positive);
}

QTEST_MAIN(tst_SevenSegmentDisplay)
#include "tst_SevenSegmentDisplay.moc"


