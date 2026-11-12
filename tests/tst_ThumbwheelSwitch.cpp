// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #154, part 1 (display): the value model of ThumbwheelSwitch as the widget applies it (rounding, rejected input,
// the overflow mark, the limits), the open and blocked buttons, and what is drawn, at the pixel: digits, sign,
// decimal point, the dash of the overflow, the marks of the tag column, filled and outlined arrows. Run at
// QT_SCALE_FACTOR 1, 1.25, 1.5 and 2, with and without installed fonts.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/ThumbwheelSwitch.h>

#include "internal/FontScale.h"
#include "internal/ThumbwheelSwitchLayout.h"
#include "internal/ThumbwheelSwitchTestAccess.h"

#include "FontTestUtils.h"
#include "PixelTestUtils.h"
#include "ScreenChangeTestUtils.h"

#include <QtGui/QImage>
#include <QtTest/QSignalSpy>

#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_ThumbwheelSwitch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void valueIsRoundedToTheResolution();
    void nonFiniteValueIsRejected();
    void valueTheDrumsCannotShowIsRejectedWithOverflow_data();
    void valueTheDrumsCannotShowIsRejectedWithOverflow();
    void configurationChangesKeepTheValue();
    void limitsAreReadWithinTheDrums();
    void valueOutsideTheLimitsIsKeptAndMarked_data();
    void valueOutsideTheLimitsIsKeptAndMarked();
    void buttonsShowWhetherTheirStepIsApplied_data();
    void buttonsShowWhetherTheirStepIsApplied();
    void drumsShowTheDigits();
    void overflowDrawsADashOnEveryDrum();
    void signDrumDrawsPlusAndMinus();
    void arrowsAreFilledOrOutlined();
    void tagMarksAreDrawn();
    void decimalPointIsDrawn();
    void everythingInsideFromTheMinimumUp_data();
    void everythingInsideFromTheMinimumUp();
    void textFollowsTheWidgetFontSize();
    void digitsNeverBelowSevenPoints();
    void minimumSizeHintFollowsTheConfiguration();
    void screenChangeOfAVisibleSwitch_data();
    void screenChangeOfAVisibleSwitch();
};

namespace {

using internal::ThumbwheelSwitchTestAccess;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

const QColor kDrum(0, 0, 112);
const QColor kDigit(255, 255, 255);
const QColor kButton(0, 112, 0);
const QColor kText(255, 255, 0);
const QColor kBackground(0, 0, 0);

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

// The switch with the test colours on an opaque black background, at its minimum size (or \a size)
void prepare(ThumbwheelSwitch &w, QSize size = QSize())
{
    w.setDrumColor(kDrum);
    w.setDigitColor(kDigit);
    w.setButtonColor(kButton);
    w.setTextColor(kText);
    QPalette pal = w.palette();
    pal.setColor(QPalette::Window, kBackground);
    w.setPalette(pal);
    w.setAutoFillBackground(true);
    w.resize(size.isValid() ? size : w.minimumSizeHint());
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

// The device pixel at the logical point \a p
QRgb at(const QImage &img, const QPointF &p)
{
    const double dpr = img.devicePixelRatio();
    return img.pixel(std::clamp(static_cast<int>(std::floor(p.x() * dpr)), 0, img.width() - 1),
                     std::clamp(static_cast<int>(std::floor(p.y() * dpr)), 0, img.height() - 1));
}

QString flags(const QVector<bool> &v)
{
    QString s;
    for (bool b : v) {
        s += b ? QLatin1Char('1') : QLatin1Char('0');
    }
    return s;
}

int countNear(const QImage &img, const QRectF &r, const QColor &c, int tolerance = 8)
{
    return PixelTest::countIn(img, r, [&](QRgb p) { return near(p, c, tolerance); });
}

// The inside of a drum, clear of its outline and rounded corners
QRectF inside(const QRectF &r)
{
    return r.adjusted(2.0, 2.0, -2.0, -2.0);
}

} // namespace

void tst_ThumbwheelSwitch::defaults()
{
    ThumbwheelSwitch w;
    QCOMPARE(w.digitCount(), 4);
    QCOMPARE(w.decimals(), 0);
    QVERIFY(!w.isSignDrumVisible());
    QCOMPARE(w.minimum(), 0.0);
    QCOMPARE(w.maximum(), 9999.0);
    QCOMPARE(w.value(), 0.0);
    QVERIFY(!w.isOverflow());
    QCOMPARE(w.unit(), QString());
    QCOMPARE(w.sizeHint(), w.minimumSizeHint());
    prepare(w);
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w), (QStringList{"0", "0", "0", "0"}));
    QCOMPARE(ThumbwheelSwitchTestAccess::lastTag(w), QString());
}

