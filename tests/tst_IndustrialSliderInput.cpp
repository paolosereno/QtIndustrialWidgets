// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #180, part 2: IndustrialSlider as a control, under the rules of #182. A press never changes the setpoint; a drag
// starts only on the drawn handle and is relative; keys step and commit at the release; the wheel is off by default
// and acts only with focus; read-only passes the input on and shows a padlock; the setpoint limits bind the operator
// only and are drawn on the scale; the focus ring.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialSlider.h>

#include "internal/IndustrialSliderLayout.h"
#include "internal/IndustrialSliderTestAccess.h"
#include "internal/RadialGaugeLayout.h"

#include "IndustrialSliderTestUtils.h"
#include "PixelTestUtils.h"

#include <QtGui/QImage>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_IndustrialSliderInput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void stepsRejectBadValues();
    void pressNeverChangesTheSetpoint_data();
    void pressNeverChangesTheSetpoint();
    void dragIsRelativeAndStartsOnlyOnTheHandle_data();
    void dragIsRelativeAndStartsOnlyOnTheHandle();
    void dragCommitsOnceAtTheRelease();
    void dragPastAnEndStaysThere_data();
    void dragPastAnEndStaysThere();
    void keysStepAndCommitAtTheRelease();
    void heldKeyCommitsOnceAtTheRelease_data();
    void heldKeyCommitsOnceAtTheRelease();
    void keyAtTheEndDoesNotCommit();
    void setSetpointNeverCommits();
    void focusOutAndHideEndTheAction_data();
    void focusOutAndHideEndTheAction();
    void readOnlyIgnoresTheOperatorInput();
    void readOnlyEndsTheRunningAction_data();
    void readOnlyEndsTheRunningAction();
    void readOnlyDrawsThePadlockBeforeTheTag_data();
    void readOnlyDrawsThePadlockBeforeTheTag();
    void padlockCountsInThePanelMinimum();
    void wheelIsOffByDefault();
    void wheelStepsOnlyWithFocus();
    void limitsAreReadWithinTheRange();
    void limitsBindTheOperatorOnly();
    void fromOutsideTheLimitsOnlyTowardsThem();
    void limitsAreHatchedWithStopMarks_data();
    void limitsAreHatchedWithStopMarks();
    void focusRingIsASolidRingInTextColor_data();
    void focusRingIsASolidRingInTextColor();
};

namespace {

using namespace SliderTest;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Sizes whose edges fall on device pixels at DPR 1, 1.25, 1.5 and 2
QSize sizeFor(bool vertical)
{
    return vertical ? QSize(140, 300) : QSize(300, 92);
}

// The layout of the slider as it is drawn at its size
internal::SliderLayout layoutOf(IndustrialSlider &s)
{
    (void)render(s);
    return internal::IndustrialSliderTestAccess::lastLayout(s);
}

double alongOf(const internal::SliderLayout &l, const IndustrialSlider &s, double value)
{
    return l.along((value - s.minimum()) / (s.maximum() - s.minimum()));
}

QPointF point(const internal::SliderLayout &l, double along, double across)
{
    return l.vertical ? QPointF(across, along) : QPointF(along, across);
}

// A point well inside the drawn handle: the centre of its outline
QPointF handlePoint(IndustrialSlider &s)
{
    const internal::SliderLayout l = layoutOf(s);
    return internal::sliderHandlePolygon(l, alongOf(l, s, s.setpoint())).boundingRect().center();
}

// The point \a pixels further along the scale (towards the maximum) than \a p
QPointF moved(const internal::SliderLayout &l, const QPointF &p, double pixels)
{
    return l.vertical ? p - QPointF(0.0, pixels) : p + QPointF(pixels, 0.0);
}

void sendMouse(IndustrialSlider &s, QEvent::Type type, const QPointF &p)
{
    const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type, p, s.mapToGlobal(p), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&s, &event);
}

void sendKey(IndustrialSlider &s, QEvent::Type type, int key, bool autoRepeat)
{
    QKeyEvent event(type, key, Qt::NoModifier, QString(), autoRepeat);
    QCoreApplication::sendEvent(&s, &event);
}

// Sends one wheel event of \a delta units (120 = one notch) and returns whether the slider accepted it
bool sendWheel(IndustrialSlider &s, int delta)
{
    const QPointF p(s.width() * 0.5, s.height() * 0.5);
    QWheelEvent wheel(p, s.mapToGlobal(p), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&s, &wheel);
    return wheel.isAccepted();
}

bool focusOn(QWidget &widget)
{
    QWidget *window = widget.window();
    window->show();
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window)) {
        return false;
    }
    widget.setFocus(Qt::OtherFocusReason);
    return QTest::qWaitFor([&] { return widget.hasFocus(); });
}

// A parent that takes the events the slider passes on
class RecordingParent : public QWidget
{
public:
    int keyPresses{0};
    int mousePresses{0};

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPresses;
        event->accept();
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        ++mousePresses;
        event->accept();
    }
};

// A slider 0-100 at \a setpoint, shown at its test size
void showSlider(IndustrialSlider &s, bool vertical, double setpoint)
{
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setRange(0.0, 100.0);
    s.setSetpoint(setpoint);
}

} // namespace

void tst_IndustrialSliderInput::defaults()
{
    IndustrialSlider s;
    QCOMPARE(s.step(), 1.0);
    QCOMPARE(s.pageStep(), 5.0);
    QVERIFY(!s.isReadOnly());
    QVERIFY(!s.isWheelEnabled());
    QCOMPARE(s.setpointLimitLow(), s.minimum());
    QCOMPARE(s.setpointLimitHigh(), s.maximum());
    QCOMPARE(s.focusPolicy(), Qt::StrongFocus);
    // Until a page step is set it follows the step
    s.setStep(0.5);
    QCOMPARE(s.pageStep(), 2.5);
    s.setPageStep(10.0);
    s.setStep(2.0);
    QCOMPARE(s.pageStep(), 10.0);
    s.resetPageStep();
    QCOMPARE(s.pageStep(), 10.0); // 5 x step(), 2
}

