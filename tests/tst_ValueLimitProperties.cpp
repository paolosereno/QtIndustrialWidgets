// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #246: the value limits of IndustrialKnob and the setpoint limits of IndustrialSlider as Q_PROPERTY. The tests go
// through the meta-object only (setProperty(), property(), QMetaProperty::reset()), as Designer and a .ui file do

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSlider.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

using namespace QtIndustrialWidgets;

namespace {

// One of the two widgets, with the names of its limit properties and of their signal
struct LimitWidget
{
    std::unique_ptr<QWidget> widget;
    const char *low;
    const char *high;
    const char *signal;
};

LimitWidget makeWidget(const QString &kind)
{
    if (kind == QLatin1String("knob")) {
        return {std::make_unique<IndustrialKnob>(), "valueLimitLow", "valueLimitHigh",
                SIGNAL(valueLimitsChanged(double,double))};
    }
    return {std::make_unique<IndustrialSlider>(), "setpointLimitLow", "setpointLimitHigh",
            SIGNAL(setpointLimitsChanged(double,double))};
}

double read(const LimitWidget &w, const char *name)
{
    return w.widget->property(name).toDouble();
}

// The range through its properties; minimum first when the range grows downwards, so that min < max at every step
void setRange(const LimitWidget &w, double minimum, double maximum)
{
    if (minimum < w.widget->property("maximum").toDouble()) {
        QVERIFY(w.widget->setProperty("minimum", minimum));
        QVERIFY(w.widget->setProperty("maximum", maximum));
    } else {
        QVERIFY(w.widget->setProperty("maximum", maximum));
        QVERIFY(w.widget->setProperty("minimum", minimum));
    }
    QCOMPARE(w.widget->property("minimum").toDouble(), minimum);
    QCOMPARE(w.widget->property("maximum").toDouble(), maximum);
}

void resetProperty(const LimitWidget &w, const char *name)
{
    const QMetaObject *mo = w.widget->metaObject();
    const int index = mo->indexOfProperty(name);
    QVERIFY2(index >= 0, name);
    QVERIFY2(mo->property(index).reset(w.widget.get()), name);
}

void addKinds()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("knob") << QStringLiteral("knob");
    QTest::newRow("slider") << QStringLiteral("slider");
}

} // namespace

class tst_ValueLimitProperties : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void properties_data() { addKinds(); }
    void properties();
    void roundTripInsideTheRange_data() { addKinds(); }
    void roundTripInsideTheRange();
    void aBoundNotSetFollowsTheRange_data() { addKinds(); }
    void aBoundNotSetFollowsTheRange();
    void resetClearsThatBoundOnly_data() { addKinds(); }
    void resetClearsThatBoundOnly();
    void existingSettersUnchanged_data() { addKinds(); }
    void existingSettersUnchanged();
    void theSpanIsKept_data();
    void theSpanIsKept();
    void nonFiniteIsIgnored_data() { addKinds(); }
    void nonFiniteIsIgnored();
    void aLimitChangeRepaints_data() { addKinds(); }
    void aLimitChangeRepaints();
};

// Both properties exist, read and write the limits, can be reset, and notify with the existing signal
void tst_ValueLimitProperties::properties()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    const QMetaObject *mo = w.widget->metaObject();
    for (const char *name : {w.low, w.high}) {
        const int index = mo->indexOfProperty(name);
        QVERIFY2(index >= 0, name);
        const QMetaProperty p = mo->property(index);
        QCOMPARE(p.typeId(), QMetaType::Double);
        QVERIFY(p.isReadable());
        QVERIFY(p.isWritable());
        QVERIFY(p.isResettable());
        QVERIFY(p.isDesignable());
        QVERIFY(p.hasNotifySignal());
        QCOMPARE(p.notifySignal().methodSignature(), QByteArray(w.signal + 1)); // SIGNAL() prefixes a code digit
    }
}

