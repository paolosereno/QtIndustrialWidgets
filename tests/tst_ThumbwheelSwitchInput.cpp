// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #154, part 2: ThumbwheelSwitch as a control, under the rules of #182. A press arms a button and its release makes
// one step; keys move the current decade, step it and type its digit; the wheel is off by default and acts only with
// focus, on the decade under the pointer; OnConfirm keeps a pending value until Enter or SET, Immediate commits at the
// end of each action; read-only passes the input on and shows a padlock; in overflow every input that would change the
// value is rejected; the focus ring and the bar of the current decade. Run at QT_SCALE_FACTOR 1, 1.25, 1.5 and 2, with
// and without installed fonts.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/ThumbwheelSwitch.h>

#include "internal/ThumbwheelSwitchLayout.h"
#include "internal/ThumbwheelSwitchTestAccess.h"

#include "PixelTestUtils.h"

#include <QtGui/QImage>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QLineEdit>

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

using namespace QtIndustrialWidgets;
using Mode = ThumbwheelSwitch::ApplyMode;

class tst_ThumbwheelSwitchInput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void pressAloneChangesNothing();
    void clickMakesOneStepAtTheRelease_data();
    void clickMakesOneStepAtTheRelease();
    void rejectedClickChangesNothing_data();
    void rejectedClickChangesNothing();
    void enterAppliesEscapeDiscards();
    void enterAndEscapeWithoutAPendingValueReachTheParent_data();
    void enterAndEscapeWithoutAPendingValueReachTheParent();
    void setButtonApplies();
    void applyAndDiscardSlots();
    void pendingBackToTheValueIsNotPending();
    void setValueKeepsThePendingValue();
    void lossOfFocusNeverApplies();
    void configurationChangeDiscardsThePendingValue_data();
    void configurationChangeDiscardsThePendingValue();
    void overflowRejectsEveryOperatorInput_data();
    void overflowRejectsEveryOperatorInput();
    void keysMoveTheCurrentDecade();
    void upAndDownStepTheCurrentDecade();
    void digitsTypeTheCurrentDecadeAndMoveOn();
    void signKeysActOnTheSignDrumOnly();
    void pageKeysReachTheParent();
    void immediateCommitsOnceAtTheKeyRelease();
    void immediateKeyThatChangesNothingDoesNotCommit();
    void immediateActionEnds_data();
    void immediateActionEnds();
    void setValueNeverCommits();
    void readOnlyIgnoresTheOperatorInput();
    void readOnlyDrawsThePadlock_data();
    void readOnlyDrawsThePadlock();
    void wheelIsOffByDefault();
    void wheelStepsTheDecadeUnderThePointerWithFocus_data();
    void wheelStepsTheDecadeUnderThePointerWithFocus();
    void wheelOutsideTheDrumsReachesTheParent();
    void wheelCountsHalfNotches();
    void pendingValueIsDrawnWithADashedOutline();
    void setButtonIsFilledWhileAValueIsPending();
    void statusLineShowsTheAppliedValue();
    void focusDrawsTheRingAndTheBarOfTheCurrentDecade();
    void minimumSizeHoldsTheInputParts();
    void everythingInsideWithAPendingValue_data();
    void everythingInsideWithAPendingValue();
};