void tst_IndustrialSliderInput::stepsRejectBadValues()
{
    IndustrialSlider s;
    s.setStep(2.0);
    s.setPageStep(7.0);
    QSignalSpy appearance(&s, &IndustrialSlider::appearanceChanged);
    for (double bad : {0.0, -1.0, kNaN, kInf, -kInf}) {
        s.setStep(bad);
        s.setPageStep(bad);
    }
    QCOMPARE(s.step(), 2.0);
    QCOMPARE(s.pageStep(), 7.0);
    QCOMPARE(appearance.count(), 0);
    s.setStep(2.0);
    s.setPageStep(7.0);
    QCOMPARE(appearance.count(), 0);
    s.setStep(3.0);
    QCOMPARE(appearance.count(), 1);
}

void tst_IndustrialSliderInput::pressNeverChangesTheSetpoint_data()
{
    addOrientations();
}

// #182: a press, on the handle or anywhere else, never changes the setpoint; a press off the handle starts no drag
void tst_IndustrialSliderInput::pressNeverChangesTheSetpoint()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    showSlider(s, vertical, 30.0);
    const internal::SliderLayout l = layoutOf(s);
    QSignalSpy changed(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    const double trackMid = vertical ? l.track.center().x() : l.track.center().y();
    // On the track at 80 %, on the ticks at 60 %, on a readout: each press, then a move of 40 px, then the release
    const QPointF elsewhere[] = {point(l, alongOf(l, s, 80.0), trackMid),
                                 point(l, alongOf(l, s, 60.0), l.tickNear + 2.0), l.setpointPod.center()};
    for (const QPointF &p : elsewhere) {
        sendMouse(s, QEvent::MouseButtonPress, p);
        sendMouse(s, QEvent::MouseMove, moved(l, p, 40.0));
        sendMouse(s, QEvent::MouseButtonRelease, moved(l, p, 40.0));
    }
    QCOMPARE(s.setpoint(), 30.0);
    // On the handle: the press alone changes nothing
    const QPointF h = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h);
    QCOMPARE(s.setpoint(), 30.0);
    sendMouse(s, QEvent::MouseButtonRelease, h);
    QCOMPARE(s.setpoint(), 30.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
}

void tst_IndustrialSliderInput::dragIsRelativeAndStartsOnlyOnTheHandle_data()
{
    addOrientations();
}

// #182: a drag from anywhere on the drawn handle moves the setpoint by the distance the pointer moves along the
// scale; the handle does not jump to the pointer
void tst_IndustrialSliderInput::dragIsRelativeAndStartsOnlyOnTheHandle()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    showSlider(s, vertical, 30.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    QVERIFY(length > 100.0);
    const QPolygonF handle = internal::sliderHandlePolygon(l, alongOf(l, s, 30.0));
    const QRectF box = handle.boundingRect();
    // Two points of the handle off its axis: nearer the minimum and nearer the maximum
    const double c = vertical ? box.center().x() : box.center().y();
    const double halfAlong = (vertical ? box.height() : box.width()) * 0.25;
    for (double offset : {-halfAlong, halfAlong}) {
        s.setSetpoint(30.0);
        const QPointF press = point(l, alongOf(l, s, 30.0) + offset, c);
        QVERIFY2(handle.containsPoint(press, Qt::OddEvenFill), "the press point is not on the handle");
        sendMouse(s, QEvent::MouseButtonPress, press);
        sendMouse(s, QEvent::MouseMove, moved(l, press, 0.0));
        QCOMPARE(s.setpoint(), 30.0); // No jump to the pointer
        sendMouse(s, QEvent::MouseMove, moved(l, press, length * 0.25));
        QVERIFY2(std::abs(s.setpoint() - 55.0) < 1e-9, qPrintable(QString::number(s.setpoint(), 'g', 17)));
        sendMouse(s, QEvent::MouseMove, moved(l, press, -length * 0.1));
        QVERIFY2(std::abs(s.setpoint() - 20.0) < 1e-9, qPrintable(QString::number(s.setpoint(), 'g', 17)));
        sendMouse(s, QEvent::MouseButtonRelease, moved(l, press, -length * 0.1));
    }
    // Just outside the handle, beside it along the scale: no drag
    s.setSetpoint(30.0);
    const double beside = (vertical ? box.height() : box.width()) * 0.5 + 2.0;
    const QPointF off = point(l, alongOf(l, s, 30.0) + beside, c);
    QVERIFY(!handle.containsPoint(off, Qt::OddEvenFill));
    sendMouse(s, QEvent::MouseButtonPress, off);
    sendMouse(s, QEvent::MouseMove, moved(l, off, length * 0.25));
    sendMouse(s, QEvent::MouseButtonRelease, moved(l, off, length * 0.25));
    QCOMPARE(s.setpoint(), 30.0);
}

// #182: setpointChanged() at each move, setpointCommitted() once at the release; away and back commits nothing
void tst_IndustrialSliderInput::dragCommitsOnceAtTheRelease()
{
    IndustrialSlider s;
    showSlider(s, false, 50.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    QSignalSpy changed(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    const QPointF h = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h);
    for (double f : {0.05, 0.10, 0.20}) {
        sendMouse(s, QEvent::MouseMove, moved(l, h, length * f));
    }
    QCOMPARE(changed.count(), 3);
    QCOMPARE(committed.count(), 0);
    sendMouse(s, QEvent::MouseButtonRelease, moved(l, h, length * 0.20));
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), s.setpoint());
    QVERIFY(std::abs(s.setpoint() - 70.0) < 1e-9);

    const QPointF h2 = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h2);
    sendMouse(s, QEvent::MouseMove, moved(l, h2, -length * 0.1));
    sendMouse(s, QEvent::MouseMove, h2);
    sendMouse(s, QEvent::MouseButtonRelease, h2);
    QCOMPARE(changed.count(), 5);
    QCOMPARE(committed.count(), 1);
}