void tst_ThumbwheelSwitch::valueIsRoundedToTheResolution()
{
    ThumbwheelSwitch w;
    w.setDigitCount(5);
    w.setDecimals(1);
    QSignalSpy changed(&w, &ThumbwheelSwitch::valueChanged);
    w.setValue(125.04);
    QCOMPARE(w.value(), 125.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.last().at(0).toDouble(), 125.0);
    w.setValue(125.0); // The same value: no signal
    w.setValue(124.96); // Rounds to the same value
    QCOMPARE(changed.count(), 1);
    w.setValue(12.25); // A half: away from zero
    QCOMPARE(w.value(), 12.3);
    QCOMPARE(changed.count(), 2);
    prepare(w);
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w), (QStringList{"0", "0", "1", "2", "3"}));
}

// #182: NaN and ±Inf are rejected with a warning; the value and the drums stay, and no overflow is shown
void tst_ThumbwheelSwitch::nonFiniteValueIsRejected()
{
    ThumbwheelSwitch w;
    w.setValue(42.0);
    QSignalSpy changed(&w, &ThumbwheelSwitch::valueChanged);
    QSignalSpy overflow(&w, &ThumbwheelSwitch::overflowChanged);
    for (double v : {kNaN, kInf, -kInf}) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("non-finite value .* ignored")));
        w.setValue(v);
    }
    QCOMPARE(w.value(), 42.0);
    QVERIFY(!w.isOverflow());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(overflow.count(), 0);
    prepare(w);
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w), (QStringList{"0", "0", "4", "2"}));
}

void tst_ThumbwheelSwitch::valueTheDrumsCannotShowIsRejectedWithOverflow_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<bool>("sign");
    QTest::newRow("five digits on four drums") << 12345.0 << false;
    QTest::newRow("just above the capacity") << 9999.5 << false; // Rounds to 10000
    QTest::newRow("negative without the sign drum") << -3.0 << false;
    QTest::newRow("negative beyond the capacity") << -10000.0 << true;
    QTest::newRow("beyond any count") << 1e300 << true;
}

// #154 decision: a value the drums cannot show is rejected with a warning, value() keeps the last accepted value, and
// the overflow mark (a dash on every drum, "OVF") stays until a value the drums can show arrives
void tst_ThumbwheelSwitch::valueTheDrumsCannotShowIsRejectedWithOverflow()
{
    QFETCH(double, input);
    QFETCH(bool, sign);
    ThumbwheelSwitch w;
    w.setSignDrumVisible(sign);
    w.setValue(77.0);
    QSignalSpy changed(&w, &ThumbwheelSwitch::valueChanged);
    QSignalSpy overflow(&w, &ThumbwheelSwitch::overflowChanged);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown on the drums, ignored")));
    w.setValue(input);
    QCOMPARE(w.value(), 77.0);
    QVERIFY(w.isOverflow());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(overflow.count(), 1);
    QCOMPARE(overflow.last().at(0).toBool(), true);
    prepare(w);
    (void)render(w);
    const int drums = 4 + (sign ? 1 : 0);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w), QStringList(QVector<QString>(drums, QStringLiteral("-")).toList()));
    QCOMPARE(ThumbwheelSwitchTestAccess::lastTag(w), QStringLiteral("OVF"));
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastPlusOpen(w)), QString(drums, QLatin1Char('0')));
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastMinusOpen(w)), QString(drums, QLatin1Char('0')));

    // A second value that cannot be shown: the mark stays, no new signal
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown on the drums, ignored")));
    w.setValue(input);
    QCOMPARE(overflow.count(), 1);
    // A value the drums show ends it, even the same value as before
    w.setValue(77.0);
    QVERIFY(!w.isOverflow());
    QCOMPARE(overflow.count(), 2);
    QCOMPARE(overflow.last().at(0).toBool(), false);
    QCOMPARE(changed.count(), 0);
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastTag(w), QString());
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w).last(), QStringLiteral("7"));
}