// setProperty() / property() give back the value written when it is inside the range, with one signal per change
void tst_ValueLimitProperties::roundTripInsideTheRange()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    setRange(w, 0, 100);
    QSignalSpy spy(w.widget.get(), w.signal);
    QVERIFY(w.widget->setProperty(w.low, 20.0));
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 100.0);
    QVERIFY(w.widget->setProperty(w.high, 70.0));
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 70.0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toDouble(), 20.0);
    QCOMPARE(spy.at(0).at(1).toDouble(), 100.0);
    QCOMPARE(spy.at(1).at(0).toDouble(), 20.0);
    QCOMPARE(spy.at(1).at(1).toDouble(), 70.0);
    // The same value again: no signal
    QVERIFY(w.widget->setProperty(w.high, 70.0));
    QCOMPARE(spy.count(), 2);
    // Outside the range it is kept and read within the range
    QVERIFY(w.widget->setProperty(w.high, 150.0));
    QCOMPARE(read(w, w.high), 100.0);
    setRange(w, 0, 200);
    QCOMPARE(read(w, w.high), 150.0);
}

// With only the low limit set, the high limit reads the maximum and follows it; and the other way round
void tst_ValueLimitProperties::aBoundNotSetFollowsTheRange()
{
    QFETCH(QString, kind);
    {
        const LimitWidget w = makeWidget(kind);
        setRange(w, 0, 100);
        QVERIFY(w.widget->setProperty(w.low, 20.0));
        QSignalSpy spy(w.widget.get(), w.signal);
        setRange(w, 0, 200);
        QCOMPARE(read(w, w.low), 20.0);
        QCOMPARE(read(w, w.high), 200.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toDouble(), 200.0);
    }
    {
        const LimitWidget w = makeWidget(kind);
        setRange(w, 0, 100);
        QVERIFY(w.widget->setProperty(w.high, 70.0));
        setRange(w, -50, 100);
        QCOMPARE(read(w, w.low), -50.0);
        QCOMPARE(read(w, w.high), 70.0);
    }
}

// The RESET of one property (Designer's reset arrow) clears that bound only; the other keeps its value
void tst_ValueLimitProperties::resetClearsThatBoundOnly()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    setRange(w, 0, 100);
    QVERIFY(w.widget->setProperty(w.low, 20.0));
    QVERIFY(w.widget->setProperty(w.high, 70.0));
    QSignalSpy spy(w.widget.get(), w.signal);
    resetProperty(w, w.low);
    QCOMPARE(read(w, w.low), 0.0);
    QCOMPARE(read(w, w.high), 70.0);
    QCOMPARE(spy.count(), 1);
    // The low limit is no longer set: it follows the range
    setRange(w, -50, 100);
    QCOMPARE(read(w, w.low), -50.0);
    QCOMPARE(read(w, w.high), 70.0);
    // A second reset changes nothing
    spy.clear();
    resetProperty(w, w.low);
    QCOMPARE(spy.count(), 0);
    resetProperty(w, w.high);
    QCOMPARE(read(w, w.high), 100.0);
    setRange(w, -50, 300);
    QCOMPARE(read(w, w.high), 300.0);
}

// setValueLimits() / setSetpointLimits() set both bounds and clear...() clears both, as before #246
void tst_ValueLimitProperties::existingSettersUnchanged()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    setRange(w, 0, 100);
    const bool knob = kind == QLatin1String("knob");
    if (knob) static_cast<IndustrialKnob *>(w.widget.get())->setValueLimits(20, 70);
    else static_cast<IndustrialSlider *>(w.widget.get())->setSetpointLimits(20, 70);
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 70.0);
    // Both set: neither follows the range
    setRange(w, -50, 200);
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 70.0);
    // The low limit alone set, then clear...(): both follow the range again
    resetProperty(w, w.high);
    QCOMPARE(read(w, w.high), 200.0);
    if (knob) static_cast<IndustrialKnob *>(w.widget.get())->clearValueLimits();
    else static_cast<IndustrialSlider *>(w.widget.get())->clearSetpointLimits();
    QCOMPARE(read(w, w.low), -50.0);
    QCOMPARE(read(w, w.high), 200.0);
}