void tst_IndustrialSliderInput::dragPastAnEndStaysThere_data()
{
    addOrientations();
}

// #182: past an end the setpoint stays at it; coming back, it follows the pointer from the press again
void tst_IndustrialSliderInput::dragPastAnEndStaysThere()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    showSlider(s, vertical, 90.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    const QPointF h = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h);
    sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.5));
    QCOMPARE(s.setpoint(), 100.0);
    sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.05));
    QVERIFY(std::abs(s.setpoint() - 95.0) < 1e-9);
    sendMouse(s, QEvent::MouseMove, moved(l, h, -length * 2.0));
    QCOMPARE(s.setpoint(), 0.0);
    sendMouse(s, QEvent::MouseButtonRelease, moved(l, h, -length * 2.0));
    QCOMPARE(s.setpoint(), 0.0);
}

// #182: the arrows by step(), the page keys by pageStep(), Home and End to the ends; each commits at its release
void tst_IndustrialSliderInput::keysStepAndCommitAtTheRelease()
{
    IndustrialSlider s;
    s.setRange(0.0, 100.0);
    s.setSetpoint(50.0);
    s.setStep(2.0);
    s.setPageStep(10.0);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    struct Step {
        Qt::Key key;
        double expected;
    };
    const Step steps[] = {{Qt::Key_Up, 52.0},       {Qt::Key_Right, 54.0}, {Qt::Key_Down, 52.0},
                          {Qt::Key_Left, 50.0},     {Qt::Key_PageUp, 60.0}, {Qt::Key_PageDown, 50.0},
                          {Qt::Key_End, 100.0},     {Qt::Key_Home, 0.0}};
    int n = 0;
    for (const Step &step : steps) {
        sendKey(s, QEvent::KeyPress, step.key, false);
        QCOMPARE(s.setpoint(), step.expected);
        QCOMPARE(committed.count(), n);
        sendKey(s, QEvent::KeyRelease, step.key, false);
        QCOMPARE(committed.count(), ++n);
        QCOMPARE(committed.last().at(0).toDouble(), step.expected);
    }
}

void tst_IndustrialSliderInput::heldKeyCommitsOnceAtTheRelease_data()
{
    QTest::addColumn<bool>("releasePairs");
    QTest::newRow("press and release pairs (xcb)") << true;
    QTest::newRow("presses only (Windows, macOS)") << false;
}

void tst_IndustrialSliderInput::heldKeyCommitsOnceAtTheRelease()
{
    QFETCH(bool, releasePairs);
    IndustrialSlider s;
    s.setRange(0.0, 100.0);
    s.setSetpoint(50.0);
    QSignalSpy changed(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    sendKey(s, QEvent::KeyPress, Qt::Key_Up, false);
    for (int i = 0; i < 3; ++i) {
        if (releasePairs) {
            sendKey(s, QEvent::KeyRelease, Qt::Key_Up, true);
        }
        sendKey(s, QEvent::KeyPress, Qt::Key_Up, true);
    }
    QCOMPARE(s.setpoint(), 54.0);
    QCOMPARE(changed.count(), 4);
    QCOMPARE(committed.count(), 0);
    sendKey(s, QEvent::KeyRelease, Qt::Key_Up, false);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), 54.0);
}