namespace {

using internal::ThumbwheelSwitchTestAccess;

const QColor kDrum(0, 0, 112);
const QColor kDigit(255, 255, 255);
const QColor kButton(0, 112, 0);
const QColor kText(255, 255, 0);
const QColor kPending(255, 0, 255);
const QColor kBackground(0, 0, 0);

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

void colour(ThumbwheelSwitch &w)
{
    w.setDrumColor(kDrum);
    w.setDigitColor(kDigit);
    w.setButtonColor(kButton);
    w.setTextColor(kText);
    w.setPendingColor(kPending);
    QPalette pal = w.palette();
    pal.setColor(QPalette::Window, kBackground);
    w.setPalette(pal);
    w.setAutoFillBackground(true);
}

QImage render(ThumbwheelSwitch &w)
{
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

internal::ThumbwheelLayout layoutOf(ThumbwheelSwitch &w)
{
    (void)render(w);
    return ThumbwheelSwitchTestAccess::lastLayout(w);
}

QRgb at(const QImage &img, const QPointF &p)
{
    const double dpr = img.devicePixelRatio();
    return img.pixel(std::clamp(static_cast<int>(std::floor(p.x() * dpr)), 0, img.width() - 1),
                     std::clamp(static_cast<int>(std::floor(p.y() * dpr)), 0, img.height() - 1));
}

int countNear(const QImage &img, const QRectF &r, const QColor &c, int tolerance = 8)
{
    return PixelTest::countIn(img, r, [&](QRgb p) { return near(p, c, tolerance); });
}

QString flags(const QVector<bool> &v)
{
    QString s;
    for (bool b : v) {
        s += b ? QLatin1Char('1') : QLatin1Char('0');
    }
    return s;
}

// A parent that takes the events the switch passes on
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

// A window with a switch (5 drums, 1 decimal, unit V, limits 0 to 2500, value 125.0) and a line edit to take the focus
struct Rig {
    RecordingParent window;
    ThumbwheelSwitch *wheel{nullptr};
    QLineEdit *other{nullptr};

    explicit Rig(Mode mode = Mode::OnConfirm)
    {
        wheel = new ThumbwheelSwitch(&window);
        other = new QLineEdit(&window);
        wheel->setApplyMode(mode);
        wheel->setDigitCount(5);
        wheel->setDecimals(1);
        wheel->setUnit(QStringLiteral("V"));
        wheel->setRange(0.0, 2500.0);
        wheel->setValue(125.0);
        colour(*wheel);
        wheel->resize(wheel->minimumSizeHint());
        other->move(0, wheel->height() + 10);
        window.resize(std::max(wheel->width(), 200), wheel->height() + 60);
    }

    bool show()
    {
        window.show();
        window.activateWindow();
        if (!QTest::qWaitForWindowActive(&window)) {
            return false;
        }
        wheel->setFocus(Qt::OtherFocusReason);
        return QTest::qWaitFor([&] { return wheel->hasFocus(); });
    }

    internal::ThumbwheelLayout layout() { return layoutOf(*wheel); }

    void click(const QRectF &r) { QTest::mouseClick(wheel, Qt::LeftButton, Qt::NoModifier, r.center().toPoint()); }
    void press(const QRectF &r) { QTest::mousePress(wheel, Qt::LeftButton, Qt::NoModifier, r.center().toPoint()); }
    void release(const QPointF &p) { QTest::mouseRelease(wheel, Qt::LeftButton, Qt::NoModifier, p.toPoint()); }
    void key(int k) { QTest::keyClick(wheel, static_cast<Qt::Key>(k)); }
};

void sendKey(QWidget &w, QEvent::Type type, int key, bool autoRepeat)
{
    QKeyEvent event(type, key, Qt::NoModifier, QString(), autoRepeat);
    QCoreApplication::sendEvent(&w, &event);
}

// One wheel event of \a delta units (120 = one notch) at \a p; returns whether the switch accepted it
bool sendWheel(QWidget &w, const QPointF &p, int delta)
{
    QWheelEvent wheel(p, w.mapToGlobal(p), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                      false);
    QCoreApplication::sendEvent(&w, &wheel);
    return wheel.isAccepted();
}

// The signals of a switch
struct Spies {
    QSignalSpy changed;
    QSignalSpy committed;
    QSignalSpy pending;

    explicit Spies(ThumbwheelSwitch *w)
        : changed(w, &ThumbwheelSwitch::valueChanged)
        , committed(w, &ThumbwheelSwitch::valueCommitted)
        , pending(w, &ThumbwheelSwitch::pendingValueChanged)
    {
    }
};

} // namespace

void tst_ThumbwheelSwitchInput::defaults()
{
    ThumbwheelSwitch w;
    QCOMPARE(w.applyMode(), Mode::OnConfirm);
    QVERIFY(!w.isReadOnly());
    QVERIFY(!w.isWheelEnabled());
    QVERIFY(!w.hasPendingValue());
    QCOMPARE(w.pendingValue(), w.value());
    QCOMPARE(w.focusPolicy(), Qt::StrongFocus);
    QCOMPARE(w.pendingColor(), QColor(255, 176, 0));
}

// #182: a press never changes anything; the release elsewhere than on the pressed button does nothing either
void tst_ThumbwheelSwitchInput::pressAloneChangesNothing()
{
    for (const Mode mode : {Mode::OnConfirm, Mode::Immediate}) {
        Rig rig(mode);
        QVERIFY(rig.show());
        const internal::ThumbwheelLayout l = rig.layout();
        Spies spies(rig.wheel);
        rig.press(l.plus.at(3));
        QCOMPARE(rig.wheel->value(), 125.0);
        QVERIFY(!rig.wheel->hasPendingValue());
        QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 3); // The pressed decade is the current one
        rig.release(l.minus.at(3).center()); // Another button
        rig.press(l.plus.at(3));
        rig.release(l.drums.at(3).center()); // Its drum
        rig.press(l.plus.at(3));
        rig.release(QPointF(1.0, 1.0)); // Nothing
        QCOMPARE(rig.wheel->value(), 125.0);
        QVERIFY(!rig.wheel->hasPendingValue());
        QCOMPARE(spies.changed.count(), 0);
        QCOMPARE(spies.committed.count(), 0);
        QCOMPARE(spies.pending.count(), 0);
        rig.click(l.drums.at(1)); // A click on a drum: its decade, no change
        QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 1);
        QCOMPARE(spies.changed.count() + spies.pending.count(), 0);
    }
}

void tst_ThumbwheelSwitchInput::clickMakesOneStepAtTheRelease_data()
{
    QTest::addColumn<bool>("immediate");
    QTest::newRow("OnConfirm") << false;
    QTest::newRow("Immediate") << true;
}

void tst_ThumbwheelSwitchInput::clickMakesOneStepAtTheRelease()
{
    QFETCH(bool, immediate);
    Rig rig(immediate ? Mode::Immediate : Mode::OnConfirm);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    rig.click(l.plus.at(2)); // Units: 125.0 → 135.0
    rig.click(l.minus.at(4)); // Tenths at 0: rejected
    if (immediate) {
        QCOMPARE(rig.wheel->value(), 135.0);
        QVERIFY(!rig.wheel->hasPendingValue());
        QCOMPARE(spies.changed.count(), 1);
        QCOMPARE(spies.committed.count(), 1);
        QCOMPARE(spies.committed.at(0).at(0).toDouble(), 135.0);
    } else {
        QCOMPARE(rig.wheel->value(), 125.0);
        QVERIFY(rig.wheel->hasPendingValue());
        QCOMPARE(rig.wheel->pendingValue(), 135.0);
        QCOMPARE(spies.changed.count(), 0);
        QCOMPARE(spies.committed.count(), 0);
        QCOMPARE(spies.pending.count(), 1);
        QCOMPARE(spies.pending.at(0).at(0).toDouble(), 135.0);
    }
    (void)render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(*rig.wheel), (QStringList{"0", "1", "3", "5", "0"}));
}

void tst_ThumbwheelSwitchInput::rejectedClickChangesNothing_data()
{
    clickMakesOneStepAtTheRelease_data();
}

// A step drawn with an outlined arrow is rejected: a digit at 9 or 0, a step out of the limits
void tst_ThumbwheelSwitchInput::rejectedClickChangesNothing()
{
    QFETCH(bool, immediate);
    Rig rig(immediate ? Mode::Immediate : Mode::OnConfirm);
    QVERIFY(rig.show());
    rig.wheel->setValue(2499.9); // + of the thousands and hundreds beyond 2500, the others at 9
    const internal::ThumbwheelLayout l = rig.layout();
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastPlusOpen(*rig.wheel)), QStringLiteral("00000"));
    Spies spies(rig.wheel);
    for (int i = 0; i < l.drums.size(); ++i) {
        rig.click(l.plus.at(i));
    }
    QCOMPARE(rig.wheel->value(), 2499.9);
    rig.wheel->setValue(0.0);
    spies.changed.clear();
    spies.pending.clear();
    for (int i = 0; i < l.drums.size(); ++i) {
        rig.click(l.minus.at(i)); // Every digit at 0
    }
    QCOMPARE(rig.wheel->value(), 0.0);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.changed.count() + spies.committed.count() + spies.pending.count(), 0);
}

