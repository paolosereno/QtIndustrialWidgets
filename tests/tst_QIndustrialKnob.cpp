#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QIndustrialKnob.h>
#include <QtGui/QPixmap>

class tst_QIndustrialKnob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void rangeAndClamping();
    void modes();
    void keyboardInteraction();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_QIndustrialKnob::defaultValues()
{
    QIndustrialKnob knob;
    QCOMPARE(knob.minimum(), 0.0);
    QCOMPARE(knob.maximum(), 100.0);
    QCOMPARE(knob.value(), 25.0);
    QCOMPARE(knob.mode(), QIndustrialKnob::KnobMode::Continuous);
}

void tst_QIndustrialKnob::rangeAndClamping()
{
    QIndustrialKnob knob;
    knob.setRange(0.0, 50.0);

    // Below minimum
    knob.setValue(-25.0);
    QCOMPARE(knob.value(), 0.0);

    // Above maximum
    knob.setValue(75.0);
    QCOMPARE(knob.value(), 50.0);

    // Valid value
    knob.setValue(25.0);
    QCOMPARE(knob.value(), 25.0);
}

void tst_QIndustrialKnob::modes()
{
    QIndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setMode(QIndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);

    QCOMPARE(knob.mode(), QIndustrialKnob::KnobMode::Discrete);
    QCOMPARE(knob.discreteSteps(), 5);

    // In discrete 5 steps over [0, 100], steps are 0, 25, 50, 75, 100
    knob.setValue(22.0);
    QCOMPARE(knob.value(), 25.0);

    knob.setValue(68.0);
    QCOMPARE(knob.value(), 75.0);
}

void tst_QIndustrialKnob::keyboardInteraction()
{
    QIndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(10.0);
    knob.setStep(5.0);

    QTest::keyClick(&knob, Qt::Key_Up);
    QCOMPARE(knob.value(), 15.0);

    QTest::keyClick(&knob, Qt::Key_Down);
    QCOMPARE(knob.value(), 10.0);
}

void tst_QIndustrialKnob::signalEmission()
{
    QIndustrialKnob knob;
    knob.setRange(0.0, 100.0);

    QSignalSpy spy(&knob, &QIndustrialKnob::valueChanged);
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);
}

void tst_QIndustrialKnob::renderOffscreen()
{
    QIndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(45.0);
    knob.resize(120, 120);

    QPixmap pix(knob.size());
    knob.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_QIndustrialKnob::extremeResizeNoCrash()
{
    QIndustrialKnob knob;
    knob.resize(1, 1);
    QPixmap pixSmall(knob.size());
    knob.render(&pixSmall);

    knob.resize(2560, 1440);
    QPixmap pixLarge(knob.size());
    knob.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QIndustrialKnob)
#include "tst_QIndustrialKnob.moc"