void tst_IndustrialSliderInput::keyAtTheEndDoesNotCommit()
{
    IndustrialSlider s;
    s.setRange(0.0, 100.0);
    s.setSetpoint(100.0);
    QSignalSpy changed(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    for (int key : {Qt::Key_Up, Qt::Key_Right, Qt::Key_PageUp, Qt::Key_End}) {
        QTest::keyClick(&s, static_cast<Qt::Key>(key));
    }
    QCOMPARE(s.setpoint(), 100.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
}

// #182: the application's writes never commit, also during a key action, and a range that clamps neither
void tst_IndustrialSliderInput::setSetpointNeverCommits()
{
    IndustrialSlider s;
    s.setRange(0.0, 100.0);
    QSignalSpy changed(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    s.setSetpoint(40.0);
    s.setSetpoint(60.0);
    s.setRange(0.0, 50.0);
    QCOMPARE(s.setpoint(), 50.0);
    QCOMPARE(changed.count(), 3);
    QCOMPARE(committed.count(), 0);
    // A key action that ends where it started, the application having moved the setpoint back: nothing to commit
    sendKey(s, QEvent::KeyPress, Qt::Key_Down, false);
    s.setSetpoint(50.0);
    sendKey(s, QEvent::KeyRelease, Qt::Key_Down, false);
    QCOMPARE(committed.count(), 0);
}

void tst_IndustrialSliderInput::focusOutAndHideEndTheAction_data()
{
    QTest::addColumn<bool>("drag");
    QTest::addColumn<bool>("hide");
    QTest::newRow("drag, focus out") << true << false;
    QTest::newRow("drag, hidden") << true << true;
    QTest::newRow("held key, focus out") << false << false;
    QTest::newRow("held key, hidden") << false << true;
}

// #182: the release will not come; the action ends there and commits once; what follows does not move the setpoint
void tst_IndustrialSliderInput::focusOutAndHideEndTheAction()
{
    QFETCH(bool, drag);
    QFETCH(bool, hide);
    QWidget window;
    auto *box = new QVBoxLayout(&window);
    auto *s = new IndustrialSlider(&window);
    auto *other = new QLineEdit(&window);
    box->addWidget(s);
    box->addWidget(other);
    s->setRange(0.0, 100.0);
    s->setSetpoint(50.0);
    window.resize(320, 200);
    // Hidden with the focus, the slider would lose it first and the focus out would end the action: the hide is
    // checked on a slider without focus (the events are sent to it directly)
    QVERIFY(focusOn(hide ? static_cast<QWidget &>(*other) : static_cast<QWidget &>(*s)));
    const internal::SliderLayout l = layoutOf(*s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    QSignalSpy committed(s, &IndustrialSlider::setpointCommitted);
    const QPointF h = handlePoint(*s);
    if (drag) {
        sendMouse(*s, QEvent::MouseButtonPress, h);
        sendMouse(*s, QEvent::MouseMove, moved(l, h, length * 0.1));
    } else {
        sendKey(*s, QEvent::KeyPress, Qt::Key_PageUp, false);
        sendKey(*s, QEvent::KeyPress, Qt::Key_PageUp, true);
    }
    const double reached = s->setpoint();
    QVERIFY(reached > 50.0);
    QCOMPARE(committed.count(), 0);
    if (hide) {
        QVERIFY(!s->hasFocus());
        s->hide();
    } else {
        QVERIFY(focusOn(*other));
    }
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), reached);
    if (drag) {
        sendMouse(*s, QEvent::MouseMove, moved(l, h, length * 0.3));
        sendMouse(*s, QEvent::MouseButtonRelease, moved(l, h, length * 0.3));
    } else {
        sendKey(*s, QEvent::KeyRelease, Qt::Key_PageUp, false);
    }
    QCOMPARE(s->setpoint(), reached);
    QCOMPARE(committed.count(), 1);
}

// #182: read-only passes the keys, the press and the wheel on to the parent and changes nothing; the slider still
// takes focus and setSetpoint() still works; off again, the same input acts
void tst_IndustrialSliderInput::readOnlyIgnoresTheOperatorInput()
{
    RecordingParent parent;
    parent.resize(340, 120);
    auto *s = new IndustrialSlider(&parent);
    s->setRange(0.0, 100.0);
    s->setWheelEnabled(true);
    s->resize(300, 92);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    QVERIFY(focusOn(*s));
    s->setSetpoint(50.0);
    const internal::SliderLayout l = layoutOf(*s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    const QPointF h = handlePoint(*s);

    QSignalSpy readOnly(s, &IndustrialSlider::readOnlyChanged);
    s->setReadOnly(true);
    s->setReadOnly(true);
    QVERIFY(s->isReadOnly());
    QCOMPARE(readOnly.count(), 1);
    QCOMPARE(readOnly.at(0).at(0).toBool(), true);

    QSignalSpy changed(s, &IndustrialSlider::setpointChanged);
    QSignalSpy committed(s, &IndustrialSlider::setpointCommitted);
    for (const Qt::Key key : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_PageUp, Qt::Key_PageDown,
                              Qt::Key_Home, Qt::Key_End}) {
        const int before = parent.keyPresses;
        QTest::keyClick(s, key);
        QVERIFY2(parent.keyPresses == before + 1,
                 qPrintable(QStringLiteral("key %1 did not reach the parent").arg(QKeySequence(key).toString())));
    }
    QTest::mousePress(s, Qt::LeftButton, Qt::NoModifier, h.toPoint());
    QCOMPARE(parent.mousePresses, 1);
    sendMouse(*s, QEvent::MouseMove, moved(l, h, length * 0.2));
    sendMouse(*s, QEvent::MouseButtonRelease, moved(l, h, length * 0.2));
    QVERIFY(!sendWheel(*s, 120));
    QVERIFY(!sendWheel(*s, -240));
    QCOMPARE(s->setpoint(), 50.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
    QVERIFY(s->hasFocus());

    s->setSetpoint(60.0);
    QCOMPARE(s->setpoint(), 60.0);
    QCOMPARE(committed.count(), 0);

    s->setReadOnly(false);
    QCOMPARE(readOnly.count(), 2);
    const int keysBefore = parent.keyPresses;
    QTest::keyClick(s, Qt::Key_Up);
    QCOMPARE(s->setpoint(), 61.0);
    QCOMPARE(parent.keyPresses, keysBefore);
    QVERIFY(sendWheel(*s, 120));
    QCOMPARE(s->setpoint(), 62.0);
    s->setSetpoint(50.0);
    const QPointF h2 = handlePoint(*s);
    QTest::mousePress(s, Qt::LeftButton, Qt::NoModifier, h2.toPoint());
    QCOMPARE(parent.mousePresses, 1);
    sendMouse(*s, QEvent::MouseMove, moved(l, h2, length * 0.2));
    sendMouse(*s, QEvent::MouseButtonRelease, moved(l, h2, length * 0.2));
    QVERIFY2(std::abs(s->setpoint() - 70.0) < 1.0, qPrintable(QString::number(s->setpoint())));
}

void tst_IndustrialSliderInput::readOnlyEndsTheRunningAction_data()
{
    QTest::addColumn<bool>("drag");
    QTest::newRow("drag") << true;
    QTest::newRow("held key") << false;
}

void tst_IndustrialSliderInput::readOnlyEndsTheRunningAction()
{
    QFETCH(bool, drag);
    IndustrialSlider s;
    showSlider(s, false, 50.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    const QPointF h = handlePoint(s);
    if (drag) {
        sendMouse(s, QEvent::MouseButtonPress, h);
        sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.1));
    } else {
        sendKey(s, QEvent::KeyPress, Qt::Key_PageUp, false);
        sendKey(s, QEvent::KeyPress, Qt::Key_PageUp, true);
    }
    const double reached = s.setpoint();
    QVERIFY(reached > 50.0);
    s.setReadOnly(true);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), reached);
    if (drag) {
        sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.3));
        sendMouse(s, QEvent::MouseButtonRelease, moved(l, h, length * 0.3));
    } else {
        sendKey(s, QEvent::KeyPress, Qt::Key_PageUp, true);
        sendKey(s, QEvent::KeyRelease, Qt::Key_PageUp, false);
    }
    QCOMPARE(s.setpoint(), reached);
    QCOMPARE(committed.count(), 1);
}