void tst_ThumbwheelSwitchInput::enterAppliesEscapeDiscards()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    rig.click(l.plus.at(2));
    rig.click(l.plus.at(2)); // Pending 145.0
    QCOMPARE(rig.wheel->pendingValue(), 145.0);
    int parentKeys = rig.window.keyPresses;
    rig.key(Qt::Key_Return);
    QCOMPARE(rig.window.keyPresses, parentKeys); // Taken by the switch
    QCOMPARE(rig.wheel->value(), 145.0);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.changed.count(), 1);
    QCOMPARE(spies.committed.count(), 1);
    QCOMPARE(spies.committed.at(0).at(0).toDouble(), 145.0);
    rig.click(l.minus.at(3)); // Pending 144.0; Enter of the keypad applies it too
    rig.key(Qt::Key_Enter);
    QCOMPARE(rig.window.keyPresses, parentKeys);
    QCOMPARE(rig.wheel->value(), 144.0);
    QCOMPARE(spies.committed.count(), 2);
    QCOMPARE(spies.committed.at(1).at(0).toDouble(), 144.0);
    rig.click(l.plus.at(1)); // Pending 244.0
    parentKeys = rig.window.keyPresses;
    rig.key(Qt::Key_Escape);
    QCOMPARE(rig.window.keyPresses, parentKeys);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 144.0);
    QCOMPARE(spies.committed.count(), 2);
    QCOMPARE(spies.pending.last().at(0).toDouble(), 144.0);
}

void tst_ThumbwheelSwitchInput::enterAndEscapeWithoutAPendingValueReachTheParent_data()
{
    clickMakesOneStepAtTheRelease_data();
}

// Without a pending value Enter and Escape are for the parent (the default button, the closing of a dialog)
void tst_ThumbwheelSwitchInput::enterAndEscapeWithoutAPendingValueReachTheParent()
{
    QFETCH(bool, immediate);
    Rig rig(immediate ? Mode::Immediate : Mode::OnConfirm);
    QVERIFY(rig.show());
    Spies spies(rig.wheel);
    for (const Qt::Key k : {Qt::Key_Return, Qt::Key_Enter, Qt::Key_Escape}) {
        const int before = rig.window.keyPresses;
        rig.key(k);
        QCOMPARE(rig.window.keyPresses, before + 1);
    }
    QCOMPARE(spies.changed.count() + spies.committed.count(), 0);
}

void tst_ThumbwheelSwitchInput::setButtonApplies()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    QVERIFY(!l.setButton.isEmpty());
    Spies spies(rig.wheel);
    rig.click(l.setButton); // Nothing pending
    QCOMPARE(spies.committed.count(), 0);
    rig.click(l.plus.at(0)); // Pending 1125.0
    rig.press(l.setButton);
    rig.release(l.tag.center()); // Released elsewhere: nothing
    QVERIFY(rig.wheel->hasPendingValue());
    QCOMPARE(spies.committed.count(), 0);
    rig.click(l.setButton);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 1125.0);
    QCOMPARE(spies.changed.count(), 1);
    QCOMPARE(spies.committed.count(), 1);
    QCOMPARE(spies.committed.at(0).at(0).toDouble(), 1125.0);
    // Immediate has no SET button
    rig.wheel->setApplyMode(Mode::Immediate);
    QVERIFY(rig.layout().setButton.isEmpty());
    QVERIFY(!ThumbwheelSwitchTestAccess::lastSetDrawn(*rig.wheel));
}

void tst_ThumbwheelSwitchInput::applyAndDiscardSlots()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    rig.wheel->applyPending(); // Nothing pending: nothing
    rig.wheel->discardPending();
    QCOMPARE(spies.changed.count() + spies.committed.count() + spies.pending.count(), 0);
    rig.click(l.minus.at(2)); // Pending 115.0
    rig.wheel->discardPending();
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 125.0);
    QCOMPARE(spies.committed.count(), 0);
    rig.click(l.minus.at(2));
    rig.wheel->applyPending();
    QCOMPARE(rig.wheel->value(), 115.0);
    QCOMPARE(spies.committed.count(), 1);
    QCOMPARE(spies.changed.count(), 1);
}

// #154 decision: a value is pending only while it differs from value()
void tst_ThumbwheelSwitchInput::pendingBackToTheValueIsNotPending()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    rig.click(l.plus.at(3));
    QVERIFY(rig.wheel->hasPendingValue());
    rig.click(l.minus.at(3)); // Back to 125.0
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.pending.count(), 2);
    QCOMPARE(spies.pending.last().at(0).toDouble(), 125.0);
    rig.key(Qt::Key_Return); // Nothing to apply: to the parent
    QCOMPARE(spies.committed.count(), 0);
    // setValue() reaching the pending value ends it too
    rig.click(l.plus.at(3)); // Pending 126.0
    rig.wheel->setValue(126.0);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 126.0);
    // Nothing pending: pendingValue() follows value(), with its signal
    const int before = spies.pending.count();
    rig.wheel->setValue(300.0);
    QCOMPARE(rig.wheel->pendingValue(), 300.0);
    QCOMPARE(spies.pending.count(), before + 1);
    QCOMPARE(spies.committed.count(), 0);
}

// setValue() changes the applied value and leaves a pending value as it is; both are visible
void tst_ThumbwheelSwitchInput::setValueKeepsThePendingValue()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    rig.click(l.plus.at(1)); // Pending 225.0
    Spies spies(rig.wheel);
    rig.wheel->setValue(1999.9);
    QCOMPARE(rig.wheel->value(), 1999.9);
    QCOMPARE(rig.wheel->pendingValue(), 225.0);
    QCOMPARE(spies.changed.count(), 1);
    QCOMPARE(spies.pending.count(), 0);
    QCOMPARE(spies.committed.count(), 0);
    (void)render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(*rig.wheel), (QStringList{"0", "2", "2", "5", "0"}));
    QCOMPARE(ThumbwheelSwitchTestAccess::lastStatus(*rig.wheel), QStringLiteral("SET?  now 1999.9 V"));
}

