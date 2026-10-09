#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QSevenSegmentDisplay.h>
#include <QtGui/QPixmap>

class tst_QSevenSegmentDisplay : public QObject
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
};

void tst_QSevenSegmentDisplay::defaultValues()
{
    QSevenSegmentDisplay disp;
    QCOMPARE(disp.digitCount(), 5);
    QCOMPARE(disp.decimalPlaces(), 1);
    QCOMPARE(disp.showLeadingZeros(), false);
}

void tst_QSevenSegmentDisplay::numericalFormatting()
{
    QSevenSegmentDisplay disp;
    disp.setDigitCount(5);
    disp.setDecimalPlaces(1);
    disp.setValue(123.4);

    QCOMPARE(disp.value(), 123.4);
    QVERIFY(disp.text().contains(QStringLiteral("123.4")));
}

void tst_QSevenSegmentDisplay::textDisplay()
{
    QSevenSegmentDisplay disp;
    disp.setText(QStringLiteral("ERR-1"));
    QCOMPARE(disp.text(), QStringLiteral("ERR-1"));

    disp.display(QStringLiteral("READY"));
    QCOMPARE(disp.text(), QStringLiteral("READY"));
}

void tst_QSevenSegmentDisplay::leadingZeros()
{
    QSevenSegmentDisplay disp;
    disp.setDigitCount(4);
    disp.setDecimalPlaces(0);
    disp.setShowLeadingZeros(true);
    disp.setValue(7);

    QCOMPARE(disp.text(), QStringLiteral("0007"));
}

void tst_QSevenSegmentDisplay::signalEmission()
{
    QSevenSegmentDisplay disp;
    QSignalSpy valSpy(&disp, &QSevenSegmentDisplay::valueChanged);
    QSignalSpy textSpy(&disp, &QSevenSegmentDisplay::textChanged);

    disp.setValue(42.0);
    QCOMPARE(valSpy.count(), 1);
    QCOMPARE(textSpy.count(), 1);
}

void tst_QSevenSegmentDisplay::renderOffscreen()
{
    QSevenSegmentDisplay disp;
    disp.setDigitCount(6);
    disp.setDecimalPlaces(2);
    disp.setValue(-42.75);
    disp.resize(200, 60);

    QPixmap pix(disp.size());
    disp.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_QSevenSegmentDisplay::extremeResizeNoCrash()
{
    QSevenSegmentDisplay disp;
    disp.resize(1, 1);
    QPixmap pixSmall(disp.size());
    disp.render(&pixSmall);

    disp.resize(2000, 500);
    QPixmap pixLarge(disp.size());
    disp.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QSevenSegmentDisplay)
#include "tst_QSevenSegmentDisplay.moc"