void tst_IndustrialSliderInput::readOnlyDrawsThePadlockBeforeTheTag_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<bool>("atMinimum");
    for (bool vertical : {false, true}) {
        for (bool atMinimum : {false, true}) {
            QTest::newRow(qPrintable(QStringLiteral("%1, %2").arg(vertical ? "vertical" : "horizontal",
                                                                  atMinimum ? "minimum size" : "test size")))
                << vertical << atMinimum;
        }
    }
}

// #180: read-only draws a padlock in the setpoint panel, before "SP", in readoutColor(): a solid body as wide as
// 0.8 of the capitals, which no glyph of the readout has. It is never left out: at the minimum size too, which grows
// to hold it
void tst_IndustrialSliderInput::readOnlyDrawsThePadlockBeforeTheTag()
{
    QFETCH(bool, vertical);
    QFETCH(bool, atMinimum);
    IndustrialSlider s;
    setVertical(s, vertical);
    s.setRange(-100000.0, 100000.0);
    s.setUnit(QStringLiteral("m³ (15 °C)"));
    const QSize plainMin = s.minimumSizeHint();
    s.setReadOnly(true);
    const QSize lockedMin = s.minimumSizeHint();
    QVERIFY2(lockedMin.width() >= plainMin.width() && lockedMin.height() >= plainMin.height(),
             qPrintable(QStringLiteral("minimum %1x%2, read-only %3x%4")
                            .arg(plainMin.width()).arg(plainMin.height()).arg(lockedMin.width()).arg(lockedMin.height())));
    prepare(s, atMinimum ? lockedMin : sizeFor(vertical));
    s.setSetpoint(-99999.9);

    const auto solidBlock = [&](const QImage &img, const QRectF &pod) {
        // The widest run of readout pixels in a row, held over 3 rows: the padlock body
        const double dpr = img.devicePixelRatio();
        const QRect r = QRectF(pod.x() * dpr, pod.y() * dpr, pod.width() * dpr, pod.height() * dpr).toAlignedRect()
                            .intersected(img.rect());
        int best = 0;
        int bestLeft = -1;
        for (int y = r.top(); y + 2 <= r.bottom(); ++y) {
            int run = 0;
            for (int x = r.left(); x <= r.right(); ++x) {
                const bool solid = near(img.pixel(x, y), kReadout, 40) && near(img.pixel(x, y + 1), kReadout, 40)
                                   && near(img.pixel(x, y + 2), kReadout, 40);
                run = solid ? run + 1 : 0;
                if (run > best) {
                    best = run;
                    bestLeft = x - run + 1;
                }
            }
        }
        return std::make_pair(best, bestLeft);
    };
    const auto leftmostInk = [&](const QImage &img, const QRectF &pod) {
        const double dpr = img.devicePixelRatio();
        const QRect r = QRectF(pod.x() * dpr, pod.y() * dpr, pod.width() * dpr, pod.height() * dpr).toAlignedRect()
                            .intersected(img.rect());
        for (int x = r.left(); x <= r.right(); ++x) {
            for (int y = r.top(); y <= r.bottom(); ++y) {
                if (near(img.pixel(x, y), kReadout, 110)) {
                    return x;
                }
            }
        }
        return -1;
    };

    const QImage locked = render(s);
    const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
    const QString text = internal::IndustrialSliderTestAccess::lastSetpointReadout(s);
    QVERIFY2(text.startsWith(QStringLiteral("🔒 SP")), qPrintable(text));
    QVERIFY(internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s)
            >= internal::minimumReadoutPixelSize(s.logicalDpiY()));
    QFont readoutFont = s.font();
    readoutFont.setPixelSize(internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s));
    const double cap = QFontMetricsF(readoutFont).capHeight() * locked.devicePixelRatio();
    const auto [lockedRun, lockedLeft] = solidBlock(locked, l.setpointPod);
    QVERIFY2(lockedRun >= std::floor(cap * 0.8) - 2,
             qPrintable(QStringLiteral("widest solid run %1 px, capitals %2 px").arg(lockedRun).arg(cap)));
    // It is the first thing in the panel
    QVERIFY2(lockedLeft <= leftmostInk(locked, l.setpointPod) + 2,
             qPrintable(QStringLiteral("solid run at %1, first ink at %2").arg(lockedLeft).arg(leftmostInk(locked, l.setpointPod))));

    // Control: without read-only no glyph has such a body
    s.setReadOnly(false);
    const QImage plain = render(s);
    QVERIFY(!internal::IndustrialSliderTestAccess::lastSetpointReadout(s).contains(QStringLiteral("🔒")));
    const auto plainRun = solidBlock(plain, internal::IndustrialSliderTestAccess::lastLayout(s).setpointPod).first;
    QVERIFY2(plainRun < std::floor(cap * 0.8) - 2,
             qPrintable(QStringLiteral("without read-only a solid run of %1 px").arg(plainRun)));
}

// #180: the padlock is never left out, so the minimum width of a panel holds it at the smallest font. Today that
// minimum never decides minimumSizeHint() (the feedback panel, or the scale and the ticks, need more), so it is read
// from the layout input: without the feedback panel, read-only widens it by at least the padlock
void tst_IndustrialSliderInput::padlockCountsInThePanelMinimum()
{
    for (bool vertical : {false, true}) {
        IndustrialSlider s;
        setVertical(s, vertical);
        s.setFeedbackReadoutVisible(false);
        const double plain = internal::IndustrialSliderTestAccess::minimumPodWidth(s);
        s.setReadOnly(true);
        const double locked = internal::IndustrialSliderTestAccess::minimumPodWidth(s);
        QFont smallest = s.font();
        smallest.setPixelSize(internal::minimumReadoutPixelSize(s.logicalDpiY()));
        const double padlock = QFontMetricsF(smallest).capHeight() * 0.8;
        QVERIFY2(plain > 0.0 && locked >= plain + padlock - 0.01,
                 qPrintable(QStringLiteral("panel minimum %1, read-only %2, padlock %3").arg(plain).arg(locked).arg(padlock)));
        // With the feedback panel the wider of the two is kept, and read-only never makes it narrower
        s.setFeedbackReadoutVisible(true);
        const double withFeedback = internal::IndustrialSliderTestAccess::minimumPodWidth(s);
        QVERIFY(withFeedback >= locked);
    }
}