// value() is kept across changes of the drums: rounded to a new resolution (with valueChanged()), and shown with the
// overflow mark when the drums can no longer show it
void tst_ThumbwheelSwitch::configurationChangesKeepTheValue()
{
    ThumbwheelSwitch w;
    w.setDigitCount(5);
    w.setDecimals(2);
    w.setValue(123.45);
    QSignalSpy changed(&w, &ThumbwheelSwitch::valueChanged);
    QSignalSpy overflow(&w, &ThumbwheelSwitch::overflowChanged);

    w.setDecimals(1); // 123.45 at 0.1: 123.5 (away from zero)
    QCOMPARE(w.value(), 123.5);
    QCOMPARE(changed.count(), 1);
    QVERIFY(!w.isOverflow());

    w.setDigitCount(3); // 1235 counts on 3 drums: overflow; decimals stay 1
    QCOMPARE(w.decimals(), 1);
    QVERIFY(w.isOverflow());
    QCOMPARE(w.value(), 123.5);
    QCOMPARE(overflow.count(), 1);
    w.setDigitCount(4); // Shown again
    QVERIFY(!w.isOverflow());
    QCOMPARE(overflow.count(), 2);

    w.setDigitCount(1); // decimals reduced to 0: the value rounded to 124, then 1 drum cannot show it
    QCOMPARE(w.decimals(), 0);
    QCOMPARE(w.value(), 124.0);
    QVERIFY(w.isOverflow());
    w.setDigitCount(9);
    QVERIFY(!w.isOverflow());
    w.setDigitCount(42);
    QCOMPARE(w.digitCount(), 9);
    w.setDecimals(20);
    QCOMPARE(w.decimals(), 8);

    ThumbwheelSwitch s;
    s.setSignDrumVisible(true);
    s.setRange(-50.0, 50.0);
    s.setValue(-12.0);
    QSignalSpy signedOverflow(&s, &ThumbwheelSwitch::overflowChanged);
    s.setSignDrumVisible(false); // A negative value without the sign drum
    QVERIFY(s.isOverflow());
    QCOMPARE(s.value(), -12.0);
    s.setSignDrumVisible(true);
    QVERIFY(!s.isOverflow());
    QCOMPARE(signedOverflow.count(), 2);
}

void tst_ThumbwheelSwitch::limitsAreReadWithinTheDrums()
{
    ThumbwheelSwitch w;
    QSignalSpy range(&w, &ThumbwheelSwitch::rangeChanged);
    w.setRange(-20.0, 20000.0); // No sign drum, 4 drums: read 0 to 9999
    QCOMPARE(w.minimum(), 0.0);
    QCOMPARE(w.maximum(), 9999.0);
    QCOMPARE(range.count(), 0); // As read, they did not change
    w.setDigitCount(5); // The maximum as set fits now
    QCOMPARE(w.maximum(), 20000.0);
    QCOMPARE(range.count(), 1);
    w.setSignDrumVisible(true);
    QCOMPARE(w.minimum(), -20.0);
    QCOMPARE(range.count(), 2);
    QCOMPARE(range.last().at(0).toDouble(), -20.0);
    QCOMPARE(range.last().at(1).toDouble(), 20000.0);

    // Invalid limits are ignored
    for (const auto &[lo, hi] : {std::pair{kNaN, 5.0}, std::pair{0.0, kInf}, std::pair{5.0, 5.0}, std::pair{6.0, 5.0}}) {
        w.setRange(lo, hi);
    }
    QCOMPARE(w.minimum(), -20.0);
    QCOMPARE(w.maximum(), 20000.0);
    QCOMPARE(range.count(), 2);
    w.setMaximum(300.0);
    w.setMinimum(-10.0);
    QCOMPARE(w.minimum(), -10.0);
    QCOMPARE(w.maximum(), 300.0);
    QCOMPARE(range.count(), 4);
    QCOMPARE(w.value(), 0.0); // The limits never change the value
}