void tst_ThumbwheelSwitchInput::lossOfFocusNeverApplies()
{
    Rig rig;
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    rig.click(l.plus.at(2));
    rig.other->setFocus(Qt::TabFocusReason);
    QVERIFY(QTest::qWaitFor([&] { return rig.other->hasFocus(); }));
    QVERIFY(rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 125.0);
    rig.wheel->hide();
    rig.wheel->show();
    QVERIFY(rig.wheel->hasPendingValue());
    QCOMPARE(spies.changed.count() + spies.committed.count(), 0);
}

void tst_ThumbwheelSwitchInput::configurationChangeDiscardsThePendingValue_data()
{
    QTest::addColumn<int>("change");
    QTest::newRow("digitCount") << 0;
    QTest::newRow("decimals") << 1;
    QTest::newRow("sign drum") << 2;
    QTest::newRow("applyMode") << 3;
    QTest::newRow("readOnly") << 4;
}

void tst_ThumbwheelSwitchInput::configurationChangeDiscardsThePendingValue()
{
    QFETCH(int, change);
    Rig rig;
    QVERIFY(rig.show());
    rig.click(rig.layout().plus.at(2));
    QVERIFY(rig.wheel->hasPendingValue());
    Spies spies(rig.wheel);
    switch (change) {
    case 0: rig.wheel->setDigitCount(6); break;
    case 1: rig.wheel->setDecimals(2); break;
    case 2: rig.wheel->setSignDrumVisible(true); break;
    case 3: rig.wheel->setApplyMode(Mode::Immediate); break;
    default: rig.wheel->setReadOnly(true); break;
    }
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->pendingValue(), rig.wheel->value());
    QCOMPARE(spies.pending.count(), 1);
    QCOMPARE(spies.committed.count(), 0);
    QCOMPARE(rig.wheel->value(), 125.0);
}

void tst_ThumbwheelSwitchInput::overflowRejectsEveryOperatorInput_data()
{
    clickMakesOneStepAtTheRelease_data();
}

// #154 decision: while the overflow mark is shown every operator input that would change the value is rejected (the
// steps, the digits, the sign, the wheel, Enter), until setValue() gives a value the drums can show. Turning the mark
// on discards a pending value
void tst_ThumbwheelSwitchInput::overflowRejectsEveryOperatorInput()
{
    QFETCH(bool, immediate);
    Rig rig(immediate ? Mode::Immediate : Mode::OnConfirm);
    rig.wheel->setSignDrumVisible(true);
    rig.wheel->setRange(-2500.0, 2500.0);
    rig.wheel->setWheelEnabled(true);
    rig.wheel->resize(rig.wheel->minimumSizeHint());
    QVERIFY(rig.show());
    internal::ThumbwheelLayout l = rig.layout();
    if (!immediate) {
        rig.click(l.plus.at(3)); // Pending 135.0
        QVERIFY(rig.wheel->hasPendingValue());
    }
    Spies spies(rig.wheel);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown")));
    rig.wheel->setValue(123456.0);
    QVERIFY(rig.wheel->isOverflow());
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.pending.count(), immediate ? 0 : 1);
    l = rig.layout();
    const int pendingSignals = spies.pending.count();
    for (int i = 0; i < l.drums.size(); ++i) {
        rig.click(l.plus.at(i));
        rig.click(l.minus.at(i));
        QVERIFY(sendWheel(*rig.wheel, l.drums.at(i).center(), 120)); // Taken, and rejected
    }
    rig.key(Qt::Key_Home); // The sign drum: moving is still possible
    QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 0);
    rig.key(Qt::Key_Minus);
    rig.key(Qt::Key_Down);
    rig.key(Qt::Key_Right);
    QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 1);
    for (const Qt::Key k : {Qt::Key_Up, Qt::Key_Down, Qt::Key_5, Qt::Key_0}) {
        rig.key(k);
    }
    rig.wheel->applyPending();
    QVERIFY(rig.wheel->isOverflow());
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->value(), 125.0);
    QCOMPARE(spies.changed.count(), 0);
    QCOMPARE(spies.committed.count(), 0);
    QCOMPARE(spies.pending.count(), pendingSignals);
    (void)render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(*rig.wheel), QStringList(QVector<QString>(6, QStringLiteral("-")).toList()));

    // A value the drums show ends it: input works again
    rig.wheel->setValue(125.0);
    QVERIFY(!rig.wheel->isOverflow());
    rig.key(Qt::Key_Up); // The thousands, current since Right
    QCOMPARE(rig.wheel->pendingValue(), 1125.0);
}

// Left and Right move the current decade (the sign drum first), Home and End to the first and the last drum (#154: not
// the value to the limits); none of them changes the value, and they do not wrap around
void tst_ThumbwheelSwitchInput::keysMoveTheCurrentDecade()
{
    Rig rig;
    rig.wheel->setSignDrumVisible(true); // 6 drums
    rig.wheel->resize(rig.wheel->minimumSizeHint());
    QVERIFY(rig.show());
    Spies spies(rig.wheel);
    const auto current = [&] { return ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel); };
    QCOMPARE(current(), 0);
    rig.key(Qt::Key_Left);
    QCOMPARE(current(), 0);
    rig.key(Qt::Key_Right);
    rig.key(Qt::Key_Right);
    QCOMPARE(current(), 2);
    rig.key(Qt::Key_End);
    QCOMPARE(current(), 5);
    rig.key(Qt::Key_Right);
    QCOMPARE(current(), 5);
    rig.key(Qt::Key_Home);
    QCOMPARE(current(), 0);
    (void)render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastCursor(*rig.wheel), 0);
    QCOMPARE(rig.wheel->value(), 125.0);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.changed.count() + spies.pending.count() + spies.committed.count(), 0);
}

void tst_ThumbwheelSwitchInput::upAndDownStepTheCurrentDecade()
{
    Rig rig;
    QVERIFY(rig.show());
    rig.key(Qt::Key_Right); // Hundreds
    rig.key(Qt::Key_Up);
    rig.key(Qt::Key_Up);
    QCOMPARE(rig.wheel->pendingValue(), 325.0);
    rig.key(Qt::Key_End); // Tenths at 0
    rig.key(Qt::Key_Down); // Rejected: no wrap-around
    QCOMPARE(rig.wheel->pendingValue(), 325.0);
    rig.key(Qt::Key_Home); // Thousands
    rig.key(Qt::Key_Up);
    rig.key(Qt::Key_Up);
    rig.key(Qt::Key_Up); // 3325 > 2500: rejected, not rounded to the limit
    QCOMPARE(rig.wheel->pendingValue(), 2325.0);
    QCOMPARE(rig.wheel->value(), 125.0);
}