void tst_IndustrialSliderInput::wheelIsOffByDefault()
{
    QWidget window;
    auto *s = new IndustrialSlider(&window);
    s->setRange(0.0, 100.0);
    s->setSetpoint(50.0);
    s->resize(300, 92);
    QVERIFY(focusOn(*s));
    QSignalSpy changed(s, &IndustrialSlider::setpointChanged);
    QVERIFY(!sendWheel(*s, 120));
    QVERIFY(!sendWheel(*s, -120));
    QCOMPARE(s->setpoint(), 50.0);
    QCOMPARE(changed.count(), 0);
}

// #182: with the wheel on, it acts only with focus; each notch is one step, committed at once; the part of a notch
// not yet reached is dropped at focus out; the limits hold
void tst_IndustrialSliderInput::wheelStepsOnlyWithFocus()
{
    QWidget window;
    auto *box = new QVBoxLayout(&window);
    auto *s = new IndustrialSlider(&window);
    auto *other = new QLineEdit(&window);
    box->addWidget(s);
    box->addWidget(other);
    s->setRange(0.0, 100.0);
    s->setSetpoint(50.0);
    QSignalSpy appearance(s, &IndustrialSlider::appearanceChanged);
    s->setWheelEnabled(true);
    s->setWheelEnabled(true);
    QCOMPARE(appearance.count(), 1);

    QSignalSpy committed(s, &IndustrialSlider::setpointCommitted);
    QVERIFY(focusOn(*other));
    QVERIFY(!sendWheel(*s, 120));
    QCOMPARE(s->setpoint(), 50.0);

    QVERIFY(focusOn(*s));
    QVERIFY(sendWheel(*s, 120));
    QCOMPARE(s->setpoint(), 51.0);
    QCOMPARE(committed.count(), 1);
    QVERIFY(sendWheel(*s, -240));
    QCOMPARE(s->setpoint(), 49.0);
    QCOMPARE(committed.count(), 3);
    QCOMPARE(committed.at(1).at(0).toDouble(), 50.0);
    QVERIFY(sendWheel(*s, 60));
    QCOMPARE(s->setpoint(), 49.0);
    QVERIFY(sendWheel(*s, 60));
    QCOMPARE(s->setpoint(), 50.0);
    QCOMPARE(committed.count(), 4);

    QVERIFY(sendWheel(*s, 80)); // Dropped at focus out
    QVERIFY(focusOn(*other));
    QVERIFY(focusOn(*s));
    QVERIFY(sendWheel(*s, 80));
    QCOMPARE(s->setpoint(), 50.0);
    QVERIFY(sendWheel(*s, 40));
    QCOMPARE(s->setpoint(), 51.0);
    QCOMPARE(committed.count(), 5);

    s->setSetpointLimits(0.0, 52.0);
    QVERIFY(sendWheel(*s, 360));
    QCOMPARE(s->setpoint(), 52.0);
    QCOMPARE(committed.count(), 6);
    QVERIFY(sendWheel(*s, 120)); // At the limit: nothing to commit
    QCOMPARE(committed.count(), 6);
}

// The limits are kept as set and read within the range; bad limits are ignored; a range that moves the limits read
// emits setpointLimitsChanged()
void tst_IndustrialSliderInput::limitsAreReadWithinTheRange()
{
    IndustrialSlider s;
    s.setRange(0.0, 100.0);
    QSignalSpy limits(&s, &IndustrialSlider::setpointLimitsChanged);
    for (const auto &bad : {std::make_pair(kNaN, 50.0), std::make_pair(10.0, kInf), std::make_pair(60.0, 60.0),
                            std::make_pair(70.0, 20.0)}) {
        s.setSetpointLimits(bad.first, bad.second);
    }
    QCOMPARE(limits.count(), 0);
    QCOMPARE(s.setpointLimitLow(), 0.0);
    QCOMPARE(s.setpointLimitHigh(), 100.0);

    s.setSetpointLimits(-20.0, 80.0);
    QCOMPARE(s.setpointLimitLow(), 0.0);
    QCOMPARE(s.setpointLimitHigh(), 80.0);
    QCOMPARE(limits.count(), 1);
    QCOMPARE(limits.last().at(0).toDouble(), 0.0);
    QCOMPARE(limits.last().at(1).toDouble(), 80.0);
    s.setSetpointLimits(-10.0, 80.0); // Read the same
    QCOMPARE(limits.count(), 1);

    s.setRange(-50.0, 60.0);
    QCOMPARE(s.setpointLimitLow(), -10.0);
    QCOMPARE(s.setpointLimitHigh(), 60.0);
    QCOMPARE(limits.count(), 2);
    s.setRange(-50.0, 90.0);
    QCOMPARE(s.setpointLimitHigh(), 80.0);
    QCOMPARE(limits.count(), 3);
    s.setRange(-60.0, 90.0); // The limits read do not move
    QCOMPARE(limits.count(), 3);

    s.clearSetpointLimits();
    QCOMPARE(s.setpointLimitLow(), -60.0);
    QCOMPARE(s.setpointLimitHigh(), 90.0);
    QCOMPARE(limits.count(), 4);
    s.clearSetpointLimits();
    QCOMPARE(limits.count(), 4);
    s.setRange(0.0, 10.0); // Without limits the limits are the range
    QCOMPARE(limits.count(), 5);
    QCOMPARE(limits.last().at(1).toDouble(), 10.0);
}

