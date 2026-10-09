#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QLedIndicator.h>
#include <QtGui/QPixmap>

class tst_QLedIndicator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void stateAndToggling();
    void signalEmission();
    void shapes();
    void blinking();
    void clickableMouseInteraction();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_QLedIndicator::defaultValues()
{
    QLedIndicator led;
    QCOMPARE(led.isOn(), true);
    QCOMPARE(led.isBlinking(), false);
    QCOMPARE(led.shape(), QLedIndicator::LedShape::Circular);
    QCOMPARE(led.isClickable(), false);
}

void tst_QLedIndicator::stateAndToggling()
{
    QLedIndicator led;
    led.setOn(false);
    QCOMPARE(led.isOn(), false);

    led.toggle();
    QCOMPARE(led.isOn(), true);

    led.setOff();
    QCOMPARE(led.isOn(), false);
}

void tst_QLedIndicator::signalEmission()
{
    QLedIndicator led;
    led.setOn(false);

    QSignalSpy spy(&led, &QLedIndicator::stateChanged);
    led.setOn(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void tst_QLedIndicator::shapes()
{
    QLedIndicator led;
    led.setShape(QLedIndicator::LedShape::Rectangular);
    QCOMPARE(led.shape(), QLedIndicator::LedShape::Rectangular);

    led.setShape(QLedIndicator::LedShape::Circular);
    QCOMPARE(led.shape(), QLedIndicator::LedShape::Circular);
}

void tst_QLedIndicator::blinking()
{
    QLedIndicator led;
    led.setBlinking(true);
    QVERIFY(led.isBlinking());

    led.setBlinkRateMs(150);
    QCOMPARE(led.blinkRateMs(), 150);

    led.setBlinking(false);
    QVERIFY(!led.isBlinking());
}

void tst_QLedIndicator::clickableMouseInteraction()
{
    QLedIndicator led;
    led.setClickable(true);
    led.setOn(false);

    QSignalSpy clickSpy(&led, &QLedIndicator::clicked);
    QSignalSpy stateSpy(&led, &QLedIndicator::stateChanged);

    QTest::mouseClick(&led, Qt::LeftButton);

    QCOMPARE(clickSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(led.isOn(), true);
}

void tst_QLedIndicator::renderOffscreen()
{
    QLedIndicator led;
    led.setOn(true);
    led.setShape(QLedIndicator::LedShape::Circular);
    led.setLabelText(QStringLiteral("RUN"));
    led.resize(60, 60);

    QPixmap pix(led.size());
    led.render(&pix);
    QVERIFY(!pix.isNull());

    led.setShape(QLedIndicator::LedShape::Rectangular);
    QPixmap pixRect(led.size());
    led.render(&pixRect);
    QVERIFY(!pixRect.isNull());
}

void tst_QLedIndicator::extremeResizeNoCrash()
{
    QLedIndicator led;
    led.resize(1, 1);
    QPixmap pixSmall(led.size());
    led.render(&pixSmall);

    led.resize(1920, 1080);
    QPixmap pixLarge(led.size());
    led.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QLedIndicator)
#include "tst_QLedIndicator.moc"
