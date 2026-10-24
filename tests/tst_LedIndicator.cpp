// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>

using namespace QtIndustrialWidgets;

namespace {

// The LED with a label rendered on a known background, at the screen device pixel ratio
// (QT_SCALE_FACTOR in ctest), without the window background
QImage renderLed(LedIndicator &led)
{
    const qreal dpr = led.devicePixelRatioF();
    QImage img((QSizeF(led.size()) * dpr).toSize(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(QColor(30, 36, 48));
    QPainter p(&img);
    led.render(&p, QPoint(), QRegion(), QWidget::RenderFlags());
    p.end();
    return img;
}

void setPaletteText(LedIndicator &led, const QColor &color)
{
    QPalette pal = led.palette();
    pal.setColor(QPalette::WindowText, color);
    led.setPalette(pal);
}

} // namespace

class tst_LedIndicator : public QObject
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
    void labelUsesTextColor();
    void textColorDefaultFollowsPalette();
    void textColorSignal();
};

void tst_LedIndicator::defaultValues()
{
    LedIndicator led;
    QCOMPARE(led.isOn(), true);
    QCOMPARE(led.isBlinking(), false);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Circular);
    QCOMPARE(led.isClickable(), false);
}

void tst_LedIndicator::stateAndToggling()
{
    LedIndicator led;
    led.setOn(false);
    QCOMPARE(led.isOn(), false);

    led.toggle();
    QCOMPARE(led.isOn(), true);

    led.setOff();
    QCOMPARE(led.isOn(), false);
}

void tst_LedIndicator::signalEmission()
{
    LedIndicator led;
    led.setOn(false);

    QSignalSpy spy(&led, &LedIndicator::stateChanged);
    led.setOn(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void tst_LedIndicator::shapes()
{
    LedIndicator led;
    led.setShape(LedIndicator::LedShape::Rectangular);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Rectangular);

    led.setShape(LedIndicator::LedShape::Circular);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Circular);
}

void tst_LedIndicator::blinking()
{
    LedIndicator led;
    led.setBlinking(true);
    QVERIFY(led.isBlinking());

    led.setBlinkRateMs(150);
    QCOMPARE(led.blinkRateMs(), 150);

    led.setBlinking(false);
    QVERIFY(!led.isBlinking());
}

void tst_LedIndicator::clickableMouseInteraction()
{
    LedIndicator led;
    led.setClickable(true);
    led.setOn(false);

    QSignalSpy clickSpy(&led, &LedIndicator::clicked);
    QSignalSpy stateSpy(&led, &LedIndicator::stateChanged);

    QTest::mouseClick(&led, Qt::LeftButton);

    QCOMPARE(clickSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(led.isOn(), true);
}

void tst_LedIndicator::renderOffscreen()
{
    LedIndicator led;
    led.setOn(true);
    led.setShape(LedIndicator::LedShape::Circular);
    led.setLabelText(QStringLiteral("RUN"));
    led.resize(60, 60);

    QPixmap pix(led.size());
    led.render(&pix);
    QVERIFY(!pix.isNull());

    led.setShape(LedIndicator::LedShape::Rectangular);
    QPixmap pixRect(led.size());
    led.render(&pixRect);
    QVERIFY(!pixRect.isNull());
}

void tst_LedIndicator::extremeResizeNoCrash()
{
    LedIndicator led;
    led.resize(1, 1);
    QPixmap pixSmall(led.size());
    led.render(&pixSmall);

    led.resize(1920, 1080);
    QPixmap pixLarge(led.size());
    led.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

// #45: the label is drawn in textColor. The same colour reached two ways must give the same image:
// A = palette WindowText #ff00ff with textColor unset, B = palette WindowText #00ff00 with textColor
// #ff00ff. Only the label uses WindowText, so B must equal A exactly. A must differ from the render
// with WindowText #00ff00 (the label is there and drawn in that colour).
void tst_LedIndicator::labelUsesTextColor()
{
    LedIndicator led(QColor(46, 204, 113));
    led.setLabelText(QStringLiteral("RUN"));
    led.setOn(true);
    led.resize(140, 56);

    setPaletteText(led, QColor(255, 0, 255));
    const QImage a = renderLed(led);
    setPaletteText(led, QColor(0, 255, 0));
    const QImage green = renderLed(led);
    led.setTextColor(QColor(255, 0, 255));
    const QImage b = renderLed(led);

    QVERIFY(a != green);
    QVERIFY2(b == a, "the label is not drawn in textColor (render differs from the same colour via the palette)");
}

// #45: by default (invalid textColor) the label follows QPalette::WindowText, as before; setting an
// invalid colour again restores that
void tst_LedIndicator::textColorDefaultFollowsPalette()
{
    LedIndicator led(QColor(46, 204, 113));
    led.setLabelText(QStringLiteral("RUN"));
    led.resize(140, 56);
    QVERIFY(!led.textColor().isValid());

    setPaletteText(led, QColor(255, 0, 255));
    const QImage magenta = renderLed(led);
    setPaletteText(led, QColor(0, 255, 0));
    const QImage initial = renderLed(led);
    QVERIFY2(initial != magenta, "the label does not follow QPalette::WindowText");

    led.setTextColor(Qt::magenta);
    QVERIFY(renderLed(led) != initial);
    led.setTextColor(QColor());
    QVERIFY(!led.textColor().isValid());
    QCOMPARE(renderLed(led), initial);
}

// #45: appearanceChanged is emitted once per change of textColor, not when the same colour is set
void tst_LedIndicator::textColorSignal()
{
    LedIndicator led;
    QSignalSpy spy(&led, &LedIndicator::appearanceChanged);
    led.setTextColor(Qt::red);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(led.textColor(), QColor(Qt::red));
    led.setTextColor(Qt::red);
    QCOMPARE(spy.count(), 1);
    led.setTextColor(QColor());
    QCOMPARE(spy.count(), 2);
    led.setTextColor(QColor());
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(tst_LedIndicator)
#include "tst_LedIndicator.moc"