// #182: the limits bind the keys, Home/End and the drag; setSetpoint() only the range; the setpoint is not moved
void tst_IndustrialSliderInput::limitsBindTheOperatorOnly()
{
    IndustrialSlider s;
    showSlider(s, false, 50.0);
    s.setSetpointLimits(20.0, 70.0);
    QCOMPARE(s.setpoint(), 50.0);
    s.setPageStep(15.0);
    for (int i = 0; i < 3; ++i) {
        QTest::keyClick(&s, Qt::Key_PageUp);
    }
    QCOMPARE(s.setpoint(), 70.0);
    QTest::keyClick(&s, Qt::Key_Home);
    QCOMPARE(s.setpoint(), 20.0);
    QTest::keyClick(&s, Qt::Key_Down);
    QCOMPARE(s.setpoint(), 20.0);
    QTest::keyClick(&s, Qt::Key_End);
    QCOMPARE(s.setpoint(), 70.0);

    s.setSetpoint(50.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    const QPointF h = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h);
    sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.4));
    QCOMPARE(s.setpoint(), 70.0);
    sendMouse(s, QEvent::MouseMove, moved(l, h, -length * 0.4));
    QCOMPARE(s.setpoint(), 20.0);
    sendMouse(s, QEvent::MouseButtonRelease, moved(l, h, -length * 0.4));

    s.setSetpoint(95.0);
    QCOMPARE(s.setpoint(), 95.0);
    s.setSetpoint(5.0);
    QCOMPARE(s.setpoint(), 5.0);
}

// #182: from a setpoint outside the limits (set by the application), input towards them is applied and input that
// would take it further out is ignored
void tst_IndustrialSliderInput::fromOutsideTheLimitsOnlyTowardsThem()
{
    IndustrialSlider s;
    showSlider(s, false, 90.0);
    s.setSetpointLimits(20.0, 70.0);
    QSignalSpy committed(&s, &IndustrialSlider::setpointCommitted);
    QTest::keyClick(&s, Qt::Key_Up);
    QTest::keyClick(&s, Qt::Key_End);
    QTest::keyClick(&s, Qt::Key_PageUp);
    QCOMPARE(s.setpoint(), 90.0);
    QCOMPARE(committed.count(), 0);
    QTest::keyClick(&s, Qt::Key_Down);
    QCOMPARE(s.setpoint(), 89.0);
    QCOMPARE(committed.count(), 1);
    QTest::keyClick(&s, Qt::Key_Home);
    QCOMPARE(s.setpoint(), 20.0);

    s.setSetpoint(10.0);
    const internal::SliderLayout l = layoutOf(s);
    const double length = std::abs(l.scaleMax - l.scaleMin);
    const QPointF h = handlePoint(s);
    sendMouse(s, QEvent::MouseButtonPress, h);
    sendMouse(s, QEvent::MouseMove, moved(l, h, -length * 0.05));
    QCOMPARE(s.setpoint(), 10.0);
    sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.05));
    QVERIFY(std::abs(s.setpoint() - 15.0) < 1e-9);
    sendMouse(s, QEvent::MouseMove, moved(l, h, length * 0.8));
    QCOMPARE(s.setpoint(), 70.0);
    sendMouse(s, QEvent::MouseButtonRelease, moved(l, h, length * 0.8));
}

void tst_IndustrialSliderInput::limitsAreHatchedWithStopMarks_data()
{
    addOrientations();
}

// #182: outside the limits the band of the ticks is hatched in scaleColor(), inside it is not; each limit inside the
// range has a stop mark from the zone band to past the end of a major tick. Without limits, none of it
void tst_IndustrialSliderInput::limitsAreHatchedWithStopMarks()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    showSlider(s, vertical, 50.0);
    s.setMajorTicks(10);
    s.setMinorTicks(0);
    const QImage without = render(s);
    s.setSetpointLimits(20.0, 70.0);
    const QImage with = render(s);
    const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
    const double c0 = l.tickNear;
    const double c1 = l.tickNear + internal::kSliderMajorTick;
    const auto scale = [](QRgb p) { return near(p, kScale, 110); };
    const auto rect = [&](double v0, double v1, double a0, double a1) {
        const double p0 = alongOf(l, s, v0);
        const double p1 = alongOf(l, s, v1);
        const double lo = std::min(p0, p1);
        const double hi = std::max(p0, p1);
        return vertical ? QRectF(a0, lo, a1 - a0, hi - lo) : QRectF(lo, a0, hi - lo, a1 - a0);
    };
    // Between two major ticks (at 0, 10, 20, ...), away from them: scale ink only from the hatching
    for (double from : {0.0, 10.0, 70.0, 80.0, 90.0}) {
        const QRectF gap = rect(from + 2.5, from + 7.5, c0 + 0.5, c1 - 0.5);
        const int ink = PixelTest::countIn(with, gap, scale);
        const int area = static_cast<int>(gap.width() * gap.height() * with.devicePixelRatio() * with.devicePixelRatio());
        QVERIFY2(ink * 6 >= area, qPrintable(QStringLiteral("%1-%2: %3 of %4 px hatched").arg(from + 2.5).arg(from + 7.5).arg(ink).arg(area)));
        QCOMPARE(PixelTest::countIn(without, gap, scale), 0);
    }
    for (double from : {20.0, 30.0, 40.0, 50.0, 60.0}) {
        const QRectF gap = rect(from + 2.5, from + 7.5, c0 - 0.5, c1 + 3.0);
        QVERIFY2(PixelTest::countIn(with, gap, scale) == 0, qPrintable(QStringLiteral("hatch inside the limits at %1").arg(from)));
    }
    // The stop marks: scale ink at the limit from the zone band to past the end of a major tick, absent without
    for (double limit : {20.0, 70.0}) {
        const double a = alongOf(l, s, limit);
        for (double across = l.zoneNear + 0.5; across < c1 + 1.5; across += 1.0) {
            const QPointF p = point(l, a, across);
            const double dpr = with.devicePixelRatio();
            const QRgb px = with.pixel(static_cast<int>(p.x() * dpr), static_cast<int>(p.y() * dpr));
            QVERIFY2(scale(px), qPrintable(QStringLiteral("limit %1, across %2: rgb(%3, %4, %5)")
                                               .arg(limit).arg(across).arg(qRed(px)).arg(qGreen(px)).arg(qBlue(px))));
        }
        // Past the square cap of a major tick (0.8 px), up to the end of the mark: the device pixels wholly inside
        // are all scale ink with the limits, none without
        const QRectF past = vertical ? QRectF(c1 + 1.0, a - 1.0, 1.5, 2.0) : QRectF(a - 1.0, c1 + 1.0, 2.0, 1.5);
        const auto whollyInside = [&](const QImage &img, const std::function<bool(QRgb)> &match) {
            const double dpr = img.devicePixelRatio();
            int all = 0;
            int matching = 0;
            for (int y = static_cast<int>(std::ceil(past.top() * dpr)); y + 1 <= past.bottom() * dpr; ++y) {
                for (int x = static_cast<int>(std::ceil(past.left() * dpr)); x + 1 <= past.right() * dpr; ++x) {
                    ++all;
                    matching += match(img.pixel(x, y)) ? 1 : 0;
                }
            }
            return std::make_pair(all, matching);
        };
        const auto [all, marked] = whollyInside(with, [](QRgb p) { return near(p, kScale, 24); });
        QVERIFY2(all > 0 && marked == all,
                 qPrintable(QStringLiteral("limit %1: %2 of %3 pixels past the tick marked").arg(limit).arg(marked).arg(all)));
        QCOMPARE(whollyInside(without, scale).second, 0);
    }
}