void tst_ThumbwheelSwitch::valueOutsideTheLimitsIsKeptAndMarked_data()
{
    QTest::addColumn<double>("value");
    QTest::addColumn<QString>("tag");
    QTest::newRow("above") << 1820.0 << QStringLiteral("▲");
    QTest::newRow("at the maximum") << 1500.0 << QString();
    QTest::newRow("inside") << 700.0 << QString();
    QTest::newRow("at the minimum") << 100.0 << QString();
    QTest::newRow("below") << 40.0 << QStringLiteral("▼");
}

// #182 limits: setValue() is not bound by them; a value outside is kept, shown, and marked with a triangle
void tst_ThumbwheelSwitch::valueOutsideTheLimitsIsKeptAndMarked()
{
    QFETCH(double, value);
    QFETCH(QString, tag);
    ThumbwheelSwitch w;
    w.setRange(100.0, 1500.0);
    w.setValue(value);
    QCOMPARE(w.value(), value);
    QVERIFY(!w.isOverflow());
    prepare(w);
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastTag(w), tag);
}

void tst_ThumbwheelSwitch::buttonsShowWhetherTheirStepIsApplied_data()
{
    QTest::addColumn<int>("digits");
    QTest::addColumn<int>("decimals");
    QTest::addColumn<bool>("sign");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<double>("value");
    QTest::addColumn<QString>("plus");  // Per drum, 1: the + step would be applied
    QTest::addColumn<QString>("minus"); // The same for −

    QTest::newRow("0000") << 4 << 0 << false << 0.0 << 9999.0 << 0.0 << "1111" << "0000";
    QTest::newRow("9999") << 4 << 0 << false << 0.0 << 9999.0 << 9999.0 << "0000" << "1111";
    QTest::newRow("0909: 9 and 0 saturate") << 4 << 0 << false << 0.0 << 9999.0 << 909.0 << "1010" << "0101";
    // Limits 0 to 1500 at 1450: thousands + (2450), hundreds + (1550) rejected; tens + (1460) applied
    QTest::newRow("1450 below 1500") << 4 << 0 << false << 0.0 << 1500.0 << 1450.0 << "0011" << "1110";
    // At 1500: every + rejected
    QTest::newRow("1500 at the maximum") << 4 << 0 << false << 0.0 << 1500.0 << 1500.0 << "0000" << "1100";
    // 1820 above the maximum 1500: only steps towards it; 0820 inside, 1720 closer, 1810 closer; + all further out
    QTest::newRow("1820 above") << 4 << 0 << false << 0.0 << 1500.0 << 1820.0 << "0000" << "1110";
    // 40 below the minimum 100: + all closer (1040 and 140 inside, 50 and 41 closer); − further out (30) or at 0
    QTest::newRow("0040 below") << 4 << 0 << false << 100.0 << 1500.0 << 40.0 << "1111" << "0000";
    // Sign drum first. −12.50 in −50..50: to + applied, − already negative; magnitude up (−22.50) still inside
    QTest::newRow("-12.50") << 4 << 2 << true << -50.0 << 50.0 << -12.5 << "11111" << "01110";
    QTest::newRow("-45.00: tens + would be -55") << 4 << 2 << true << -50.0 << 50.0 << -45.0 << "10111" << "01100";
    QTest::newRow("0: no sign change") << 4 << 2 << true << -50.0 << 50.0 << 0.0 << "01111" << "00000";
    // +30 in −20..50: to − would be −30, outside
    QTest::newRow("30: sign change out of the limits") << 4 << 0 << true << -20.0 << 50.0 << 30.0 << "00011" << "00010";
}

