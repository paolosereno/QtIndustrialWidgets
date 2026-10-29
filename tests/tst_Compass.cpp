// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/Compass.h>

#include "FontTestUtils.h"
#include "ReadoutTestUtils.h"

#include <memory>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

namespace {

// Synthetic mouse event with explicit buttons, delivered synchronously. QTest::mouseMove() does not deliver a
// MouseMove event to an offscreen widget with Qt 6.2 (#79), so drags are sent as explicit events.
void sendMouse(QWidget &w, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, w.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&w, &e);
}


constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QImage renderCompass(Compass &c)
{
    return c.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// #151: a compass whose parts can be told apart in a render: ticks and labels pure green (textColor and
// cardinalColor), the north half of the needle and the "N" label pure blue (needleColor), the south half pure
// magenta (needleTailColor), the lubber line pure yellow. No heading bug, no digital readout. 300 x 300: the
// dial radius is 144 px (side / 2 - 6)
void prepareForDial(Compass &c, Compass::DisplayMode mode, double heading)
{
    c.setDisplayMode(mode);
    c.setDialColor(QColor(20, 24, 32));
    c.setBezelColor(QColor(48, 56, 70));
    c.setTextColor(QColor(0, 255, 0));
    c.setCardinalColor(QColor(0, 255, 0));
    c.setNeedleColor(QColor(0, 0, 255));
    c.setNeedleTailColor(QColor(255, 0, 255));
    c.setLubberColor(QColor(255, 255, 0));
    c.setHeadingBugVisible(false);
    c.setDigitalReadoutVisible(false);
    c.resize(300, 300);
    c.setHeading(heading);
}

bool isGreen(QRgb p) { return qGreen(p) > 150 && qGreen(p) - qRed(p) > 80 && qGreen(p) - qBlue(p) > 80; }
// The south half of the needle: magenta, red and blue alike (pure, darker or antialiased on the dial). Text
// antialiasing can fringe the red flag with pixels like (226, 80, 162): those are not the needle
bool isTail(QRgb p) { return qRed(p) > 150 && qBlue(p) > 150 && std::abs(qRed(p) - qBlue(p)) < 40 && qGreen(p) < 60; }
bool isLubber(QRgb p) { return qRed(p) > 180 && qGreen(p) > 180 && qBlue(p) < 90; }
// The needle and the "N" label at full strength; bluish() also takes them dimmed
bool isBlue(QRgb p) { return qBlue(p) > 150 && qBlue(p) - qRed(p) > 80 && qBlue(p) - qGreen(p) > 80; }
bool isBluish(QRgb p) { return qBlue(p) > 60 && qBlue(p) - qRed(p) > 30 && qBlue(p) - qGreen(p) > 30; }
// The HDG failure flag: a saturated red, a colour that prepareForDial() gives to no other part
bool isFlagRed(QRgb p) { return qRed(p) > 180 && qGreen(p) < 90 && qBlue(p) < 100; }

struct Dial {
    QPointF centre; // device px
    double radius;  // device px
};

// As in Compass.cpp: centre QPointF(rect().center()), radius min(w, h) / 2 - 6 (logical px), scaled to device px
Dial dialOf(const Compass &c, const QImage &img)
{
    const qreal dpr = img.devicePixelRatio();
    return {QPointF(c.rect().center()) * dpr, (std::min(c.width(), c.height()) / 2.0 - 6.0) * dpr};
}

// Pixels matching \a pred at a distance from the centre within [r0, r1] x radius
template <typename Pred>
QVector<QPoint> ringPixels(const QImage &img, const Dial &d, double r0, double r1, Pred pred)
{
    QVector<QPoint> out;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const double r = std::hypot(x + 0.5 - d.centre.x(), y + 0.5 - d.centre.y()) / d.radius;
            if (r >= r0 && r <= r1 && pred(img.pixel(x, y))) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

// Rows holding a run of flag-red pixels at least \a minRun px long: a solid flag gives many such rows; the
// strokes of a text or a needle along a radius do not
int flagRows(const QImage &img, int minRun)
{
    int rows = 0;
    for (int y = 0; y < img.height(); ++y) {
        int run = 0;
        int longest = 0;
        for (int x = 0; x < img.width(); ++x) {
            run = isFlagRed(img.pixel(x, y)) ? run + 1 : 0;
            longest = std::max(longest, run);
        }
        if (longest >= minRun) {
            ++rows;
        }
    }
    return rows;
}

// Empty when \a img shows a flag: at least 0.08 x radius rows with a red run of at least 0.25 x radius
QString flagError(const QImage &img, const Dial &d)
{
    const int rows = flagRows(img, static_cast<int>(std::ceil(d.radius * 0.25)));
    const int needed = static_cast<int>(std::ceil(d.radius * 0.08));
    return rows >= needed ? QString() : QStringLiteral("%1 flag rows, %2 needed").arg(rows).arg(needed);
}
} // namespace

class tst_Compass : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void textSizeIgnoresWidgetFontSize();
    void initTestCase();
    void defaultValues();
    void headingNormalization();
    void targetHeadingAndBug();
    void courseDeviationCalculation();
    void displayModeSwitch();
    void colorSetters();
    void mouseInteractionBug();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void cleanupTestCase();
    void invalidHeadingIsNaN();
    void northUpInvalidHidesNeedleAndShowsFlag();
    void headingUpInvalidShowsDimmedCardAtNorthWithoutLubber();
    void invalidReadoutShowsDashes_data();
    void invalidReadoutShowsDashes();
    void bugDragOnInvalidHeadingUpUsesTheCardAtNorth();
    void flagVisibleAtMinimumSize_data();
    void flagVisibleAtMinimumSize();
    void valueStateOnlyValidOrInvalid();
    void valueStateSignalOrder();
};