// 0–9 type the digit of the current decade and move to the next one; a digit that would leave the limits is rejected
// and the decade stays; the same digit only moves on
void tst_ThumbwheelSwitchInput::digitsTypeTheCurrentDecadeAndMoveOn()
{
    Rig rig;
    QVERIFY(rig.show());
    const auto current = [&] { return ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel); };
    for (const Qt::Key k : {Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5}) {
        rig.key(k);
    }
    QCOMPARE(rig.wheel->pendingValue(), 1234.5);
    QCOMPARE(current(), 4); // The last decade stays current
    rig.key(Qt::Key_7);
    QCOMPARE(rig.wheel->pendingValue(), 1234.7);
    rig.key(Qt::Key_Home);
    rig.key(Qt::Key_3); // 3234.7 > 2500: rejected
    QCOMPARE(rig.wheel->pendingValue(), 1234.7);
    QCOMPARE(current(), 0);
    rig.key(Qt::Key_1); // The same digit: on to the next decade
    QCOMPARE(current(), 1);
    QCOMPARE(rig.wheel->pendingValue(), 1234.7);
    QCOMPARE(rig.wheel->value(), 125.0);
}

// On the sign drum + and − (and Up and Down) select the sign; elsewhere they do nothing; digits do nothing on it
void tst_ThumbwheelSwitchInput::signKeysActOnTheSignDrumOnly()
{
    Rig rig(Mode::Immediate);
    rig.wheel->setSignDrumVisible(true);
    rig.wheel->setRange(-50.0, 50.0);
    rig.wheel->setValue(12.0);
    rig.wheel->resize(rig.wheel->minimumSizeHint());
    QVERIFY(rig.show());
    rig.key(Qt::Key_Minus);
    QCOMPARE(rig.wheel->value(), -12.0);
    rig.key(Qt::Key_Minus); // Already −
    rig.key(Qt::Key_5); // Not a digit drum
    QCOMPARE(rig.wheel->value(), -12.0);
    rig.key(Qt::Key_Up);
    QCOMPARE(rig.wheel->value(), 12.0);
    rig.key(Qt::Key_Down);
    QCOMPARE(rig.wheel->value(), -12.0);
    rig.key(Qt::Key_Plus);
    QCOMPARE(rig.wheel->value(), 12.0);
    rig.key(Qt::Key_End);
    rig.key(Qt::Key_Minus); // A digit drum: nothing
    QCOMPARE(rig.wheel->value(), 12.0);
    rig.key(Qt::Key_Plus);
    QCOMPARE(rig.wheel->value(), 12.0);
}

void tst_ThumbwheelSwitchInput::pageKeysReachTheParent()
{
    Rig rig;
    QVERIFY(rig.show());
    for (const Qt::Key k : {Qt::Key_PageUp, Qt::Key_PageDown}) {
        const int before = rig.window.keyPresses;
        rig.key(k);
        QCOMPARE(rig.window.keyPresses, before + 1);
    }
    QVERIFY(!rig.wheel->hasPendingValue());
}

// #182, Immediate: one commit at the release of a key held down, after every step it made; a typed digit commits at
// its release
void tst_ThumbwheelSwitchInput::immediateCommitsOnceAtTheKeyRelease()
{
    Rig rig(Mode::Immediate);
    QVERIFY(rig.show());
    rig.key(Qt::Key_Right); // Hundreds
    Spies spies(rig.wheel);
    sendKey(*rig.wheel, QEvent::KeyPress, Qt::Key_Up, false);
    sendKey(*rig.wheel, QEvent::KeyRelease, Qt::Key_Up, true);
    sendKey(*rig.wheel, QEvent::KeyPress, Qt::Key_Up, true);
    sendKey(*rig.wheel, QEvent::KeyRelease, Qt::Key_Up, true);
    sendKey(*rig.wheel, QEvent::KeyPress, Qt::Key_Up, true);
    QCOMPARE(rig.wheel->value(), 425.0);
    QCOMPARE(spies.changed.count(), 3);
    QCOMPARE(spies.committed.count(), 0);
    sendKey(*rig.wheel, QEvent::KeyRelease, Qt::Key_Up, false);
    QCOMPARE(spies.committed.count(), 1);
    QCOMPARE(spies.committed.at(0).at(0).toDouble(), 425.0);
    rig.key(Qt::Key_9); // The hundreds, still current: 925.0
    QCOMPARE(rig.wheel->value(), 925.0);
    QCOMPARE(spies.committed.count(), 2);
    QVERIFY(!rig.wheel->hasPendingValue());
}

void tst_ThumbwheelSwitchInput::immediateKeyThatChangesNothingDoesNotCommit()
{
    Rig rig(Mode::Immediate);
    QVERIFY(rig.show());
    Spies spies(rig.wheel);
    rig.key(Qt::Key_End);
    rig.key(Qt::Key_Down); // Tenths at 0
    rig.key(Qt::Key_0); // The same digit
    QCOMPARE(spies.changed.count(), 0);
    QCOMPARE(spies.committed.count(), 0);
}

void tst_ThumbwheelSwitchInput::immediateActionEnds_data()
{
    QTest::addColumn<int>("end");
    QTest::newRow("loss of focus") << 0;
    QTest::newRow("hide") << 1;
    QTest::newRow("read-only") << 2;
    QTest::newRow("a press") << 3;
    // A key sent to the switch itself (as QTest and automation tools do) while it has no focus: only the hide ends it
    QTest::newRow("hide, key sent without focus") << 4;
}