void tst_IndustrialSliderInput::focusRingIsASolidRingInTextColor_data()
{
    addOrientations();
}

// #183, #180: with the focus a solid 2 px ring in textColor() runs along the edge, inside the 4 px margin: every
// pixel the pen covers in full is in textColor(), on all four sides, and nothing changes outside the margin
void tst_IndustrialSliderInput::focusRingIsASolidRingInTextColor()
{
    QFETCH(bool, vertical);
    QWidget window;
    auto *s = new IndustrialSlider(&window);
    auto *other = new QLineEdit(&window);
    setVertical(*s, vertical);
    prepare(*s, sizeFor(vertical));
    s->setRange(0.0, 100.0);
    s->setSetpoint(30.0);
    s->setFeedback(40.0);
    other->setGeometry(0, s->height() + 10, 100, 30);
    window.resize(s->width(), s->height() + 50);
    QVERIFY(focusOn(*other));
    const QImage plain = render(*s);
    QVERIFY(internal::IndustrialSliderTestAccess::lastFocusRing(*s).isEmpty());
    QVERIFY(focusOn(*s));
    const QImage focused = render(*s);
    QCOMPARE(internal::IndustrialSliderTestAccess::lastFocusRing(*s),
             QRectF(1.0, 1.0, s->width() - 2.0, s->height() - 2.0));

    const double dpr = focused.devicePixelRatio();
    const double w = s->width();
    const double h = s->height();
    const double m = internal::kSliderMargin;
    int changed = 0;
    for (int y = 0; y < focused.height(); ++y) {
        for (int x = 0; x < focused.width(); ++x) {
            if (focused.pixel(x, y) == plain.pixel(x, y)) {
                continue;
            }
            ++changed;
            const double x0 = x / dpr;
            const double y0 = y / dpr;
            const double x1 = (x + 1) / dpr;
            const double y1 = (y + 1) / dpr;
            QVERIFY2(x0 < m || y0 < m || x1 > w - m || y1 > h - m,
                     qPrintable(QStringLiteral("pixel (%1, %2) changed inside the margin").arg(x).arg(y)));
        }
    }
    QVERIFY2(changed > 0, "no focus indicator");
    // The pixels covered in full by the pen, the band 0-2 px along each edge
    int perSide[4] = {0, 0, 0, 0};
    int off = 0;
    QString first;
    for (int y = 0; y < focused.height(); ++y) {
        for (int x = 0; x < focused.width(); ++x) {
            const double x0 = x / dpr;
            const double y0 = y / dpr;
            const double x1 = (x + 1) / dpr;
            const double y1 = (y + 1) / dpr;
            const bool inside = x1 <= w && y1 <= h;
            const int side = !inside ? -1
                             : x1 <= 2.0 ? 0
                             : x0 >= w - 2.0 ? 1
                             : y1 <= 2.0 ? 2
                             : y0 >= h - 2.0 ? 3
                                             : -1;
            if (side < 0) {
                continue;
            }
            QVERIFY2(channelDistance(plain.pixel(x, y), kText.rgb()) > 200,
                     qPrintable(QStringLiteral("at (%1, %2) the background is too close to textColor").arg(x).arg(y)));
            ++perSide[side];
            if (channelDistance(focused.pixel(x, y), kText.rgb()) > 48) {
                if (off == 0) {
                    const QRgb c = focused.pixel(x, y);
                    first = QStringLiteral("(%1, %2): rgb(%3, %4, %5)").arg(x).arg(y).arg(qRed(c)).arg(qGreen(c)).arg(qBlue(c));
                }
                ++off;
            }
        }
    }
    for (int side = 0; side < 4; ++side) {
        QVERIFY2(perSide[side] > 20, qPrintable(QStringLiteral("side %1: %2 pixels checked").arg(side).arg(perSide[side])));
    }
    QVERIFY2(off == 0, qPrintable(QStringLiteral("%1 pixels of the ring not in textColor, first %2").arg(off).arg(first)));
}

QTEST_MAIN(tst_IndustrialSliderInput)
#include "tst_IndustrialSliderInput.moc"
