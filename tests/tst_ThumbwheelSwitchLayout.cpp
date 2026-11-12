// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #154: the value model and the geometry of ThumbwheelSwitch, without rendering. Counts, digits, steps without
// wrap-around or carry, no -0, the limits of the operator's input; drums, buttons, decimal point, unit and tag column
// in order, on whole pixels, and minimumThumbwheelSize() the smallest size that keeps them inside.

#include <QtTest/QtTest>

#include "internal/ThumbwheelSwitchLayout.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

class tst_ThumbwheelSwitchLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsRoundHalvesAwayFromZero();
    void whatTheDrumsCanShow();
    void digitsAreTheMagnitude();
    void stepsSaturateWithoutCarry_data();
    void stepsSaturateWithoutCarry();
    void signHasNoMinusZero();
    void limitsAllowOnlyTowardsThem_data();
    void limitsAllowOnlyTowardsThem();
    void partsInOrder_data();
    void partsInOrder();
    void minimumSizeKeepsEveryPartInside_data();
    void minimumSizeKeepsEveryPartInside();
    void largerSizeCentresTheGroup();
    void nonFiniteAndTinyInputs();
};

namespace {

ThumbwheelFormat format(int digits, int decimals = 0, bool sign = false)
{
    ThumbwheelFormat f;
    f.digitCount = digits;
    f.decimals = decimals;
    f.signDrum = sign;
    return f;
}

ThumbwheelTextSizes typicalText()
{
    ThumbwheelTextSizes t;
    t.digitPx = 20.0;
    t.digitWidth = 11.2;
    t.digitHeight = 23.4;
    t.unitWidth = 18.6;
    t.tagWidth = 24.3;
    t.tagHeight = 14.1;
    return t;
}

ThumbwheelLayoutInput input(const ThumbwheelFormat &f, double w = 0.0, double h = 0.0, bool unit = true)
{
    ThumbwheelLayoutInput in;
    in.format = f;
    in.text = typicalText();
    if (!unit) {
        in.text.unitWidth = 0.0;
    }
    in.width = w;
    in.height = h;
    return in;
}

bool whole(double v)
{
    return std::abs(v - std::round(v)) < 1e-9;
}

bool onWholePixels(const QRectF &r)
{
    return whole(r.left()) && whole(r.top()) && whole(r.width()) && whole(r.height());
}

QString rectText(const QRectF &r)
{
    return QStringLiteral("(%1, %2, %3 x %4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

} // namespace

void tst_ThumbwheelSwitchLayout::countsRoundHalvesAwayFromZero()
{
    const ThumbwheelFormat f = format(5, 1);
    QCOMPARE(f.scale(), qint64(10));
    QCOMPARE(f.countOf(12.25).value(), qint64(123)); // 122.5: away from zero
    QCOMPARE(f.countOf(-12.25).value(), qint64(-123));
    QCOMPARE(f.countOf(12.24).value(), qint64(122));
    QCOMPARE(f.countOf(0.0).value(), qint64(0));
    QCOMPARE(f.valueOf(123), 12.3);
    QCOMPARE(f.valueOf(-5), -0.5);
    QVERIFY(!f.countOf(std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(!f.countOf(std::numeric_limits<double>::infinity()));
    QVERIFY(!f.countOf(-std::numeric_limits<double>::infinity()));
    QVERIFY(!f.countOf(1e20));
}

void tst_ThumbwheelSwitchLayout::whatTheDrumsCanShow()
{
    const ThumbwheelFormat unsignedFormat = format(4);
    QCOMPARE(unsignedFormat.capacity(), qint64(9999));
    QVERIFY(unsignedFormat.canShow(0));
    QVERIFY(unsignedFormat.canShow(9999));
    QVERIFY(!unsignedFormat.canShow(10000));
    QVERIFY(!unsignedFormat.canShow(-1)); // No sign drum
    const ThumbwheelFormat signedFormat = format(4, 0, true);
    QVERIFY(signedFormat.canShow(-1));
    QVERIFY(signedFormat.canShow(-9999));
    QVERIFY(!signedFormat.canShow(-10000));
    QCOMPARE(format(9).capacity(), qint64(999999999));
    QCOMPARE(format(1).capacity(), qint64(9));
}

void tst_ThumbwheelSwitchLayout::digitsAreTheMagnitude()
{
    const ThumbwheelFormat f = format(4, 0, true);
    const QList<int> expected{0, 1, 2, 5};
    for (qint64 count : {qint64(125), qint64(-125)}) {
        QList<int> digits;
        for (int decade = 0; decade < 4; ++decade) {
            digits << f.digit(count, decade);
        }
        QCOMPARE(digits, expected);
    }
}

void tst_ThumbwheelSwitchLayout::stepsSaturateWithoutCarry_data()
{
    QTest::addColumn<qint64>("count");
    QTest::addColumn<int>("decade");
    QTest::addColumn<int>("direction");
    QTest::addColumn<qint64>("expected"); // -1000000: rejected
    const qint64 none = -1000000;
    QTest::newRow("units up") << qint64(125) << 3 << 1 << qint64(126);
    QTest::newRow("hundreds up") << qint64(1099) << 1 << 1 << qint64(1199);
    QTest::newRow("tens at 9 up: no carry") << qint64(1099) << 2 << 1 << none;
    QTest::newRow("units at 9 up: no wrap") << qint64(9) << 3 << 1 << none;
    QTest::newRow("thousands at 9 up") << qint64(9000) << 0 << 1 << none;
    QTest::newRow("tens at 0 down: no borrow") << qint64(1005) << 2 << -1 << none;
    QTest::newRow("units at 0 down: no wrap") << qint64(0) << 3 << -1 << none;
    QTest::newRow("thousands down") << qint64(2000) << 0 << -1 << qint64(1000);
    QTest::newRow("negative: the digit up, the value down") << qint64(-125) << 3 << 1 << qint64(-126);
    QTest::newRow("negative: the digit down, the value up") << qint64(-125) << 3 << -1 << qint64(-124);
    QTest::newRow("negative to 0: no -0") << qint64(-1) << 3 << -1 << qint64(0);
    QTest::newRow("decade out of range") << qint64(125) << 4 << 1 << none;
}

void tst_ThumbwheelSwitchLayout::stepsSaturateWithoutCarry()
{
    QFETCH(qint64, count);
    QFETCH(int, decade);
    QFETCH(int, direction);
    QFETCH(qint64, expected);
    const std::optional<qint64> next = format(4, 0, true).stepDigit(count, decade, direction);
    if (expected == -1000000) {
        QVERIFY2(!next, qPrintable(QStringLiteral("stepped to %1").arg(next.value_or(0))));
    } else {
        QVERIFY(next);
        QCOMPARE(*next, expected);
    }
}

void tst_ThumbwheelSwitchLayout::signHasNoMinusZero()
{
    const ThumbwheelFormat f = format(4, 0, true);
    QVERIFY(!f.withSign(0, true));
    QVERIFY(!f.withSign(0, false));
    QCOMPARE(f.withSign(25, true).value(), qint64(-25));
    QCOMPARE(f.withSign(-25, false).value(), qint64(25));
    QCOMPARE(f.withSign(-25, true).value(), qint64(-25));
    QVERIFY(!format(4).withSign(25, true)); // No sign drum
}

void tst_ThumbwheelSwitchLayout::limitsAllowOnlyTowardsThem_data()
{
    QTest::addColumn<double>("from");
    QTest::addColumn<double>("to");
    QTest::addColumn<bool>("allowed");
    // Limits 100 to 500
    QTest::newRow("inside to inside") << 200.0 << 300.0 << true;
    QTest::newRow("inside to the limit") << 400.0 << 500.0 << true;
    QTest::newRow("inside to above") << 450.0 << 550.0 << false;
    QTest::newRow("inside to below") << 150.0 << 50.0 << false;
    QTest::newRow("above, closer") << 900.0 << 800.0 << true;
    QTest::newRow("above, further out") << 900.0 << 901.0 << false;
    QTest::newRow("above, into the limits") << 600.0 << 500.0 << true;
    QTest::newRow("below, closer") << 10.0 << 20.0 << true;
    QTest::newRow("below, further out") << 10.0 << 0.0 << false;
}

void tst_ThumbwheelSwitchLayout::limitsAllowOnlyTowardsThem()
{
    QFETCH(double, from);
    QFETCH(double, to);
    QFETCH(bool, allowed);
    const ThumbwheelLimits limits{100.0, 500.0};
    QCOMPARE(limits.allows(from, to), allowed);
    QCOMPARE(limits.stateOf(99.0), ThumbwheelLimitState::Below);
    QCOMPARE(limits.stateOf(100.0), ThumbwheelLimitState::Inside);
    QCOMPARE(limits.stateOf(500.0), ThumbwheelLimitState::Inside);
    QCOMPARE(limits.stateOf(500.5), ThumbwheelLimitState::Above);
}

void tst_ThumbwheelSwitchLayout::partsInOrder_data()
{
    QTest::addColumn<int>("digits");
    QTest::addColumn<int>("decimals");
    QTest::addColumn<bool>("sign");
    QTest::addColumn<bool>("unit");
    QTest::newRow("4 digits") << 4 << 0 << false << true;
    QTest::newRow("5 digits, 1 decimal") << 5 << 1 << false << true;
    QTest::newRow("4 digits, 2 decimals, sign") << 4 << 2 << true << true;
    QTest::newRow("1 digit, no unit") << 1 << 0 << false << false;
    QTest::newRow("9 digits, 8 decimals, sign") << 9 << 8 << true << true;
}

// Left to right: the drums with equal gaps (the gap of the decimal point wider, after the right drum), the unit, the
// tag column; each button above or below its drum, as wide; nothing overlapping
void tst_ThumbwheelSwitchLayout::partsInOrder()
{
    QFETCH(int, digits);
    QFETCH(int, decimals);
    QFETCH(bool, sign);
    QFETCH(bool, unit);
    const ThumbwheelLayoutInput in = input(format(digits, decimals, sign), 0.0, 0.0, unit);
    const QSizeF min = minimumThumbwheelSize(in);
    const ThumbwheelLayout l = layoutThumbwheel(input(format(digits, decimals, sign), min.width(), min.height(), unit));
    const int drums = digits + (sign ? 1 : 0);
    QCOMPARE(l.drums.size(), drums);
    QCOMPARE(l.plus.size(), drums);
    QCOMPARE(l.minus.size(), drums);
    QCOMPARE(l.signDrum, sign);
    QCOMPARE(l.drumOfDecade(0), sign ? 1 : 0);
    const int pointAfter = decimals > 0 ? l.drumOfDecade(digits - decimals - 1) : -1;
    for (int i = 0; i < drums; ++i) {
        const QRectF d = l.drums.at(i);
        QVERIFY2(onWholePixels(d), qPrintable(rectText(d)));
        QCOMPARE(d.size(), l.drums.first().size());
        QVERIFY(d.width() >= typicalText().digitWidth + 2.0 * thumbwheelDrumPadX(20.0));
        QVERIFY(d.height() >= typicalText().digitHeight + 2.0 * thumbwheelDrumPadY(20.0));
        const QRectF p = l.plus.at(i);
        const QRectF m = l.minus.at(i);
        QVERIFY2(onWholePixels(p) && onWholePixels(m), qPrintable(rectText(p) + rectText(m)));
        QCOMPARE(p.left(), d.left());
        QCOMPARE(p.width(), d.width());
        QCOMPARE(m.left(), d.left());
        QCOMPARE(m.width(), d.width());
        QCOMPARE(p.bottom() + kThumbwheelButtonGap, d.top());
        QCOMPARE(d.bottom() + kThumbwheelButtonGap, m.top());
        QVERIFY(p.height() >= kThumbwheelMinButton);
        QCOMPARE(m.height(), p.height());
        if (i + 1 < drums) {
            const double gap = l.drums.at(i + 1).left() - d.right();
            if (i == pointAfter) {
                QVERIFY2(gap > kThumbwheelDrumGap, qPrintable(QStringLiteral("gap of the point %1").arg(gap)));
                QVERIFY(l.decimalPoint.left() > d.right());
                QVERIFY(l.decimalPoint.right() < l.drums.at(i + 1).left());
                QVERIFY(l.decimalPoint.top() > d.center().y()); // At the foot of the digits
                QVERIFY(l.decimalPoint.bottom() <= d.bottom());
            } else {
                QCOMPARE(gap, kThumbwheelDrumGap);
            }
        }
    }
    QCOMPARE(l.decimalPoint.isEmpty(), decimals == 0);
    const double drumsRight = l.drums.last().right();
    if (unit) {
        QCOMPARE(l.unit.left(), drumsRight + kThumbwheelUnitGap);
        QVERIFY(l.unit.width() >= typicalText().unitWidth);
        QCOMPARE(l.tag.left(), l.unit.right() + kThumbwheelTagGap);
    } else {
        QVERIFY(l.unit.isEmpty());
        QCOMPARE(l.tag.left(), drumsRight + kThumbwheelTagGap);
    }
    QVERIFY(l.tag.width() >= typicalText().tagWidth);
    QCOMPARE(l.tag.top(), l.drums.first().top());
    QCOMPARE(l.tag.height(), l.drums.first().height());
}

void tst_ThumbwheelSwitchLayout::minimumSizeKeepsEveryPartInside_data()
{
    partsInOrder_data();
}

// At the minimum size every part is inside the widget with the margin; one pixel less in either direction and the
// group no longer is
void tst_ThumbwheelSwitchLayout::minimumSizeKeepsEveryPartInside()
{
    QFETCH(int, digits);
    QFETCH(int, decimals);
    QFETCH(bool, sign);
    QFETCH(bool, unit);
    const ThumbwheelFormat f = format(digits, decimals, sign);
    const QSizeF min = minimumThumbwheelSize(input(f, 0.0, 0.0, unit));
    QVERIFY(whole(min.width()) && whole(min.height()));
    const QRectF inner(kThumbwheelMargin, kThumbwheelMargin, min.width() - 2.0 * kThumbwheelMargin,
                       min.height() - 2.0 * kThumbwheelMargin);
    const ThumbwheelLayout l = layoutThumbwheel(input(f, min.width(), min.height(), unit));
    QVector<QRectF> parts = l.drums + l.plus + l.minus;
    parts << l.tag;
    if (unit) {
        parts << l.unit;
    }
    for (const QRectF &r : parts) {
        QVERIFY2(inner.contains(r), qPrintable(rectText(r) + QStringLiteral(" outside ") + rectText(inner)));
    }
    QVERIFY(inner.contains(l.group));
    for (const QSizeF &smaller : {QSizeF(min.width() - 1.0, min.height()), QSizeF(min.width(), min.height() - 1.0)}) {
        const ThumbwheelLayout s = layoutThumbwheel(input(f, smaller.width(), smaller.height(), unit));
        const QRectF in(kThumbwheelMargin, kThumbwheelMargin, smaller.width() - 2.0 * kThumbwheelMargin,
                        smaller.height() - 2.0 * kThumbwheelMargin);
        QVERIFY2(!in.contains(s.group), qPrintable(QStringLiteral("the group fits in %1 x %2, below the minimum")
                                                       .arg(smaller.width()).arg(smaller.height())));
    }
}

void tst_ThumbwheelSwitchLayout::largerSizeCentresTheGroup()
{
    const ThumbwheelFormat f = format(5, 1);
    const QSizeF min = minimumThumbwheelSize(input(f));
    const double w = min.width() + 100.0;
    const double h = min.height() + 60.0;
    const ThumbwheelLayout l = layoutThumbwheel(input(f, w, h));
    QVERIFY(std::abs(l.group.left() - (w - l.group.right())) <= 1.0);
    QVERIFY(std::abs(l.group.top() - (h - l.group.bottom())) <= 1.0);
    QVERIFY(onWholePixels(l.drums.first()));
}

void tst_ThumbwheelSwitchLayout::nonFiniteAndTinyInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double v : {nan, inf, -inf, -5.0, 0.0}) {
        ThumbwheelLayoutInput in = input(format(4, 2, true), v, v);
        in.text.digitPx = v;
        in.text.digitWidth = v;
        in.text.digitHeight = v;
        in.text.unitWidth = v;
        in.text.tagWidth = v;
        in.text.tagHeight = v;
        const ThumbwheelLayout l = layoutThumbwheel(in);
        const QSizeF min = minimumThumbwheelSize(in);
        QVERIFY(std::isfinite(min.width()) && std::isfinite(min.height()));
        QVector<QRectF> parts = l.drums + l.plus + l.minus;
        parts << l.tag << l.unit << l.decimalPoint << l.group;
        for (const QRectF &r : parts) {
            QVERIFY2(std::isfinite(r.x()) && std::isfinite(r.y()) && std::isfinite(r.width())
                         && std::isfinite(r.height()) && r.width() >= 0.0 && r.height() >= 0.0,
                     qPrintable(rectText(r)));
        }
    }
    // Out-of-range configurations are clamped
    ThumbwheelLayoutInput in = input(format(40, 70, false));
    QCOMPARE(layoutThumbwheel(in).drums.size(), kThumbwheelMaxDigits);
    in.format.digitCount = -3;
    QCOMPARE(layoutThumbwheel(in).drums.size(), 1);
    QVERIFY(layoutThumbwheel(in).decimalPoint.isEmpty());
}

QTEST_APPLESS_MAIN(tst_ThumbwheelSwitchLayout)
#include "tst_ThumbwheelSwitchLayout.moc"