void tst_ThumbwheelSwitch::buttonsShowWhetherTheirStepIsApplied()
{
    QFETCH(int, digits);
    QFETCH(int, decimals);
    QFETCH(bool, sign);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(double, value);
    QFETCH(QString, plus);
    QFETCH(QString, minus);
    ThumbwheelSwitch w;
    w.setDigitCount(digits);
    w.setDecimals(decimals);
    w.setSignDrumVisible(sign);
    w.setRange(minimum, maximum);
    w.setValue(value);
    prepare(w);
    (void)render(w);
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastPlusOpen(w)), plus);
    QCOMPARE(flags(ThumbwheelSwitchTestAccess::lastMinusOpen(w)), minus);
}

// Each digit drum shows its digit in digitColor on the drum colour
void tst_ThumbwheelSwitch::drumsShowTheDigits()
{
    ThumbwheelSwitch w;
    w.setValue(1250.0);
    prepare(w);
    const QImage img = render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w), (QStringList{"1", "2", "5", "0"}));
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
    for (const QRectF &drum : l.drums) {
        const QRectF in = inside(drum);
        QVERIFY2(countNear(img, in, kDigit, 40) > 0, "no digit ink in a drum");
        QVERIFY2(countNear(img, in, kDrum) > countNear(img, in, kDigit, 40), "the drum face is not drawn");
        // Nothing but the face, the digit and their blend inside the drum
        const int foreign = PixelTest::countIn(img, in, [](QRgb p) {
            return qRed(p) != qGreen(p) || qBlue(p) < qRed(p); // Blends of (0,0,112) and white keep r == g <= b
        });
        QCOMPARE(foreign, 0);
    }
}

// The overflow: a horizontal bar across the middle of each drum, no digit above or below it
void tst_ThumbwheelSwitch::overflowDrawsADashOnEveryDrum()
{
    ThumbwheelSwitch w;
    w.setValue(8888.0); // Every drum has ink above and below the middle while it shows 8
    prepare(w);
    QImage img = render(w);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
    const auto upperAndLower = [&](const QImage &image) {
        int n = 0;
        for (const QRectF &drum : l.drums) {
            const QRectF in = inside(drum);
            const double h = in.height();
            n += countNear(image, QRectF(in.left(), in.top(), in.width(), h * 0.3), kDigit, 40);
            n += countNear(image, QRectF(in.left(), in.bottom() - h * 0.3, in.width(), h * 0.3), kDigit, 40);
        }
        return n;
    };
    QVERIFY2(upperAndLower(img) > 0, "the digits 8 draw nothing away from the middle");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown")));
    w.setValue(123456.0);
    img = render(w);
    QCOMPARE(upperAndLower(img), 0);
    for (const QRectF &drum : l.drums) {
        QVERIFY2(near(at(img, drum.center()), kDigit, 40), qPrintable(PixelTest::rgb(at(img, drum.center()))));
        // The bar is half as wide as the drum: the face at a quarter from each side, at mid height
        QVERIFY(near(at(img, QPointF(drum.left() + drum.width() * 0.15, drum.center().y())), kDrum));
        QVERIFY(near(at(img, QPointF(drum.right() - drum.width() * 0.15, drum.center().y())), kDrum));
    }
}