// #204 item 4: a bound written past the other, when the other is set, moves it by the previous span; a bound not set
// stays not set. The bounds as set are read after the range is widened to ±1000, where a bound not set reads ±1000
void tst_ValueLimitProperties::theSpanIsKept_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<double>("initialLow");  // NaN: not set
    QTest::addColumn<double>("initialHigh"); // NaN: not set
    QTest::addColumn<double>("narrowMinimum"); // The range before the call (from -100..100)
    QTest::addColumn<double>("narrowMaximum");
    QTest::addColumn<bool>("writeLow");
    QTest::addColumn<double>("written");
    QTest::addColumn<double>("expectedLow");
    QTest::addColumn<double>("expectedHigh");
    QTest::addColumn<int>("signalCount"); // At the call, in the narrow range

    const double none = std::numeric_limits<double>::quiet_NaN();
    for (const char *kind : {"knob", "slider"}) {
        const QString k = QString::fromLatin1(kind);
        auto row = [&](const char *name) { return &QTest::newRow(qPrintable(k + QLatin1Char(' ') + QLatin1String(name))); };
        *row("low above high: high + span") << k << 20.0 << 40.0 << -100.0 << 100.0 << true << 60.0 << 60.0 << 80.0 << 1;
        *row("low on high: high + span") << k << 20.0 << 40.0 << -100.0 << 100.0 << true << 40.0 << 40.0 << 60.0 << 1;
        *row("low below high: high kept") << k << 20.0 << 40.0 << -100.0 << 100.0 << true << 39.0 << 39.0 << 40.0 << 1;
        *row("high below low: low - span") << k << 20.0 << 40.0 << -100.0 << 100.0 << false << 10.0 << -10.0 << 10.0 << 1;
        *row("high on low: low - span") << k << 20.0 << 40.0 << -100.0 << 100.0 << false << 20.0 << 0.0 << 20.0 << 1;
        *row("high above low: low kept") << k << 20.0 << 40.0 << -100.0 << 100.0 << false << 21.0 << 20.0 << 21.0 << 1;
        // The other bound not set stays not set and follows the range
        *row("high below low not set") << k << none << none << -100.0 << 100.0 << false << -150.0 << -1000.0 << -150.0 << 1;
        *row("low above high not set") << k << none << none << -100.0 << 100.0 << true << 100.0 << 100.0 << 1000.0 << 1;
        // The other bound set, the one written not set: the span from the end of the range
        *row("high below low set alone") << k << 20.0 << none << -100.0 << 100.0 << false << 10.0 << -70.0 << 10.0 << 1;
        *row("low above high set alone") << k << none << 40.0 << -100.0 << 100.0 << true << 50.0 << 50.0 << 190.0 << 1;
        // ... and that span not positive (the other bound kept past the end of the narrowed range): it goes to the
        // value written, so the limits are never inverted
        *row("low above high set past the minimum") << k << none << 20.0 << 50.0 << 100.0 << true << 60.0 << 60.0 << 60.0 << 1;
        *row("high below low set past the maximum") << k << 80.0 << none << -100.0 << 50.0 << false << 40.0 << 40.0 << 40.0 << 1;
        // #246 review: the other bound not set stays not set and follows the range, whatever the span (scale 0-100)
        *row("low past the end, high not set") << k << 20.0 << none << 0.0 << 100.0 << true << 150.0 << 150.0 << 1000.0 << 1;
        *row("high past the end, low not set") << k << none << 80.0 << 0.0 << 100.0 << false << -50.0 << -1000.0 << -50.0 << 1;
        // The bound written set past the end of the narrowed range, the other not set: only that bound is set
        *row("low past the narrowed end") << k << 80.0 << none << -100.0 << 50.0 << true << 60.0 << 60.0 << 1000.0 << 0;
        *row("high past the narrowed end") << k << none << 20.0 << 50.0 << 100.0 << false << 40.0 << -1000.0 << 40.0 << 0;
        // The same with the bound on the end of the narrowed range
        *row("low on the narrowed end") << k << 50.0 << none << -100.0 << 50.0 << true << 60.0 << 60.0 << 1000.0 << 0;
        *row("high on the narrowed end") << k << none << 50.0 << 50.0 << 100.0 << false << 40.0 << -1000.0 << 40.0 << 0;
    }
}