// #182: a key held down when the switch loses the focus, is hidden or becomes read-only commits there, once
void tst_ThumbwheelSwitchInput::immediateActionEnds()
{
    QFETCH(int, end);
    Rig rig(Mode::Immediate);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    rig.key(Qt::Key_Right);
    if (end == 4) {
        rig.other->setFocus(Qt::TabFocusReason);
        QVERIFY(QTest::qWaitFor([&] { return rig.other->hasFocus(); }));
    }
    Spies spies(rig.wheel);
    sendKey(*rig.wheel, QEvent::KeyPress, Qt::Key_Up, false);
    QCOMPARE(spies.committed.count(), 0);
    switch (end) {
    case 0:
        rig.other->setFocus(Qt::TabFocusReason);
        QVERIFY(QTest::qWaitFor([&] { return rig.other->hasFocus(); }));
        break;
    case 1: rig.wheel->hide(); break;
    case 2: rig.wheel->setReadOnly(true); break;
    case 3: rig.press(l.drums.at(0)); break;
    default: rig.wheel->hide(); break;
    }
    QCOMPARE(spies.committed.count(), 1);
    QCOMPARE(spies.committed.at(0).at(0).toDouble(), 225.0);
    sendKey(*rig.wheel, QEvent::KeyRelease, Qt::Key_Up, false); // The release: already committed
    QCOMPARE(spies.committed.count(), 1);
}

void tst_ThumbwheelSwitchInput::setValueNeverCommits()
{
    for (const Mode mode : {Mode::OnConfirm, Mode::Immediate}) {
        ThumbwheelSwitch w;
        w.setApplyMode(mode);
        QSignalSpy committed(&w, &ThumbwheelSwitch::valueCommitted);
        w.setValue(12.0);
        w.setValue(9000.0);
        QCOMPARE(committed.count(), 0);
    }
}

// #182: read-only, mouse, keys and wheel reach the parent and change nothing; setValue() still works; every arrow is
// outlined
void tst_ThumbwheelSwitchInput::readOnlyIgnoresTheOperatorInput()
{
    Rig rig;
    rig.wheel->setWheelEnabled(true);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    QSignalSpy readOnly(rig.wheel, &ThumbwheelSwitch::readOnlyChanged);
    rig.wheel->setReadOnly(true);
    rig.wheel->setReadOnly(true);
    QCOMPARE(readOnly.count(), 1);
    QVERIFY(rig.wheel->isReadOnly());
    Spies spies(rig.wheel);
    for (const Qt::Key k : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_Home, Qt::Key_End, Qt::Key_5,
                            Qt::Key_Plus, Qt::Key_Minus, Qt::Key_Return, Qt::Key_Escape}) {
        const int before = rig.window.keyPresses;
        rig.key(k);
        QVERIFY2(rig.window.keyPresses == before + 1,
                 qPrintable(QStringLiteral("key %1 did not reach the parent").arg(QKeySequence(k).toString())));
    }
    rig.click(l.plus.at(2));
    rig.click(l.setButton);
    QCOMPARE(rig.window.mousePresses, 2);
    QVERIFY(!sendWheel(*rig.wheel, l.drums.at(2).center(), 120));
    QCOMPARE(rig.wheel->value(), 125.0);
    QVERIFY(!rig.wheel->hasPendingValue());
    QCOMPARE(spies.changed.count() + spies.committed.count() + spies.pending.count(), 0);
    QVERIFY(rig.wheel->hasFocus());
    (void)render(*rig.wheel);
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastPlusOpen(*rig.wheel)), QStringLiteral("00000"));
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastMinusOpen(*rig.wheel)), QStringLiteral("00000"));
    rig.wheel->setValue(200.0);
    QCOMPARE(rig.wheel->value(), 200.0);
    rig.wheel->setReadOnly(false);
    rig.key(Qt::Key_Up);
    QVERIFY(rig.wheel->hasPendingValue());
}

void tst_ThumbwheelSwitchInput::readOnlyDrawsThePadlock_data()
{
    clickMakesOneStepAtTheRelease_data();
}

// The padlock, a path in textColor() at the start of the status line; in Immediate the line is there only in read-only
void tst_ThumbwheelSwitchInput::readOnlyDrawsThePadlock()
{
    QFETCH(bool, immediate);
    ThumbwheelSwitch w;
    w.setApplyMode(immediate ? Mode::Immediate : Mode::OnConfirm);
    colour(w);
    const QSize editable = w.minimumSizeHint();
    w.setReadOnly(true);
    const QSize locked = w.minimumSizeHint();
    if (immediate) {
        QVERIFY2(locked.height() > editable.height(), "no room for the padlock in Immediate");
    } else {
        QCOMPARE(locked, editable);
    }
    w.resize(locked);
    const QImage img = render(w);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
    QVERIFY(ThumbwheelSwitchTestAccess::lastPadlock(w));
    QVERIFY(QRectF(0.0, 0.0, locked.width(), locked.height()).contains(l.padlock));
    const int ink = countNear(img, l.padlock, kText, 60);
    QVERIFY2(ink * 1.0 >= l.padlock.width() * l.padlock.height() * img.devicePixelRatio() * img.devicePixelRatio() * 0.25,
             qPrintable(QStringLiteral("%1 padlock pixels").arg(ink)));
    w.setReadOnly(false);
    const QImage editableImg = render(w);
    QVERIFY(!ThumbwheelSwitchTestAccess::lastPadlock(w));
    QCOMPARE(countNear(editableImg, l.padlock, kText, 60), 0);
}

void tst_ThumbwheelSwitchInput::wheelIsOffByDefault()
{
    Rig rig(Mode::Immediate);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    QVERIFY(!sendWheel(*rig.wheel, l.drums.at(2).center(), 120)); // Not taken: Qt passes it to the parent
    QCOMPARE(rig.wheel->value(), 125.0);
}

void tst_ThumbwheelSwitchInput::wheelStepsTheDecadeUnderThePointerWithFocus_data()
{
    clickMakesOneStepAtTheRelease_data();
}