// The sign as shapes: + has a vertical bar through the middle, − only the horizontal one
void tst_ThumbwheelSwitch::signDrumDrawsPlusAndMinus()
{
    ThumbwheelSwitch w;
    w.setSignDrumVisible(true);
    w.setRange(-100.0, 100.0);
    w.setValue(25.0);
    prepare(w);
    QImage img = render(w);
    const QRectF drum = ThumbwheelSwitchTestAccess::lastLayout(w).drums.first();
    const QPointF centre = drum.center();
    const double reach = std::round(drum.width() * 0.5) * 0.5 - 2.0; // Inside the arms of the sign
    const QPointF above(centre.x(), centre.y() - reach);
    const QPointF beside(centre.x() - reach, centre.y());
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w).first(), QStringLiteral("+"));
    QVERIFY2(near(at(img, centre), kDigit, 40), qPrintable(PixelTest::rgb(at(img, centre))));
    QVERIFY2(near(at(img, above), kDigit, 40), qPrintable(PixelTest::rgb(at(img, above))));
    QVERIFY2(near(at(img, beside), kDigit, 40), qPrintable(PixelTest::rgb(at(img, beside))));
    w.setValue(-25.0);
    img = render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w).first(), QStringLiteral("−"));
    QVERIFY(near(at(img, beside), kDigit, 40));
    QVERIFY2(near(at(img, above), kDrum), qPrintable(PixelTest::rgb(at(img, above))));
    w.setValue(0.0); // 0 is +
    (void)render(w);
    QCOMPARE(ThumbwheelSwitchTestAccess::lastDrums(w).first(), QStringLiteral("+"));
}

// An open step: a filled arrow (its middle in digitColor); a rejected one: an outline (its middle the button face)
void tst_ThumbwheelSwitch::arrowsAreFilledOrOutlined()
{
    ThumbwheelSwitch w;
    w.setValue(909.0); // + open on 0 and 0, closed on 9 and 9
    prepare(w);
    const QImage img = render(w);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
    const QVector<bool> plus = ThumbwheelSwitchTestAccess::lastPlusOpen(w);
    const QVector<bool> minus = ThumbwheelSwitchTestAccess::lastMinusOpen(w);
    QCOMPARE(flags(plus), QStringLiteral("1010"));
    for (int i = 0; i < l.drums.size(); ++i) {
        for (const auto &[button, open] : {std::pair{l.plus.at(i), plus.at(i)}, std::pair{l.minus.at(i), minus.at(i)}}) {
            const QRgb middle = at(img, button.center());
            QVERIFY2(near(middle, open ? kDigit : kButton, 40),
                     qPrintable(QStringLiteral("drum %1: %2").arg(i).arg(PixelTest::rgb(middle))));
            // Both kinds of arrow have ink: the outline is drawn
            QVERIFY(countNear(img, button, kDigit, 60) > 0);
        }
    }
}

// ▲ and ▼ as filled triangles in textColor, pointing up and down; "OVF" as text, also in textColor
void tst_ThumbwheelSwitch::tagMarksAreDrawn()
{
    ThumbwheelSwitch w;
    w.setRange(100.0, 1500.0);
    w.setValue(700.0);
    prepare(w);
    QImage img = render(w);
    const QRectF tag = ThumbwheelSwitchTestAccess::lastLayout(w).tag;
    QCOMPARE(countNear(img, tag, kText, 60), 0); // Nothing inside the limits
    for (const auto &[value, up] : {std::pair{1820.0, true}, std::pair{40.0, false}}) {
        w.setValue(value);
        img = render(w);
        // The ink of the triangle: wider at its base than at its tip
        const double dpr = img.devicePixelRatio();
        const QRect r = QRectF(tag.topLeft() * dpr, tag.size() * dpr).toAlignedRect();
        int top = -1;
        int bottom = -1;
        int topWidth = 0;
        int bottomWidth = 0;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            int n = 0;
            for (int x = r.left(); x <= r.right(); ++x) {
                n += near(img.pixel(x, y), kText, 8) ? 1 : 0;
            }
            if (n > 0) {
                if (top < 0) {
                    top = y;
                    topWidth = n;
                }
                bottom = y;
                bottomWidth = n;
            }
        }
        QVERIFY2(top >= 0, "no triangle drawn");
        QVERIFY2(bottom - top >= 3, "the triangle is too flat");
        QVERIFY2(up ? bottomWidth > topWidth : topWidth > bottomWidth,
                 qPrintable(QStringLiteral("widths %1 at the top, %2 at the bottom").arg(topWidth).arg(bottomWidth)));
    }
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown")));
    w.setValue(99999.0);
    img = render(w);
    QVERIFY2(countNear(img, tag, kText, 60) > 0, "no ink of the OVF tag");
}