void tst_Compass::initTestCase()
{
}

void tst_Compass::cleanupTestCase()
{
}

void tst_Compass::defaultValues()
{
    Compass compass;
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(compass.targetHeading(), 0.0);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);
    QVERIFY(compass.isHeadingBugVisible());
    QVERIFY(compass.isHeadingBugInteractive());
    QVERIFY(compass.isLubberLineVisible());
    QVERIFY(compass.isDigitalReadoutVisible());
    QCOMPARE(compass.courseDeviation(), 0.0);
    QVERIFY(compass.minimumSizeHint().width() > 0);
    QVERIFY(compass.sizeHint().width() >= compass.minimumSizeHint().width());
}

void tst_Compass::headingNormalization()
{
    Compass compass;
    QSignalSpy spy(&compass, &Compass::headingChanged);

    // Standard heading within [0, 360)
    compass.setHeading(45.0);
    QCOMPARE(compass.heading(), 45.0);
    QCOMPARE(spy.count(), 1);

    // 360.0 wraps to 0.0
    compass.setHeading(360.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2);

    // Multiple revolutions
    compass.setHeading(720.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2); // No change from 0.0, no new emission

    compass.setHeading(450.0);
    QCOMPARE(compass.heading(), 90.0);
    QCOMPARE(spy.count(), 3);

    // Negative angle wraps properly
    compass.setHeading(-90.0);
    QCOMPARE(compass.heading(), 270.0);
    QCOMPARE(spy.count(), 4);

    compass.setHeading(-370.0);
    QCOMPARE(compass.heading(), 350.0);
    QCOMPARE(spy.count(), 5);

    // Static helper
    QCOMPARE(Compass::normalizeDegrees(0.0), 0.0);
    QCOMPARE(Compass::normalizeDegrees(360.0), 0.0);
    QCOMPARE(Compass::normalizeDegrees(-45.0), 315.0);
}

void tst_Compass::targetHeadingAndBug()
{
    Compass compass;
    QSignalSpy spyTarget(&compass, &Compass::targetHeadingChanged);
    QSignalSpy spyApp(&compass, &Compass::appearanceChanged);

    compass.setTargetHeading(120.0);
    QCOMPARE(compass.targetHeading(), 120.0);
    QCOMPARE(spyTarget.count(), 1);

    compass.setTargetHeading(-45.0);
    QCOMPARE(compass.targetHeading(), 315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Setting same value does not emit
    compass.setTargetHeading(315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Bug visibility and interaction
    compass.setHeadingBugVisible(false);
    QVERIFY(!compass.isHeadingBugVisible());
    QVERIFY(spyApp.count() >= 1);

    compass.setHeadingBugInteractive(false);
    QVERIFY(!compass.isHeadingBugInteractive());

    compass.setLubberLineVisible(false);
    QVERIFY(!compass.isLubberLineVisible());

    compass.setDigitalReadoutVisible(false);
    QVERIFY(!compass.isDigitalReadoutVisible());
}

void tst_Compass::courseDeviationCalculation()
{
    Compass compass;

    // On course
    compass.setHeading(45.0);
    compass.setTargetHeading(45.0);
    QCOMPARE(compass.courseDeviation(), 0.0);

    // 10 degrees to starboard (+10°)
    compass.setHeading(50.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), 10.0);

    // 10 degrees to port (-10°)
    compass.setHeading(30.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), -10.0);

    // Crossing 0° / 360° north meridian
    compass.setHeading(10.0);
    compass.setTargetHeading(350.0);
    QCOMPARE(compass.courseDeviation(), 20.0);

    compass.setHeading(350.0);
    compass.setTargetHeading(10.0);
    QCOMPARE(compass.courseDeviation(), -20.0);
}