// #182: only with focus; on the decade under the pointer (its drum or its buttons), which becomes the current one; in
// Immediate each notch commits
void tst_ThumbwheelSwitchInput::wheelStepsTheDecadeUnderThePointerWithFocus()
{
    QFETCH(bool, immediate);
    Rig rig(immediate ? Mode::Immediate : Mode::OnConfirm);
    rig.wheel->setWheelEnabled(true);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    Spies spies(rig.wheel);
    QVERIFY(sendWheel(*rig.wheel, l.drums.at(1).center(), 240)); // Two notches on the hundreds
    QCOMPARE(rig.wheel->pendingValue(), 325.0);
    QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 1);
    QVERIFY(sendWheel(*rig.wheel, l.minus.at(3).center(), -120)); // The ones, through their − button
    QCOMPARE(rig.wheel->pendingValue(), 324.0);
    QCOMPARE(ThumbwheelSwitchTestAccess::currentDrum(*rig.wheel), 3);
    if (immediate) {
        QCOMPARE(rig.wheel->value(), 324.0);
        QCOMPARE(spies.committed.count(), 3);
    } else {
        QCOMPARE(rig.wheel->value(), 125.0);
        QCOMPARE(spies.committed.count(), 0);
    }
    // Without focus: to the parent
    rig.other->setFocus(Qt::TabFocusReason);
    QVERIFY(QTest::qWaitFor([&] { return rig.other->hasFocus(); }));
    QVERIFY(!sendWheel(*rig.wheel, l.drums.at(1).center(), 120));
    QCOMPARE(rig.wheel->pendingValue(), 324.0);
}

void tst_ThumbwheelSwitchInput::wheelOutsideTheDrumsReachesTheParent()
{
    Rig rig;
    rig.wheel->setWheelEnabled(true);
    QVERIFY(rig.show());
    const internal::ThumbwheelLayout l = rig.layout();
    for (const QPointF &p : {l.unit.center(), l.tag.center(), l.setButton.center(), QPointF(1.0, 1.0)}) {
        QVERIFY(!sendWheel(*rig.wheel, p, 120));
    }
    QVERIFY(!rig.wheel->hasPendingValue());
}

// #182: a notch is 120 units, summed over the smaller events of a high-resolution wheel
void tst_ThumbwheelSwitchInput::wheelCountsHalfNotches()
{
    Rig rig;
    rig.wheel->setWheelEnabled(true);
    QVERIFY(rig.show());
    const QPointF p = rig.layout().drums.at(2).center();
    QVERIFY(sendWheel(*rig.wheel, p, 60));
    QVERIFY(!rig.wheel->hasPendingValue());
    QVERIFY(sendWheel(*rig.wheel, p, 60));
    QCOMPARE(rig.wheel->pendingValue(), 135.0);
}

// The pending value does not rely on colour: a dashed outline on every drum, in pendingColor()
void tst_ThumbwheelSwitchInput::pendingValueIsDrawnWithADashedOutline()
{
    Rig rig;
    QVERIFY(rig.show());
    QImage img = render(*rig.wheel);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(*rig.wheel);
    for (const QRectF &drum : l.drums) {
        QCOMPARE(countNear(img, drum, kPending, 60), 0);
    }
    rig.click(l.plus.at(2));
    img = render(*rig.wheel);
    QVERIFY(ThumbwheelSwitchTestAccess::lastPendingOutline(*rig.wheel));
    for (const QRectF &drum : l.drums) {
        // Along the middle line of the top edge (1 px below the top: the 2 px pen covers every pixel there in full at
        // DPR 1 to 2), dashes of pendingColor with gaps of the drum face between them
        const double dpr = img.devicePixelRatio();
        const int y = static_cast<int>(std::floor((drum.top() + 1.0) * dpr));
        int dash = 0;
        int gap = 0;
        for (int x = static_cast<int>(std::ceil((drum.left() + 3.0) * dpr)); x < (drum.right() - 3.0) * dpr; ++x) {
            dash += near(img.pixel(x, y), kPending, 8) ? 1 : 0;
            gap += near(img.pixel(x, y), kDrum, 8) ? 1 : 0;
        }
        QVERIFY2(dash > 0 && gap > 0, qPrintable(QStringLiteral("%1 dash and %2 gap pixels").arg(dash).arg(gap)));
        QVERIFY(countNear(img, QRectF(drum.left(), drum.top() + 3.0, 2.0, drum.height() - 6.0), kPending, 60) > 0);
        // Inside: the digit on the face, no outline
        QCOMPARE(countNear(img, drum.adjusted(4.0, 4.0, -4.0, -4.0), kPending, 60), 0);
    }
}

void tst_ThumbwheelSwitchInput::setButtonIsFilledWhileAValueIsPending()
{
    Rig rig;
    QVERIFY(rig.show());
    QImage img = render(*rig.wheel);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(*rig.wheel);
    const QPointF face(l.setButton.left() + 3.0, l.setButton.top() + 3.0); // Clear of the text and the corners
    QVERIFY(!ThumbwheelSwitchTestAccess::lastSetOpen(*rig.wheel));
    QVERIFY2(near(at(img, face), kBackground), qPrintable(PixelTest::rgb(at(img, face))));
    QVERIFY(countNear(img, l.setButton, kText, 60) > 0); // The outline and the text
    rig.click(l.plus.at(2));
    img = render(*rig.wheel);
    QVERIFY(ThumbwheelSwitchTestAccess::lastSetOpen(*rig.wheel));
    QVERIFY2(near(at(img, face), kButton), qPrintable(PixelTest::rgb(at(img, face))));
    QVERIFY(countNear(img, l.setButton, kDigit, 60) > 0);
    QCOMPARE(countNear(img, l.setButton, kText, 8), 0);
}

// The applied value stays visible while a value is pending: the status line, in textColor()
void tst_ThumbwheelSwitchInput::statusLineShowsTheAppliedValue()
{
    Rig rig;
    rig.wheel->setSignDrumVisible(true);
    rig.wheel->setRange(-2500.0, 2500.0);
    rig.wheel->setValue(-12.5);
    rig.wheel->resize(rig.wheel->minimumSizeHint());
    QVERIFY(rig.show());
    QImage img = render(*rig.wheel);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastStatus(*rig.wheel), QString());
    QCOMPARE(countNear(img, l.status, kText, 60), 0);
    rig.click(l.plus.at(3)); // −22.5
    img = render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastStatus(*rig.wheel), QStringLiteral("SET?  now −0012.5 V"));
    QVERIFY(countNear(img, l.status, kText, 60) > 0);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(*rig.wheel), (QStringList{"−", "0", "0", "2", "2", "5"}));
}