void tst_ThumbwheelSwitch::decimalPointIsDrawn()
{
    ThumbwheelSwitch w;
    w.setDigitCount(5);
    w.setDecimals(2);
    prepare(w);
    const QImage img = render(w);
    const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
    QVERIFY(!l.decimalPoint.isEmpty());
    QVERIFY(l.decimalPoint.left() > l.drums.at(2).right());
    QVERIFY(l.decimalPoint.right() < l.drums.at(3).left());
    QVERIFY2(near(at(img, l.decimalPoint.center()), kText, 30), qPrintable(PixelTest::rgb(at(img, l.decimalPoint.center()))));
    w.setDecimals(0);
    QVERIFY(layoutOf(w).decimalPoint.isEmpty());
}

void tst_ThumbwheelSwitch::everythingInsideFromTheMinimumUp_data()
{
    QTest::addColumn<int>("digits");
    QTest::addColumn<int>("decimals");
    QTest::addColumn<bool>("sign");
    QTest::addColumn<QString>("unit");
    QTest::newRow("default") << 4 << 0 << false << QString();
    QTest::newRow("5.1 V") << 5 << 1 << false << QStringLiteral("V");
    QTest::newRow("sign, 2 decimals, °C") << 4 << 2 << true << QStringLiteral("°C");
    QTest::newRow("9 digits, long unit") << 9 << 3 << true << QStringLiteral("mbar abs");
    QTest::newRow("1 digit") << 1 << 0 << false << QString();
}

// #44: from minimumSizeHint() up, every part (and the ink of the texts) is inside the widget, and nothing is drawn
// outside the group
void tst_ThumbwheelSwitch::everythingInsideFromTheMinimumUp()
{
    QFETCH(int, digits);
    QFETCH(int, decimals);
    QFETCH(bool, sign);
    QFETCH(QString, unit);
    ThumbwheelSwitch w;
    w.setDigitCount(digits);
    w.setDecimals(decimals);
    w.setSignDrumVisible(sign);
    w.setUnit(unit);
    w.setRange(-1.0, 1.0);
    w.setValue(sign ? -2.0 : 2.0); // A tag is drawn (below or above the limits)
    const QSize min = w.minimumSizeHint();
    for (const QSize &size : {min, min + QSize(37, 23)}) {
        prepare(w, size);
        const QImage img = render(w);
        const internal::ThumbwheelLayout l = ThumbwheelSwitchTestAccess::lastLayout(w);
        const QRectF widget(0.0, 0.0, size.width(), size.height());
        QVERIFY(widget.contains(l.group));
        QVERIFY(!ThumbwheelSwitchTestAccess::lastTag(w).isEmpty());
        // No ink outside the group (with one pixel of antialiasing)
        const QRectF group = l.group.adjusted(-1.0, -1.0, 1.0, 1.0);
        const double dpr = img.devicePixelRatio();
        int outside = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (!group.contains(QPointF((x + 0.5) / dpr, (y + 0.5) / dpr)) && !near(img.pixel(x, y), kBackground, 0)) {
                    ++outside;
                }
            }
        }
        QVERIFY2(outside == 0, qPrintable(QStringLiteral("%1 pixels drawn outside the group at %2 x %3")
                                              .arg(outside).arg(size.width()).arg(size.height())));
        // The unit and the tag: their ink inside their places
        if (!unit.isEmpty()) {
            QVERIFY(countNear(img, l.unit, kText, 60) > 0);
            const QRectF right(l.unit.right() + 1.0, l.unit.top(), l.tag.left() - l.unit.right() - 2.0, l.unit.height());
            QCOMPARE(countNear(img, right, kText, 60), 0);
        }
    }
}