void tst_Compass::displayModeSwitch()
{
    Compass compass;
    QSignalSpy spyMode(&compass, &Compass::displayModeChanged);

    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    // Re-setting same mode does not emit duplicate
    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    compass.setDisplayMode(Compass::DisplayMode::HeadingUp);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);
    QCOMPARE(spyMode.count(), 2);
}

void tst_Compass::colorSetters()
{
    Compass compass;
    QSignalSpy spy(&compass, &Compass::appearanceChanged);

    compass.setDialColor(QColor(10, 10, 10));
    QCOMPARE(compass.dialColor(), QColor(10, 10, 10));

    compass.setBezelColor(QColor(40, 50, 60));
    QCOMPARE(compass.bezelColor(), QColor(40, 50, 60));

    compass.setTextColor(QColor(255, 255, 255));
    QCOMPARE(compass.textColor(), QColor(255, 255, 255));

    compass.setCardinalColor(QColor(0, 200, 255));
    QCOMPARE(compass.cardinalColor(), QColor(0, 200, 255));

    compass.setNeedleColor(QColor(255, 0, 0));
    QCOMPARE(compass.needleColor(), QColor(255, 0, 0));

    compass.setNeedleTailColor(QColor(200, 200, 200));
    QCOMPARE(compass.needleTailColor(), QColor(200, 200, 200));

    compass.setBugColor(QColor(255, 128, 0));
    QCOMPARE(compass.bugColor(), QColor(255, 128, 0));

    compass.setLubberColor(QColor(255, 255, 0));
    QCOMPARE(compass.lubberColor(), QColor(255, 255, 0));

    QVERIFY(spy.count() >= 8);
}