// #183: with focus, a solid 2 px ring in textColor() along the edge and a bar under the current decade; without, none
void tst_ThumbwheelSwitchInput::focusDrawsTheRingAndTheBarOfTheCurrentDecade()
{
    Rig rig;
    QVERIFY(rig.show());
    rig.key(Qt::Key_Right);
    rig.key(Qt::Key_Right);
    QImage img = render(*rig.wheel);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastCursor(*rig.wheel), 2);
    for (int i = 0; i < l.cursors.size(); ++i) {
        const int ink = countNear(img, l.cursors.at(i), kText);
        QVERIFY2((i == 2) == (ink > 0), qPrintable(QStringLiteral("bar %1: %2 pixels").arg(i).arg(ink)));
    }
    QVERIFY(near(at(img, l.cursors.at(2).center()), kText));
    const QRectF ring = ThumbwheelSwitchTestAccess::lastFocusRing(*rig.wheel);
    QCOMPARE(ring, QRectF(rig.wheel->rect()).adjusted(1.0, 1.0, -1.0, -1.0));
    const double w = rig.wheel->width();
    const double h = rig.wheel->height();
    // On the middle line of the band 0-2 px along each edge: a pixel there lies inside the band at DPR 1 to 2, also
    // where the edge of the widget falls inside a device pixel
    for (const QPointF &p : {QPointF(w * 0.5, 1.0), QPointF(w * 0.5, h - 1.0), QPointF(1.0, h * 0.5),
                             QPointF(w - 1.0, h * 0.5)}) {
        QVERIFY2(near(at(img, p), kText), qPrintable(PixelTest::rgb(at(img, p))));
    }
    rig.other->setFocus(Qt::TabFocusReason);
    QVERIFY(QTest::qWaitFor([&] { return rig.other->hasFocus(); }));
    img = render(*rig.wheel);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastCursor(*rig.wheel), -1);
    QCOMPARE(countNear(img, l.cursors.at(2), kText), 0);
    QVERIFY(near(at(img, QPointF(w * 0.5, 1.0)), kBackground));
}

// The SET button and the status line belong to OnConfirm; the bar of the current decade is always reserved; neither
// a pending value nor the focus changes the minimum
void tst_ThumbwheelSwitchInput::minimumSizeHoldsTheInputParts()
{
    Rig rig;
    const QSize confirm = rig.wheel->minimumSizeHint();
    rig.wheel->setApplyMode(Mode::Immediate);
    const QSize immediate = rig.wheel->minimumSizeHint();
    QVERIFY(immediate.height() < confirm.height());
    QVERIFY(immediate.width() < confirm.width());
    rig.wheel->setApplyMode(Mode::OnConfirm);
    rig.wheel->resize(confirm);
    QVERIFY(rig.show());
    rig.click(rig.layout().plus.at(2));
    QVERIFY(rig.wheel->hasPendingValue());
    QCOMPARE(rig.wheel->minimumSizeHint(), confirm);
    rig.wheel->setReadOnly(true);
    QCOMPARE(rig.wheel->minimumSizeHint(), confirm);
}

void tst_ThumbwheelSwitchInput::everythingInsideWithAPendingValue_data()
{
    QTest::addColumn<int>("digits");
    QTest::addColumn<int>("decimals");
    QTest::addColumn<bool>("sign");
    QTest::addColumn<QString>("unit");
    QTest::newRow("1 digit") << 1 << 0 << false << QString();
    QTest::newRow("5.1 V") << 5 << 1 << false << QStringLiteral("V");
    QTest::newRow("9 digits, sign, long unit") << 9 << 3 << true << QStringLiteral("mbar abs");
}

// #44: at the minimum size, with a pending value, the focus and the widest applied value, nothing is drawn outside the
// group and the status text stays in its line
void tst_ThumbwheelSwitchInput::everythingInsideWithAPendingValue()
{
    QFETCH(int, digits);
    QFETCH(int, decimals);
    QFETCH(bool, sign);
    QFETCH(QString, unit);
    Rig rig;
    rig.wheel->setDigitCount(digits);
    rig.wheel->setDecimals(decimals);
    rig.wheel->setSignDrumVisible(sign);
    rig.wheel->setUnit(unit);
    rig.wheel->setRange(-1e9, 1e9);
    // The applied value with every digit 8, the widest in most fonts
    double eights = 0.0;
    for (int i = 0; i < digits; ++i) {
        eights = eights * 10.0 + 8.0;
    }
    rig.wheel->setValue((sign ? -eights : eights) / std::pow(10.0, decimals));
    rig.wheel->resize(rig.wheel->minimumSizeHint());
    QVERIFY(rig.show());
    rig.key(Qt::Key_End);
    rig.key(Qt::Key_Down); // The last digit 8 → 7: pending
    QVERIFY(rig.wheel->hasPendingValue());
    const QImage img = render(*rig.wheel);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(*rig.wheel);
    QVERIFY(!ThumbwheelSwitchTestAccess::lastStatus(*rig.wheel).isEmpty());
    const QRectF group = l.group.adjusted(-1.0, -1.0, 1.0, 1.0);
    const QRectF ringBand = QRectF(rig.wheel->rect()).adjusted(2.0, 2.0, -2.0, -2.0); // Inside the focus ring
    const double dpr = img.devicePixelRatio();
    int outside = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QPointF p((x + 0.5) / dpr, (y + 0.5) / dpr);
            // The pixels the focus ring (the band 0-2 px along each edge) touches, even in part, are its own
            const QRectF area(x / dpr, y / dpr, 1.0 / dpr, 1.0 / dpr);
            const bool ring = area.left() < ringBand.left() || area.top() < ringBand.top()
                              || area.right() > ringBand.right() || area.bottom() > ringBand.bottom();
            if (!ring && !group.contains(p) && !near(img.pixel(x, y), kBackground, 0)) {
                ++outside;
            }
        }
    }
    QVERIFY2(outside == 0, qPrintable(QStringLiteral("%1 pixels drawn outside the group").arg(outside)));
    // The status text: its ink inside its line, none to the right of the group
    QVERIFY(countNear(img, l.status, kText, 60) > 0);
    const QRectF below(l.group.left(), l.status.bottom() + 1.0, l.group.width(), ringBand.bottom() - l.status.bottom() - 1.0);
    QCOMPARE(countNear(img, below, kText, 60), 0);
}

QTEST_MAIN(tst_ThumbwheelSwitchInput)
#include "tst_ThumbwheelSwitchInput.moc"