void tst_ThumbwheelSwitch::textFollowsTheWidgetFontSize()
{
    if (!(internal::effectivePixelSize(QApplication::font()) > 0.0)) {
        QFAIL("the application font has no size");
    }
    ThumbwheelSwitch base;
    prepare(base, QSize(400, 200));
    ThumbwheelSwitch grown;
    grown.setFont(FontTest::doubledFont());
    prepare(grown, QSize(400, 200));
    (void)render(base);
    (void)render(grown);
    const int a = ThumbwheelSwitchTestAccess::lastDigitPixelSize(base);
    const int b = ThumbwheelSwitchTestAccess::lastDigitPixelSize(grown);
    QVERIFY2(a > 0 && b >= 1.5 * a, qPrintable(QStringLiteral("digits %1 px, %2 px with a font twice as large").arg(a).arg(b)));
    QVERIFY(grown.minimumSizeHint().width() > base.minimumSizeHint().width());
    QVERIFY(grown.minimumSizeHint().height() > base.minimumSizeHint().height());
    QVERIFY(layoutOf(grown).drums.first().height() > layoutOf(base).drums.first().height());
}

// The texts never go below 7 pt, whatever the widget font
void tst_ThumbwheelSwitch::digitsNeverBelowSevenPoints()
{
    ThumbwheelSwitch w;
    QFont tiny = w.font();
    tiny.setPixelSize(2);
    w.setFont(tiny);
    prepare(w);
    (void)render(w);
    const double sevenPoints = 7.0 * w.logicalDpiY() / 72.0;
    QVERIFY2(ThumbwheelSwitchTestAccess::lastDigitPixelSize(w) >= std::floor(sevenPoints),
             qPrintable(QStringLiteral("%1 px").arg(ThumbwheelSwitchTestAccess::lastDigitPixelSize(w))));
}

// The minimum follows what it is made of
void tst_ThumbwheelSwitch::minimumSizeHintFollowsTheConfiguration()
{
    ThumbwheelSwitch w;
    QSize last = w.minimumSizeHint();
    const std::vector<std::pair<const char *, std::function<void()>>> wider = {
        {"one more drum", [&] { w.setDigitCount(5); }},
        {"a decimal point", [&] { w.setDecimals(1); }},
        {"the sign drum", [&] { w.setSignDrumVisible(true); }},
        {"a unit", [&] { w.setUnit(QStringLiteral("V")); }},
        {"a longer unit", [&] { w.setUnit(QStringLiteral("mV DC")); }},
    };
    for (const auto &[what, change] : wider) {
        change();
        const QSize now = w.minimumSizeHint();
        QVERIFY2(now.width() > last.width(), what);
        QCOMPARE(now.height(), last.height());
        last = now;
    }
    // The value and the limits change nothing of the size
    w.setRange(-5.0, 5.0);
    w.setValue(-3.2);
    QCOMPARE(w.minimumSizeHint(), last);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown")));
    w.setValue(1e9);
    QCOMPARE(w.minimumSizeHint(), last);
}

void tst_ThumbwheelSwitch::screenChangeOfAVisibleSwitch_data()
{
    ScreenChange::addScreens();
}

// #177: moved to a screen of another DPR or logical DPI, the switch draws and sizes itself as a new one there, and its
// layout gets the new minimum once
void tst_ThumbwheelSwitch::screenChangeOfAVisibleSwitch()
{
    QFETCH(double, dpr);
    QFETCH(int, dpi);
    const std::vector<std::pair<const char *, std::function<void(ThumbwheelSwitch &)>>> configurations = {
        {"default", [](ThumbwheelSwitch &) {}},
        {"5.1 V", [](ThumbwheelSwitch &w) { w.setDigitCount(5); w.setDecimals(1); w.setUnit(QStringLiteral("V")); w.setValue(125.0); }},
        {"signed, below the limits", [](ThumbwheelSwitch &w) {
             w.setSignDrumVisible(true);
             w.setRange(0.0, 10.0);
             w.setValue(-7.0);
         }},
    };
    for (const auto &configuration : configurations) {
        bool minimumChanged = false;
        ScreenChange::checkScreenChange<ThumbwheelSwitch>(configuration.second, dpr, dpi, &minimumChanged);
        if (QTest::currentTestFailed()) {
            qWarning("configuration: %s", configuration.first);
            return;
        }
    }
}

QTEST_MAIN(tst_ThumbwheelSwitch)
#include "tst_ThumbwheelSwitch.moc"