void tst_Compass::mouseInteractionBug()
{
    Compass compass;
    compass.resize(300, 300);
    compass.show();
    QVERIFY(QTest::qWaitForWindowExposed(&compass));

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    compass.setHeadingBugInteractive(true);

    // Center is (150, 150).
    // Click at 3 o'clock: (250, 150) -> angle = 90° (East)
    QPoint eastPoint(250, 150);
    QTest::mousePress(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);
    QTest::mouseRelease(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);

    // Allow slight float tolerance around 90°
    QVERIFY(std::abs(compass.targetHeading() - 90.0) < 3.0);

    // Drag to 6 o'clock: (150, 250) -> angle = 180° (South)
    QPoint southPoint(150, 250);
    sendMouse(compass, QEvent::MouseButtonPress, eastPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(compass, QEvent::MouseMove, southPoint, Qt::NoButton, Qt::LeftButton);
    sendMouse(compass, QEvent::MouseButtonRelease, southPoint, Qt::LeftButton, Qt::NoButton);

    QVERIFY(std::abs(compass.targetHeading() - 180.0) < 3.0);
}

void tst_Compass::renderOffscreen()
{
    Compass compass;
    compass.resize(250, 250);
    compass.setHeading(45.0);
    compass.setTargetHeading(90.0);

    // Render HeadingUp mode
    compass.setDisplayMode(Compass::DisplayMode::HeadingUp);
    QPixmap pixmap1(250, 250);
    pixmap1.fill(Qt::transparent);
    compass.render(&pixmap1);
    QVERIFY(!pixmap1.isNull());

    // Render NorthUp mode
    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QPixmap pixmap2(250, 250);
    pixmap2.fill(Qt::transparent);
    compass.render(&pixmap2);
    QVERIFY(!pixmap2.isNull());
}

void tst_Compass::extremeResizeNoCrash()
{
    Compass compass;

    const QList<QSize> extremeSizes = {
        QSize(0, 0),
        QSize(1, 1),
        QSize(5, 5),
        QSize(10, 10),
        QSize(20, 20),
        QSize(100, 100),
        QSize(800, 800),
        QSize(1920, 1080),
        QSize(3840, 2160)
    };

    for (const auto &sz : extremeSizes) {
        compass.resize(sz);
        if (sz.width() > 0 && sz.height() > 0) {
            QPixmap pixmap(sz);
            compass.render(&pixmap);
        }
    }
}

void tst_Compass::nanAndInfinityResilience()
{
    Compass compass;
    compass.setHeading(90.0);
    QCOMPARE(compass.heading(), 90.0);

    // #151: NaN and ±Inf have no direction: the heading is invalid (NaN), never the last value or 0°
    compass.setHeading(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(std::isnan(compass.heading()));
    compass.setHeading(90.0);
    compass.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY(std::isnan(compass.heading()));
    compass.setHeading(90.0);
    compass.setHeading(-std::numeric_limits<double>::infinity());
    QVERIFY(std::isnan(compass.heading()));

    // Target heading NaN/Inf ignored
    compass.setTargetHeading(45.0);
    compass.setTargetHeading(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(compass.targetHeading(), 45.0);
    compass.setTargetHeading(std::numeric_limits<double>::infinity());
    QCOMPARE(compass.targetHeading(), 45.0);

    // normalizeDegrees static helper test
    QCOMPARE(Compass::normalizeDegrees(std::numeric_limits<double>::quiet_NaN()), 0.0);
    QCOMPARE(Compass::normalizeDegrees(std::numeric_limits<double>::infinity()), 0.0);

    // Offscreen render in both display modes
    compass.resize(200, 200);
    QPixmap pix(compass.size());
    compass.render(&pix);
    QVERIFY(!pix.isNull());

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QPixmap pixNorth(compass.size());
    compass.render(&pixNorth);
    QVERIFY(!pixNorth.isNull());
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_Compass::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<Compass>();
        if (font) g->setFont(*font);
        g->setDigitalReadoutVisible(false);
        g->resize(300, 300);
        return g;
    };
    auto standard = make(nullptr);
    const QFont doubled = FontTest::doubledFont();
    auto large = make(&doubled);
    const int base = FontTest::textGlyphHeight(*standard);
    const int grown = FontTest::textGlyphHeight(*large);
    QVERIFY2(base > 0, "no text drawn in textColor");
    QVERIFY2(grown == base, qPrintable(QStringLiteral("tallest glyph %1 px with the default font, %2 px with a font twice as large").arg(base).arg(grown)));
}

// #151: NaN and ±Inf have no direction: heading() and courseDeviation() are NaN, headingChanged(NaN) is emitted
// once (a repeated invalid input emits nothing), and the next finite heading is emitted
void tst_Compass::invalidHeadingIsNaN()
{
    Compass c;
    c.setHeading(90.0);
    c.setTargetHeading(45.0);
    QSignalSpy spy(&c, &Compass::headingChanged);

    c.setHeading(kNaN);
    QVERIFY2(std::isnan(c.heading()), qPrintable(QStringLiteral("heading() = %1").arg(c.heading())));
    QVERIFY2(std::isnan(c.courseDeviation()), qPrintable(QStringLiteral("courseDeviation() = %1").arg(c.courseDeviation())));
    QCOMPARE(spy.count(), 1);
    QVERIFY(std::isnan(spy.at(0).at(0).toDouble()));

    c.setHeading(kNaN);
    c.setHeading(std::numeric_limits<double>::infinity());
    c.setHeading(-std::numeric_limits<double>::infinity());
    QCOMPARE(spy.count(), 1);
    QVERIFY(std::isnan(c.heading()));

    c.setHeading(30.0);
    QCOMPARE(c.heading(), 30.0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toDouble(), 30.0);

    // +Inf is invalid, not 0° (North)
    c.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY2(std::isnan(c.heading()), qPrintable(QStringLiteral("heading() = %1").arg(c.heading())));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(c.targetHeading(), 45.0);
}

// #151: North-up, an invalid heading draws no needle and shows the HDG failure flag
void tst_Compass::northUpInvalidHidesNeedleAndShowsFlag()
{
    Compass c;
    prepareForDial(c, Compass::DisplayMode::NorthUp, 90.0);
    const QImage valid = renderCompass(c);
    const Dial d = dialOf(c, valid);
    QVERIFY(!ringPixels(valid, d, 0.0, 1.0, isTail).isEmpty());
    QVERIFY(!ringPixels(valid, d, 0.15, 0.6, isBlue).isEmpty());
    QVERIFY2(!flagError(valid, d).isEmpty(), "a valid heading shows the flag");

    c.setHeading(kNaN);
    const QImage invalid = renderCompass(c);
    QCOMPARE(ringPixels(invalid, d, 0.0, 1.0, isTail).size(), 0);
    QCOMPARE(ringPixels(invalid, d, 0.15, 0.6, isBlue).size(), 0);
    const QString flag = flagError(invalid, d);
    QVERIFY2(flag.isEmpty(), qPrintable(flag));
}

// #151: heading-up, an invalid heading draws the card at 0° with dimmed colours and no lubber line, and shows
// the HDG failure flag, so that the card is not read as a heading of 000
void tst_Compass::headingUpInvalidShowsDimmedCardAtNorthWithoutLubber()
{
    Compass reference;
    prepareForDial(reference, Compass::DisplayMode::HeadingUp, 0.0);
    const QImage north = renderCompass(reference);
    const Dial d = dialOf(reference, north);
    const int brightLabels = ringPixels(north, d, 0.6, 1.0, isGreen).size();
    QVERIFY(brightLabels > 0);

    Compass c;
    prepareForDial(c, Compass::DisplayMode::HeadingUp, 47.0);
    QVERIFY(!ringPixels(renderCompass(c), d, 0.0, 1.2, isLubber).isEmpty());
    c.setHeading(kNaN);
    const QImage invalid = renderCompass(c);

    // The "N" label (needle colour, dimmed) at the top of the dial: the card is drawn at 0°
    const QVector<QPoint> n = ringPixels(invalid, d, 0.6, 1.0, isBluish);
    QVERIFY(!n.isEmpty());
    QPointF sum;
    for (const QPoint &p : n) {
        sum += QPointF(p) + QPointF(0.5, 0.5);
    }
    const QPointF nCentre = sum / n.size();
    QVERIFY2(std::abs(nCentre.x() - d.centre.x()) < 0.06 * d.radius && nCentre.y() < d.centre.y() - 0.5 * d.radius,
             qPrintable(QStringLiteral("N at (%1, %2), centre (%3, %4)")
                            .arg(nCentre.x()).arg(nCentre.y()).arg(d.centre.x()).arg(d.centre.y())));

    // Dimmed: the ticks and labels lose their full colour
    const int dimLabels = ringPixels(invalid, d, 0.6, 1.0, isGreen).size();
    QVERIFY2(dimLabels < brightLabels / 10, qPrintable(QStringLiteral("%1 full-colour label pixels, %2 on a valid card")
                                                         .arg(dimLabels).arg(brightLabels)));
    QCOMPARE(ringPixels(invalid, d, 0.0, 1.2, isLubber).size(), 0);
    const QString flag = flagError(invalid, d);
    QVERIFY2(flag.isEmpty(), qPrintable(flag));
}

// #151: an invalid heading reads "---" (then the degree sign), not the last heading. The test font has no
// glyph for "°": only the first three ink groups are checked
void tst_Compass::invalidReadoutShowsDashes_data()
{
    QTest::addColumn<Compass::DisplayMode>("mode");
    QTest::newRow("heading-up") << Compass::DisplayMode::HeadingUp;
    QTest::newRow("north-up") << Compass::DisplayMode::NorthUp;
}

void tst_Compass::invalidReadoutShowsDashes()
{
    QFETCH(Compass::DisplayMode, mode);
    QString error;
    const QFont font = ReadoutTest::readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    Compass c;
    c.setFont(font);
    c.setDisplayMode(mode);
    c.setHeadingBugVisible(false);
    c.resize(300, 300);
    c.setHeading(90.0);

    // The display: where the render changes when it is hidden
    c.setDigitalReadoutVisible(false);
    const QImage without = renderCompass(c);
    c.setDigitalReadoutVisible(true);
    const QImage with = renderCompass(c);
    QRect box;
    for (int y = 0; y < with.height(); ++y) {
        for (int x = 0; x < with.width(); ++x) {
            if (with.pixel(x, y) != without.pixel(x, y)) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    QVERIFY(!box.isEmpty());
    box.adjust(3, 3, -3, -3);

    c.setHeading(kNaN);
    const QVector<ReadoutTest::InkGroup> groups = ReadoutTest::readoutInkGroups(renderCompass(c), box);
    QVERIFY2(groups.size() >= 3, qPrintable(QStringLiteral("%1 ink groups").arg(groups.size())));
    const QString dashes = ReadoutTest::dashesError(groups.mid(0, 3));
    QVERIFY2(dashes.isEmpty(), qPrintable(dashes));
}

// #151: heading-up with an invalid heading, the card is at 0°: a click on the dial sets the target heading
// read on that card (3 o'clock is 090°)
void tst_Compass::bugDragOnInvalidHeadingUpUsesTheCardAtNorth()
{
    Compass c;
    c.setDisplayMode(Compass::DisplayMode::HeadingUp);
    c.resize(300, 300);
    c.setHeading(47.0);
    c.setHeading(kNaN);
    const QPointF centre(c.rect().center());
    const double radius = 300 / 2.0 - 6.0;
    const QPointF east = centre + QPointF(radius * 0.8, 0.0);
    sendMouse(c, QEvent::MouseButtonPress, east, Qt::LeftButton, Qt::LeftButton);
    sendMouse(c, QEvent::MouseButtonRelease, east, Qt::LeftButton, Qt::NoButton);
    QVERIFY2(std::abs(c.targetHeading() - 90.0) < 1.0, qPrintable(QStringLiteral("targetHeading() = %1").arg(c.targetHeading())));
}

// #151: the HDG flag stays a solid, visible flag at the minimum size
void tst_Compass::flagVisibleAtMinimumSize_data()
{
    QTest::addColumn<Compass::DisplayMode>("mode");
    QTest::newRow("heading-up") << Compass::DisplayMode::HeadingUp;
    QTest::newRow("north-up") << Compass::DisplayMode::NorthUp;
}

void tst_Compass::flagVisibleAtMinimumSize()
{
    QFETCH(Compass::DisplayMode, mode);
    Compass c;
    prepareForDial(c, mode, 0.0);
    c.setDigitalReadoutVisible(true);
    c.resize(c.minimumSizeHint());
    c.setHeading(kNaN);
    const QImage img = renderCompass(c);
    const QString flag = flagError(img, dialOf(c, img));
    QVERIFY2(flag.isEmpty(), qPrintable(QStringLiteral("%1x%2: ").arg(c.width()).arg(c.height()) + flag));
}

// #151: a heading has no range: every finite angle is Valid, NaN and ±Inf are Invalid; never Over/Underrange
void tst_Compass::valueStateOnlyValidOrInvalid()
{
    const double inf = std::numeric_limits<double>::infinity();
    Compass c;
    QCOMPARE(c.valueState(), ValueState::Valid);
    struct Step {
        double input;
        ValueState state;
    };
    const Step steps[] = {
        {720.5, ValueState::Valid}, {-30.0, ValueState::Valid}, {kNaN, ValueState::Invalid}, {1e9, ValueState::Valid},
        {inf, ValueState::Invalid}, {359.9, ValueState::Valid}, {-inf, ValueState::Invalid}, {0.0, ValueState::Valid},
    };
    for (const Step &step : steps) {
        c.setHeading(step.input);
        QVERIFY2(c.valueState() == step.state, qPrintable(QStringLiteral("input %1").arg(step.input)));
        QCOMPARE(std::isnan(c.heading()), step.state == ValueState::Invalid);
    }
    QCOMPARE(c.property("valueState").value<ValueState>(), ValueState::Valid);
}

void tst_Compass::valueStateSignalOrder()
{
    Compass c;
    c.setHeading(90.0);
    QStringList events;
    connect(&c, &Compass::headingChanged, &c, [&events](double h) { events << QStringLiteral("heading %1").arg(h); });
    connect(&c, &Compass::valueStateChanged, &c,
            [&events](ValueState st) { events << QStringLiteral("state %1").arg(static_cast<int>(st)); });

    c.setHeading(kNaN);
    QCOMPARE(events, QStringList({QStringLiteral("heading nan"), QStringLiteral("state 3")}));
    events.clear();
    c.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY(events.isEmpty());

    // Back to the heading held before: still a change, since heading() was NaN
    c.setHeading(90.0);
    QCOMPARE(events, QStringList({QStringLiteral("heading 90"), QStringLiteral("state 0")}));
    events.clear();
    c.setHeading(450.0);
    QVERIFY(events.isEmpty());
}

QTEST_MAIN(tst_Compass)
#include "tst_Compass.moc"