void tst_ValueLimitProperties::theSpanIsKept()
{
    QFETCH(QString, kind);
    QFETCH(double, initialLow);
    QFETCH(double, initialHigh);
    QFETCH(double, narrowMinimum);
    QFETCH(double, narrowMaximum);
    QFETCH(bool, writeLow);
    QFETCH(double, written);
    QFETCH(double, expectedLow);
    QFETCH(double, expectedHigh);
    QFETCH(int, signalCount);

    const LimitWidget w = makeWidget(kind);
    setRange(w, -100, 100);
    if (!std::isnan(initialLow)) QVERIFY(w.widget->setProperty(w.low, initialLow));
    if (!std::isnan(initialHigh)) QVERIFY(w.widget->setProperty(w.high, initialHigh));
    QCOMPARE(read(w, w.low), std::isnan(initialLow) ? -100.0 : initialLow);
    QCOMPARE(read(w, w.high), std::isnan(initialHigh) ? 100.0 : initialHigh);
    setRange(w, narrowMinimum, narrowMaximum);

    QSignalSpy spy(w.widget.get(), w.signal);
    QVERIFY(w.widget->setProperty(writeLow ? w.low : w.high, written));
    QCOMPARE(spy.count(), signalCount);
    // As read in the narrow range
    QCOMPARE(read(w, w.low), std::clamp(expectedLow, narrowMinimum, narrowMaximum));
    QCOMPARE(read(w, w.high), std::clamp(expectedHigh, narrowMinimum, narrowMaximum));
    // As set
    setRange(w, -1000, 1000);
    QCOMPARE(read(w, w.low), expectedLow);
    QCOMPARE(read(w, w.high), expectedHigh);
}

void tst_ValueLimitProperties::nonFiniteIsIgnored()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    setRange(w, 0, 100);
    QVERIFY(w.widget->setProperty(w.low, 20.0));
    QVERIFY(w.widget->setProperty(w.high, 70.0));
    QSignalSpy spy(w.widget.get(), w.signal);
    for (const double v : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
        QVERIFY(w.widget->setProperty(w.low, v));
        QVERIFY(w.widget->setProperty(w.high, v));
    }
    QCOMPARE(spy.count(), 0);
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 70.0);
    // Still set: they do not follow the range
    setRange(w, -50, 200);
    QCOMPARE(read(w, w.low), 20.0);
    QCOMPARE(read(w, w.high), 70.0);
}

namespace {

class PaintCounter : public QObject
{
public:
    int paints{0};

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) ++paints;
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

// The limits are drawn (hatching, stop marks): a change through a property or a reset schedules a repaint
void tst_ValueLimitProperties::aLimitChangeRepaints()
{
    QFETCH(QString, kind);
    const LimitWidget w = makeWidget(kind);
    setRange(w, 0, 100);
    PaintCounter counter;
    w.widget->installEventFilter(&counter);
    w.widget->resize(w.widget->sizeHint());
    w.widget->show();
    QVERIFY(QTest::qWaitForWindowExposed(w.widget.get()));
    QTRY_VERIFY(counter.paints > 0); // The first paint after show
    QTest::qWait(50);
    counter.paints = 0;
    QVERIFY(w.widget->setProperty(w.low, 20.0));
    QTRY_VERIFY(counter.paints > 0);
    QTest::qWait(50);
    counter.paints = 0;
    resetProperty(w, w.low);
    QTRY_VERIFY(counter.paints > 0);
}

QTEST_MAIN(tst_ValueLimitProperties)
#include "tst_ValueLimitProperties.moc"
