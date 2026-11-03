// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #152: TankDisplay. Value states (#151), limits and their state, the drawing of the vessel, the fill, the markers
// and the readout, the computed minimum size and the screen change of #177.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/TankDisplay.h>

#include "internal/FontScale.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/TankDisplayLayout.h"
#include "internal/TankDisplayTestAccess.h"

#include "FontTestUtils.h"
#include "HatchTestUtils.h"
#include "ReadoutTestUtils.h"
#include "ScreenChangeTestUtils.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QVBoxLayout>

#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;
using LimitState = TankDisplay::LimitState;
using TankShape = TankDisplay::TankShape;

class tst_TankDisplay : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void newDisplayIsDrawnWithoutAValue();
    void valueStateAndRawValue();
    void signalOrder();
    void acceptedLimitOrders_data();
    void acceptedLimitOrders();
    void rejectedLimitOrders_data();
    void rejectedLimitOrders();
    void limitCrossings_data();
    void limitCrossings();
    void limitStateFollowsLimitAndHysteresisChanges();
    void fillHeight_data();
    void fillHeight();
    void outlineAtItsGeometricPoints_data();
    void outlineAtItsGeometricPoints();
    void granularFillHasGrainsOnlyBelowTheSurface();
    void scaleLabelsAtTheirValues();
    void limitMarkers_data();
    void limitMarkers();
    void limitLinesOnWholeDevicePixels();
    void everyActiveLimitStateIsVisible_data();
    void everyActiveLimitStateIsVisible();
    void invalidVesselIsHatched_data();
    void invalidVesselIsHatched();
    void readoutStates();
    void textsInsideTheWidgetFromTheMinimumUp_data();
    void textsInsideTheWidgetFromTheMinimumUp();
    void readoutNeverBelowSevenPoints_data();
    void readoutNeverBelowSevenPoints();
    void unitGoesBeforeTheNumber();
    void minimumSizeHintFollowsTheConfiguration();
    void screenChangeOfAVisibleTankDisplay_data();
    void screenChangeOfAVisibleTankDisplay();
    void nonFiniteInput();
};

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Colours that no other part of the drawing uses
const QColor kContent(255, 0, 255);
const QColor kVessel(0, 0, 255);
const QColor kInterior = QColor(0, 0, 255).darker(400);
const QColor kScale(0, 255, 0);
const QColor kText(255, 255, 0);
const QColor kLimit(255, 128, 0);
const QColor kBackground(0, 0, 0);
const QColor kPod(16, 20, 26); // the readout panel, as in TankDisplay.cpp

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

// The display with the test colours on an opaque black background
void prepare(TankDisplay &t, QSize size = QSize(160, 260))
{
    t.setContentColor(kContent);
    t.setVesselColor(kVessel);
    t.setScaleColor(kScale);
    t.setTextColor(kText);
    t.setLimitColor(kLimit);
    QPalette pal = t.palette();
    pal.setColor(QPalette::Window, kBackground);
    t.setPalette(pal);
    t.setAutoFillBackground(true);
    t.resize(size);
}

QImage render(TankDisplay &t)
{
    return t.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

internal::TankLayout layoutOf(const TankDisplay &t)
{
    return internal::TankDisplayTestAccess::lastLayout(t);
}

// Pixel at a logical point
QRgb at(const QImage &img, double x, double y)
{
    const double dpr = img.devicePixelRatio();
    const int px = static_cast<int>(std::floor(x * dpr));
    const int py = static_cast<int>(std::floor(y * dpr));
    if (px < 0 || py < 0 || px >= img.width() || py >= img.height()) {
        return qRgba(0, 0, 0, 0);
    }
    return img.pixel(px, py);
}

QString rgb(QRgb p)
{
    return QStringLiteral("rgb(%1, %2, %3)").arg(qRed(p)).arg(qGreen(p)).arg(qBlue(p));
}

// Device pixels of \a img in a logical rectangle that match a predicate
int countIn(const QImage &img, const QRectF &logical, const std::function<bool(QRgb)> &match)
{
    const double dpr = img.devicePixelRatio();
    const QRect r = QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect().intersected(img.rect());
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            n += match(img.pixel(x, y)) ? 1 : 0;
        }
    }
    return n;
}

// Pixels drawn in a colour (white): those that change when only that colour becomes transparent
QImage inkOf(TankDisplay &t, void (TankDisplay::*set)(const QColor &), const QColor &color)
{
    (t.*set)(QColor(color.red(), color.green(), color.blue(), 0));
    const QImage without = render(t);
    (t.*set)(color);
    const QImage with = render(t);
    QImage ink(with.size(), QImage::Format_ARGB32);
    ink.setDevicePixelRatio(with.devicePixelRatio());
    for (int y = 0; y < with.height(); ++y) {
        for (int x = 0; x < with.width(); ++x) {
            ink.setPixel(x, y, with.pixel(x, y) != without.pixel(x, y) ? qRgb(255, 255, 255) : qRgb(0, 0, 0));
        }
    }
    return ink;
}

void addShapes()
{
    QTest::addColumn<int>("shape");
    QTest::newRow("vertical cylinder") << static_cast<int>(TankShape::VerticalCylinder);
    QTest::newRow("horizontal cylinder") << static_cast<int>(TankShape::HorizontalCylinder);
    QTest::newRow("hopper") << static_cast<int>(TankShape::Hopper);
    QTest::newRow("sphere") << static_cast<int>(TankShape::Sphere);
}

QSize sizeFor(TankShape shape)
{
    return shape == TankShape::HorizontalCylinder ? QSize(280, 200) : shape == TankShape::Sphere ? QSize(220, 240)
                                                                                                  : QSize(160, 280);
}

QString stateName(LimitState s)
{
    return QString::fromLatin1(QMetaEnum::fromType<LimitState>().valueToKey(static_cast<int>(s)));
}

// Readout ink: the pixels that change when only the readout colour changes (#167)
QVector<QPoint> readoutInk(TankDisplay &t)
{
    t.setReadoutColor(QColor(255, 0, 255, 0));
    const QImage without = render(t);
    t.setReadoutColor(QColor(255, 0, 255));
    const QImage with = render(t);
    t.setReadoutColor(QColor(240, 244, 248));
    QVector<QPoint> ink;
    for (int y = 0; y < with.height(); ++y) {
        for (int x = 0; x < with.width(); ++x) {
            if (with.pixel(x, y) != without.pixel(x, y)) {
                ink.push_back(QPoint(x, y));
            }
        }
    }
    return ink;
}

QRect boundsOf(const QVector<QPoint> &points)
{
    QRect r;
    for (const QPoint &p : points) {
        r |= QRect(p, QSize(1, 1));
    }
    return r;
}

// Ink box of a readout line drawn at 7 pt as TankDisplay draws it (bold widget font, greyscale antialiasing, on the
// opaque panel colour, at the widget's DPI and DPR): the smallest box over the 64 x 64 sub-pixel positions of the
// raster engine (the memory of #167: no slack, an opaque background)
QSize sevenPointLineInk(const QString &text, bool triangle, const QWidget &w, qreal dpr)
{
    static QHash<QString, QSize> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(triangle).arg(text).arg(w.logicalDpiY()).arg(dpr);
    if (cache.contains(key)) {
        return cache.value(key);
    }
    QFont font = w.font();
    font.setBold(true);
    font.setPixelSize(internal::minimumReadoutPixelSize(w.logicalDpiY()));
    font.setStyleStrategy(QFont::StyleStrategy(font.styleStrategy() | QFont::NoSubpixelAntialias));
    const QFontMetricsF fm(font, &w);
    const double triW = triangle ? fm.capHeight() * 0.9 : 0.0;
    const double gap = (triangle && !text.isEmpty()) ? triW * 0.35 : 0.0;
    QSize smallest(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    const QSize area(static_cast<int>(std::ceil(triW + gap + fm.horizontalAdvance(text) + 8.0)),
                     static_cast<int>(std::ceil(fm.height() + 8.0)));
    for (int step = 0; step < 64 * 64; ++step) {
        const QPointF offset((step % 64) / 64.0 / dpr, (step / 64) / 64.0 / dpr);
        QImage img(area * dpr, QImage::Format_RGB32);
        img.setDevicePixelRatio(dpr);
        img.setDotsPerMeterX(qRound(w.logicalDpiX() / 0.0254));
        img.setDotsPerMeterY(qRound(w.logicalDpiY() / 0.0254));
        img.fill(kPod);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            const QPointF origin = QPointF(4.0, 4.0 + fm.ascent()) + offset;
            if (triangle) {
                const double midY = origin.y() - fm.capHeight() * 0.5;
                QPolygonF tri;
                tri << QPointF(origin.x(), midY + triW * 0.5) << QPointF(origin.x() + triW, midY + triW * 0.5)
                    << QPointF(origin.x() + triW * 0.5, midY - triW * 0.5);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 0, 255));
                p.drawPolygon(tri);
            }
            p.setFont(font);
            p.setPen(QColor(255, 0, 255));
            p.drawText(origin + QPointF(triW + gap, 0.0), text);
        }
        QRect box;
        for (int y = 0; y < img.height(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (row[x] != kPod.rgb()) {
                    box |= QRect(x, y, 1, 1);
                }
            }
        }
        smallest = QSize(std::min(smallest.width(), box.width()), std::min(smallest.height(), box.height()));
    }
    cache.insert(key, smallest);
    return smallest;
}

} // namespace

void tst_TankDisplay::defaults()
{
    TankDisplay t;
    QCOMPARE(t.shape(), TankShape::VerticalCylinder);
    QCOMPARE(t.content(), TankDisplay::Content::Liquid);
    QCOMPARE(t.minimum(), 0.0);
    QCOMPARE(t.maximum(), 100.0);
    // No value until the first setValue() (#152), unlike the gauges
    QVERIFY(std::isnan(t.value()));
    QVERIFY(std::isnan(t.rawValue()));
    QCOMPARE(t.valueState(), ValueState::Invalid);
    QCOMPARE(t.precision(), 1);
    QCOMPARE(t.unit(), QStringLiteral("%"));
    QVERIFY(std::isnan(t.lowLowLimit()) && std::isnan(t.lowLimit()) && std::isnan(t.highLimit())
            && std::isnan(t.highHighLimit()));
    QCOMPARE(t.limitHysteresis(), 0.0);
    QCOMPARE(t.limitState(), LimitState::Unknown);
    QCOMPARE(t.readoutColor(), QColor(240, 244, 248));
    QCOMPARE(t.property("limitState").value<LimitState>(), LimitState::Unknown);
    // Neither the range nor the limits give a value
    QSignalSpy valueSpy(&t, &TankDisplay::valueChanged);
    QSignalSpy stateSpy(&t, &TankDisplay::valueStateChanged);
    QSignalSpy limitSpy(&t, &TankDisplay::limitStateChanged);
    t.setRange(10.0, 50.0);
    QVERIFY(t.setLimits(15.0, 20.0, 40.0, 45.0));
    QVERIFY(std::isnan(t.value()));
    QCOMPARE(t.valueState(), ValueState::Invalid);
    QCOMPARE(t.limitState(), LimitState::Unknown);
    QVERIFY(valueSpy.isEmpty() && stateSpy.isEmpty() && limitSpy.isEmpty());
    // A NaN input changes nothing
    t.setValue(kNaN);
    QVERIFY(valueSpy.isEmpty() && stateSpy.isEmpty() && limitSpy.isEmpty());
    // The first input is evaluated afresh: here it is below the low-low limit
    t.setValue(12.0);
    QCOMPARE(t.value(), 12.0);
    QCOMPARE(t.valueState(), ValueState::Valid);
    QCOMPARE(t.limitState(), LimitState::LowLow);
    QCOMPARE(valueSpy.size(), 1);
    QCOMPARE(stateSpy.size(), 1);
    QCOMPARE(limitSpy.size(), 1);
}

// A new TankDisplay is drawn as one whose input is NaN: hatched, "---", no limit in force; not as an empty vessel
void tst_TankDisplay::newDisplayIsDrawnWithoutAValue()
{
    TankDisplay fresh;
    prepare(fresh);
    QVERIFY(fresh.setLimits(5.0, 15.0, 85.0, 95.0));
    TankDisplay nan;
    prepare(nan);
    QVERIFY(nan.setLimits(5.0, 15.0, 85.0, 95.0));
    nan.setValue(2.0);
    nan.setValue(kNaN);
    TankDisplay zero;
    prepare(zero);
    QVERIFY(zero.setLimits(5.0, 15.0, 85.0, 95.0));
    zero.setValue(0.0);
    const QImage a = render(fresh);
    QCOMPARE(a, render(nan));
    QVERIFY(a != render(zero));
}

void tst_TankDisplay::valueStateAndRawValue()
{
    TankDisplay t;
    struct Step {
        double input;
        ValueState state;
        double value;
    };
    const Step steps[] = {
        {50.0, ValueState::Valid, 50.0},      {150.0, ValueState::Overrange, 100.0},
        {kInf, ValueState::Overrange, 100.0}, {-20.0, ValueState::Underrange, 0.0},
        {-kInf, ValueState::Underrange, 0.0}, {100.0, ValueState::Valid, 100.0},
        {kNaN, ValueState::Invalid, kNaN},    {75.0, ValueState::Valid, 75.0},
    };
    for (const Step &step : steps) {
        t.setValue(step.input);
        const QString where = QStringLiteral("input %1").arg(step.input);
        QVERIFY2(t.valueState() == step.state, qPrintable(where));
        if (std::isnan(step.value)) {
            QVERIFY2(std::isnan(t.value()) && std::isnan(t.rawValue()), qPrintable(where));
        } else {
            QVERIFY2(t.value() == step.value && t.rawValue() == step.input, qPrintable(where));
        }
    }
    // A range change classifies the last input again
    t.setValue(150.0);
    t.setRange(0.0, 200.0);
    QCOMPARE(t.value(), 150.0);
    QCOMPARE(t.valueState(), ValueState::Valid);
}

void tst_TankDisplay::signalOrder()
{
    TankDisplay t;
    QVERIFY(t.setLimits(kNaN, 10.0, 90.0, kNaN));
    t.setValue(50.0);
    QStringList events;
    connect(&t, &TankDisplay::valueChanged, &t, [&events](double v) { events << QStringLiteral("value %1").arg(v); });
    connect(&t, &TankDisplay::valueStateChanged, &t,
            [&events](ValueState s) { events << QStringLiteral("state %1").arg(static_cast<int>(s)); });
    connect(&t, &TankDisplay::limitStateChanged, &t,
            [&events](LimitState s) { events << QStringLiteral("limit ") + stateName(s); });

    t.setValue(kNaN);
    QCOMPARE(events, QStringList({QStringLiteral("value nan"), QStringLiteral("state 3"), QStringLiteral("limit Unknown")}));
    events.clear();
    t.setValue(kNaN);
    QVERIFY(events.isEmpty());
    t.setValue(150.0);
    QCOMPARE(events, QStringList({QStringLiteral("value 100"), QStringLiteral("state 2"), QStringLiteral("limit High")}));
    events.clear();
    t.setValue(160.0); // same value(), state and limit state
    QVERIFY(events.isEmpty());
    t.setValue(5.0);
    QCOMPARE(events, QStringList({QStringLiteral("value 5"), QStringLiteral("state 0"), QStringLiteral("limit Low")}));
}

void tst_TankDisplay::acceptedLimitOrders_data()
{
    QTest::addColumn<double>("lowLow");
    QTest::addColumn<double>("low");
    QTest::addColumn<double>("high");
    QTest::addColumn<double>("highHigh");
    QTest::newRow("all four") << 5.0 << 15.0 << 85.0 << 95.0;
    QTest::newRow("low-low equal to low") << 10.0 << 10.0 << 85.0 << 95.0;
    QTest::newRow("high equal to high-high") << 5.0 << 15.0 << 90.0 << 90.0;
    QTest::newRow("none") << kNaN << kNaN << kNaN << kNaN;
    QTest::newRow("high only") << kNaN << kNaN << 80.0 << kNaN;
    QTest::newRow("low-low and high-high") << 5.0 << kNaN << kNaN << 95.0;
    // The order applies only between enabled limits
    QTest::newRow("low disabled, low-low below high") << 40.0 << kNaN << 60.0 << kNaN;
    QTest::newRow("outside the range") << -50.0 << -10.0 << 120.0 << 150.0;
    QTest::newRow("negative") << -95.0 << -85.0 << -15.0 << -5.0;
}

void tst_TankDisplay::acceptedLimitOrders()
{
    QFETCH(double, lowLow);
    QFETCH(double, low);
    QFETCH(double, high);
    QFETCH(double, highHigh);
    TankDisplay t;
    QSignalSpy spy(&t, &TankDisplay::limitsChanged);
    QVERIFY(t.setLimits(lowLow, low, high, highHigh));
    const auto same = [](double a, double b) { return (std::isnan(a) && std::isnan(b)) || a == b; };
    QVERIFY(same(t.lowLowLimit(), lowLow) && same(t.lowLimit(), low) && same(t.highLimit(), high)
            && same(t.highHighLimit(), highHigh));
    const bool anyEnabled = !std::isnan(lowLow) || !std::isnan(low) || !std::isnan(high) || !std::isnan(highHigh);
    QCOMPARE(spy.count(), anyEnabled ? 1 : 0);
    // The same limits again: accepted, no signal
    QVERIFY(t.setLimits(lowLow, low, high, highHigh));
    QCOMPARE(spy.count(), anyEnabled ? 1 : 0);
}

void tst_TankDisplay::rejectedLimitOrders_data()
{
    QTest::addColumn<double>("lowLow");
    QTest::addColumn<double>("low");
    QTest::addColumn<double>("high");
    QTest::addColumn<double>("highHigh");
    QTest::newRow("low-low above low") << 20.0 << 15.0 << 85.0 << 95.0;
    QTest::newRow("low equal to high") << 5.0 << 50.0 << 50.0 << 95.0;
    QTest::newRow("low above high") << 5.0 << 60.0 << 50.0 << 95.0;
    QTest::newRow("high above high-high") << 5.0 << 15.0 << 96.0 << 95.0;
    QTest::newRow("low disabled, low-low equal to high") << 50.0 << kNaN << 50.0 << kNaN;
    QTest::newRow("low and high disabled, low-low above high-high") << 60.0 << kNaN << kNaN << 50.0;
    QTest::newRow("high disabled, low above high-high") << kNaN << 96.0 << kNaN << 95.0;
    QTest::newRow("+Inf high-high") << 5.0 << 15.0 << 85.0 << kInf;
    QTest::newRow("-Inf low-low") << -kInf << 15.0 << 85.0 << 95.0;
}

// The call is rejected and changes nothing: the limits, the limit state, no signal; the single setters too
void tst_TankDisplay::rejectedLimitOrders()
{
    QFETCH(double, lowLow);
    QFETCH(double, low);
    QFETCH(double, high);
    QFETCH(double, highHigh);
    TankDisplay t;
    QVERIFY(t.setLimits(1.0, 2.0, 98.0, 99.0));
    t.setValue(98.5);
    QCOMPARE(t.limitState(), LimitState::High);
    QSignalSpy limits(&t, &TankDisplay::limitsChanged);
    QSignalSpy state(&t, &TankDisplay::limitStateChanged);
    QSignalSpy appearance(&t, &TankDisplay::appearanceChanged);
    const auto unchanged = [&]() {
        return t.lowLowLimit() == 1.0 && t.lowLimit() == 2.0 && t.highLimit() == 98.0 && t.highHighLimit() == 99.0
            && t.limitState() == LimitState::High && limits.isEmpty() && state.isEmpty() && appearance.isEmpty();
    };
    QVERIFY(!t.setLimits(lowLow, low, high, highHigh));
    QVERIFY(unchanged());

    // One limit out of order through its own setter, with the others in place: a warning names the limit and why
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setLowLowLimit(3) rejected: lowLowLimit must be <= lowLimit (2); "
                                       "the limits are unchanged");
    t.setLowLowLimit(3.0);
    QVERIFY(unchanged());
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setLowLimit(98) rejected: lowLimit must be < highLimit (98); "
                                       "the limits are unchanged");
    t.setLowLimit(98.0);
    QVERIFY(unchanged());
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setHighLimit(2) rejected: highLimit must be > lowLimit (2); "
                                       "the limits are unchanged");
    t.setHighLimit(2.0);
    QVERIFY(unchanged());
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setHighHighLimit(97) rejected: highHighLimit must be >= highLimit "
                                       "(98); the limits are unchanged");
    t.setHighHighLimit(97.0);
    QVERIFY(unchanged());
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setHighLimit(inf) rejected: a limit cannot be infinite; the limits "
                                       "are unchanged");
    t.setHighLimit(kInf);
    QVERIFY(unchanged());
    // The property writes through the same setter
    QTest::ignoreMessage(QtWarningMsg, "TankDisplay::setHighLimit(2) rejected: highLimit must be > lowLimit (2); "
                                       "the limits are unchanged");
    QVERIFY(t.setProperty("highLimit", 2.0));
    QVERIFY(unchanged());
}

void tst_TankDisplay::limitCrossings_data()
{
    QTest::addColumn<QVector<double>>("limits"); // lowLow, low, high, highHigh
    QTest::addColumn<double>("hysteresis");
    QTest::addColumn<QVector<double>>("inputs");
    QTest::addColumn<QStringList>("states");

    const QVector<double> four{10.0, 20.0, 80.0, 90.0};
    QTest::newRow("thresholds included, no hysteresis")
        << four << 0.0 << QVector<double>{50.0, 79.999, 80.0, 89.999, 90.0, 80.0, 79.999, 20.001, 20.0, 10.001, 10.0, 20.0, 50.0}
        << QStringList{"Normal", "Normal", "High", "High", "HighHigh", "High", "Normal", "Normal", "Low", "Low", "LowLow", "Low", "Normal"};
    QTest::newRow("hysteresis on leaving, not on entering")
        << four << 2.0 << QVector<double>{79.0, 80.0, 79.0, 78.0, 77.999, 79.999, 80.0, 90.0, 88.0, 87.999, 77.0}
        << QStringList{"Normal", "High", "High", "High", "Normal", "Normal", "High", "HighHigh", "HighHigh", "High", "Normal"};
    QTest::newRow("hysteresis on the low side")
        << four << 2.0 << QVector<double>{21.0, 20.0, 21.0, 22.0, 22.001, 10.0, 12.0, 12.001, 22.001}
        << QStringList{"Normal", "Low", "Low", "Low", "Normal", "LowLow", "LowLow", "Low", "Normal"};
    QTest::newRow("NaN gives Unknown, the next input is evaluated afresh")
        << four << 5.0 << QVector<double>{85.0, kNaN, 78.0, 85.0, kNaN, 80.0}
        << QStringList{"High", "Unknown", "Normal", "High", "Unknown", "High"};
    QTest::newRow("infinities") << four << 0.0 << QVector<double>{kInf, -kInf, kNaN, 50.0}
                                << QStringList{"HighHigh", "LowLow", "Unknown", "Normal"};
    QTest::newRow("infinities, high and low only") << QVector<double>{kNaN, 20.0, 80.0, kNaN} << 0.0
                                                   << QVector<double>{kInf, -kInf} << QStringList{"High", "Low"};
    QTest::newRow("no limit") << QVector<double>{kNaN, kNaN, kNaN, kNaN} << 0.0 << QVector<double>{kInf, -kInf, 0.0, kNaN}
                              << QStringList{"Normal", "Normal", "Normal", "Unknown"};
    // The input, not the value shown: 150 on 0-100 is above a high-high limit of 120
    QTest::newRow("limits outside the range") << QVector<double>{-20.0, -10.0, 110.0, 120.0} << 0.0
                                              << QVector<double>{150.0, 115.0, 100.0, -15.0, -30.0}
                                              << QStringList{"HighHigh", "High", "Normal", "Low", "LowLow"};
    // A jump from one side to the other: the limit entered wins over the one held by the hysteresis
    QTest::newRow("jump across with a wide hysteresis") << QVector<double>{kNaN, 40.0, 50.0, kNaN} << 30.0
                                                        << QVector<double>{50.0, 40.0, 45.0, 50.0}
                                                        << QStringList{"High", "Low", "Low", "High"};
}

void tst_TankDisplay::limitCrossings()
{
    QFETCH(QVector<double>, limits);
    QFETCH(double, hysteresis);
    QFETCH(QVector<double>, inputs);
    QFETCH(QStringList, states);
    TankDisplay t;
    // The sequences start from no input in force: NaN, so that the first input is evaluated afresh (the initial
    // value 0 is an input, below the low limits of most rows)
    t.setValue(kNaN);
    QVERIFY(t.setLimits(limits[0], limits[1], limits[2], limits[3]));
    QCOMPARE(t.limitState(), LimitState::Unknown);
    t.setLimitHysteresis(hysteresis);
    QCOMPARE(t.limitHysteresis(), hysteresis);
    QSignalSpy spy(&t, &TankDisplay::limitStateChanged);
    LimitState before = t.limitState();
    int expectedSignals = 0;
    for (int i = 0; i < inputs.size(); ++i) {
        t.setValue(inputs[i]);
        const QString got = stateName(t.limitState());
        QVERIFY2(got == states[i], qPrintable(QStringLiteral("input %1 (step %2): %3, expected %4")
                                                  .arg(inputs[i]).arg(i).arg(got, states[i])));
        expectedSignals += t.limitState() != before ? 1 : 0;
        before = t.limitState();
    }
    QCOMPARE(spy.count(), expectedSignals);
}

// A change of the limits or of the hysteresis evaluates the last input again
void tst_TankDisplay::limitStateFollowsLimitAndHysteresisChanges()
{
    TankDisplay t;
    t.setValue(85.0);
    QCOMPARE(t.limitState(), LimitState::Normal);
    QSignalSpy spy(&t, &TankDisplay::limitStateChanged);
    QVERIFY(t.setLimits(kNaN, kNaN, 80.0, kNaN));
    QCOMPARE(t.limitState(), LimitState::High);
    QCOMPARE(spy.count(), 1);
    t.setHighHighLimit(85.0);
    QCOMPARE(t.limitState(), LimitState::HighHigh);
    // High-high moved above the input, by less than the hysteresis: still left, as there is none yet
    t.setHighHighLimit(86.0);
    QCOMPARE(t.limitState(), LimitState::High);
    t.setLimitHysteresis(3.0);
    t.setHighHighLimit(85.0);
    QCOMPARE(t.limitState(), LimitState::HighHigh);
    t.setHighHighLimit(87.0); // 85 >= 87 - 3: held
    QCOMPARE(t.limitState(), LimitState::HighHigh);
    t.setLimitHysteresis(1.0); // 85 < 87 - 1: left
    QCOMPARE(t.limitState(), LimitState::High);
    QVERIFY(t.setLimits(kNaN, kNaN, kNaN, kNaN));
    QCOMPARE(t.limitState(), LimitState::Normal);
    QCOMPARE(spy.count(), 6);
}

void tst_TankDisplay::fillHeight_data()
{
    QTest::addColumn<int>("shape");
    QTest::addColumn<double>("input");
    for (int s = 0; s < 4; ++s) {
        for (double v : {0.0, 10.0, 37.5, 50.0, 82.0, 100.0, 150.0, -kInf}) {
            QTest::newRow(qPrintable(QStringLiteral("shape %1, input %2").arg(s).arg(v))) << s << v;
        }
    }
}

// In the middle column of the vessel the fill reaches the height of the value on a linear scale from the bottom of
// the vessel (minimum) to its top (maximum): 1.5 px below it the content colour, 1.5 px above it the empty inside
void tst_TankDisplay::fillHeight()
{
    QFETCH(int, shape);
    QFETCH(double, input);
    TankDisplay t;
    t.setShape(static_cast<TankShape>(shape));
    prepare(t, sizeFor(static_cast<TankShape>(shape)));
    t.setRange(-20.0, 180.0); // 0..100 shown as 10..60 % of the height, the ends of the scale at -20 and 180
    t.setValue(input);
    const QImage img = render(t);
    const QRectF v = layoutOf(t).vessel;
    QVERIFY(v.height() > 50.0);
    const double shown = std::clamp(input, -20.0, 180.0);
    const double surface = v.bottom() - (shown + 20.0) / 200.0 * v.height();
    const double x = v.center().x() + 0.25;
    const QString where = QStringLiteral("surface at y %1 in %2..%3").arg(surface).arg(v.top()).arg(v.bottom());
    if (surface < v.bottom() - 3.0) {
        const QRgb below = at(img, x, surface + 1.5);
        QVERIFY2(near(below, kContent), qPrintable(where + QStringLiteral(": below it ") + rgb(below)));
        const QRgb bottom = at(img, x, v.bottom() - 3.0);
        QVERIFY2(near(bottom, kContent), qPrintable(where + QStringLiteral(": at the bottom ") + rgb(bottom)));
    } else {
        // Empty: no content at the bottom
        QVERIFY2(!near(at(img, x, v.bottom() - 3.0), kContent, 60), qPrintable(where));
    }
    if (surface >= v.bottom() - 3.0) {
        const QRgb inside = at(img, x, v.bottom() - 4.0);
        QVERIFY2(near(inside, kInterior), qPrintable(where + QStringLiteral(": empty, at the bottom ") + rgb(inside)));
    } else if (surface > v.top() + 3.0) {
        const QRgb above = at(img, x, surface - 1.5);
        QVERIFY2(near(above, kInterior), qPrintable(where + QStringLiteral(": above it ") + rgb(above)));
        const QRgb top = at(img, x, v.top() + 3.0);
        QVERIFY2(near(top, kInterior), qPrintable(where + QStringLiteral(": at the top ") + rgb(top)));
    }
}

void tst_TankDisplay::outlineAtItsGeometricPoints_data()
{
    addShapes();
}

// A full vessel: at a few heights the content reaches 2.5 px inside the edge of the outline, and 2.5 px outside it
// is the background (the wall is 2 px wide, centred on the edge). The edges are computed here from the shape
void tst_TankDisplay::outlineAtItsGeometricPoints()
{
    QFETCH(int, shape);
    const TankShape s = static_cast<TankShape>(shape);
    TankDisplay t;
    t.setShape(s);
    prepare(t, sizeFor(s));
    t.setValue(100.0);
    const QImage img = render(t);
    const QRectF r = layoutOf(t).vessel;
    const double cx = r.center().x();
    // A point of the right half of the outline and its outward normal; the left half is its mirror image
    struct Edge {
        QPointF point;
        QPointF normal;
    };
    const auto unit = [](double x, double y) {
        const double n = std::hypot(x, y);
        return QPointF(x / n, y / n);
    };
    QVector<Edge> edges;
    switch (s) {
    case TankShape::Sphere: {
        QVERIFY(qFuzzyCompare(r.width(), r.height()));
        const double radius = r.width() * 0.5;
        for (double f : {-0.6, 0.0, 0.4, 0.8}) {
            const double c = std::sqrt(1.0 - f * f);
            edges << Edge{QPointF(cx + radius * c, r.center().y() + radius * f), QPointF(c, f)};
        }
        break;
    }
    case TankShape::Hopper: {
        const double coneY = r.top() + 0.6 * r.height();
        const double outlet = 0.12 * r.width();
        edges << Edge{QPointF(r.right(), r.top() + 0.35 * r.height()), QPointF(1.0, 0.0)};
        // Straight cone from the full width at 0.6 of the height to the outlet at the bottom
        const double dx = r.width() * 0.5 - outlet;
        const double dy = r.bottom() - coneY;
        for (double f : {0.2, 0.5, 0.8}) {
            edges << Edge{QPointF(r.right() - f * dx, coneY + f * dy), unit(dy, dx)};
        }
        break;
    }
    case TankShape::VerticalCylinder:
        QVERIFY(r.width() <= 0.6 * r.height() + 1e-9);
        edges << Edge{QPointF(r.right(), r.top() + 0.3 * r.height()), QPointF(1.0, 0.0)}
              << Edge{QPointF(r.right(), r.top() + 0.7 * r.height()), QPointF(1.0, 0.0)};
        break;
    case TankShape::HorizontalCylinder: {
        QVERIFY(r.height() <= 0.55 * r.width() + 1e-9);
        // The ends: half ellipses of semi-axes e and h / 2
        const double e = std::min(0.15 * r.width(), 0.25 * r.height());
        const double b = r.height() * 0.5;
        for (double f : {0.0, 0.5, -0.8}) {
            const double c = std::sqrt(1.0 - f * f);
            edges << Edge{QPointF(r.right() - e + e * c, r.center().y() + b * f), unit(c / e, f / b)};
        }
        break;
    }
    }
    for (const Edge &e : edges) {
        for (double side : {-1.0, 1.0}) {
            const QPointF point(cx + side * (e.point.x() - cx), e.point.y());
            const QPointF normal(side * e.normal.x(), e.normal.y());
            const QPointF in = point - normal * 2.5;
            const QPointF out = point + normal * 2.5;
            const QRgb inside = at(img, in.x(), in.y());
            const QRgb outside = at(img, out.x(), out.y());
            const QString where = QStringLiteral("edge at (%1, %2), side %3: inside %4, outside %5")
                                      .arg(point.x()).arg(point.y()).arg(side).arg(rgb(inside), rgb(outside));
            QVERIFY2(near(inside, kContent) && near(outside, kBackground), qPrintable(where));
        }
    }
}

void tst_TankDisplay::granularFillHasGrainsOnlyBelowTheSurface()
{
    // Grains on a 4 px grid: four surfaces about 1 px apart put a row of grains at every distance below them
    for (double level : {50.0, 50.6, 51.2, 51.8}) {
        TankDisplay t;
        prepare(t);
        t.setContent(TankDisplay::Content::Granular);
        t.setValue(level);
        const QImage img = render(t);
        const QRectF v = layoutOf(t).vessel;
        const double surface = v.bottom() - level / 100.0 * v.height();
        const QString where = QStringLiteral("level %1, surface at y %2").arg(level).arg(surface);
        // The middle half of the width, clear of the wall and of the dished heads (0.15 of the width deep)
        const QRectF below(v.left() + v.width() * 0.25, surface + 3.0, v.width() * 0.5,
                           v.bottom() - v.width() * 0.2 - surface - 3.0);
        const QRectF above(v.left() + v.width() * 0.25, v.top() + v.width() * 0.2, v.width() * 0.5,
                           surface - 1.0 - v.top() - v.width() * 0.2);
        const int all = countIn(img, below, [](QRgb) { return true; });
        const int dark = countIn(img, below, [](QRgb p) { return near(p, kContent.darker(145), 30); });
        const int light = countIn(img, below, [](QRgb p) { return near(p, kContent.lighter(130), 30); });
        const int plain = countIn(img, below, [](QRgb p) { return near(p, kContent); });
        QVERIFY2(dark > all / 20 && light > 0 && plain > all / 4,
                 qPrintable(where + QStringLiteral(": %1 pixels, %2 dark grains, %3 light, %4 plain content")
                                        .arg(all).arg(dark).arg(light).arg(plain)));
        // Above the surface (up to 1 px from it, clear of its antialiased edge) only the empty inside
        const int aboveNotEmpty = countIn(img, above, [](QRgb p) { return !near(p, kInterior); });
        QVERIFY2(aboveNotEmpty == 0, qPrintable(where + QStringLiteral(": %1 pixels above the surface").arg(aboveNotEmpty)));
        // The liquid has no grains
        t.setContent(TankDisplay::Content::Liquid);
        const QImage liquid = render(t);
        QCOMPARE(countIn(liquid, below, [](QRgb p) { return !near(p, kContent); }), 0);
    }
}

// The scale labels at their values: the minimum at the bottom of the vessel, the maximum at its top. With a range of
// 0 to 100000 the bottom label "0" is much narrower than the top one, "100000"
void tst_TankDisplay::scaleLabelsAtTheirValues()
{
    TankDisplay t;
    prepare(t, QSize(200, 300));
    t.setRange(0.0, 100000.0);
    t.setPrecision(0);
    t.setTextColor(QColor(255, 255, 0, 0));
    const QImage without = render(t);
    t.setTextColor(kText);
    const QImage with = render(t);
    const internal::TankLayout l = layoutOf(t);
    const double dpr = with.devicePixelRatio();
    const QFontMetricsF lm([&] {
        QFont f = t.font();
        f.setBold(true);
        f.setPixelSize(std::max(internal::minimumReadoutPixelSize(t.logicalDpiY()), 10));
        return f;
    }());
    // Width of the label ink in a band of one line height around a height
    const auto inkWidth = [&](double y) {
        const QRect band = QRectF(QPointF(l.labelX - 1.0, y - lm.height() * 0.5) * dpr,
                                  QSizeF(t.width() - l.labelX + 1.0, lm.height()) * dpr).toAlignedRect().intersected(with.rect());
        int left = std::numeric_limits<int>::max();
        int right = -1;
        for (int py = band.top(); py <= band.bottom(); ++py) {
            for (int px = band.left(); px <= band.right(); ++px) {
                if (with.pixel(px, py) != without.pixel(px, py)) {
                    left = std::min(left, px);
                    right = std::max(right, px);
                }
            }
        }
        return right < 0 ? 0 : right - left + 1;
    };
    const int bottom = inkWidth(l.vessel.bottom());
    const int top = inkWidth(l.vessel.top());
    QVERIFY2(bottom > 0 && top > 3 * bottom,
             qPrintable(QStringLiteral("label ink %1 px wide at the bottom (\"0\"), %2 px at the top (\"100000\")").arg(bottom).arg(top)));
}

void tst_TankDisplay::limitMarkers_data()
{
    addShapes();
}

// Each limit inside the range has a dashed line across the vessel and a marker on its left at its height. The
// marker of the limit in force is filled and tagged; the others are outlined and untagged. A limit outside the
// range has none
void tst_TankDisplay::limitMarkers()
{
    QFETCH(int, shape);
    const TankShape s = static_cast<TankShape>(shape);
    struct Case {
        double input;
        int inForce; // index in the limits, -1 for none
    };
    const double limits[4] = {10.0, 30.0, 60.0, 85.0};
    const char *const tags[4] = {"LL", "L", "H", "HH"};
    const auto isInk = [](QRgb p) { return qRed(p) == 255; };
    for (const Case &c : {Case{50.0, -1}, Case{75.0, 2}, Case{95.0, 3}, Case{20.0, 1}, Case{5.0, 0}, Case{kNaN, -1}}) {
        TankDisplay t;
        t.setShape(s);
        prepare(t, sizeFor(s));
        t.setRange(0.0, 200.0); // the limits in the lower half of the vessel
        QVERIFY(t.setLimits(limits[0], limits[1], limits[2], limits[3]));
        t.setValue(c.input);
        const QImage img = render(t);
        const QImage limitInk = inkOf(t, &TankDisplay::setLimitColor, kLimit);
        const QImage textInk = inkOf(t, &TankDisplay::setTextColor, kText);
        const internal::TankLayout l = layoutOf(t);
        const QRectF v = l.vessel;
        for (int i = 0; i < 4; ++i) {
            const double y = v.bottom() - limits[i] / 200.0 * v.height();
            const QString where = QStringLiteral("input %1, limit %2 at y %3").arg(c.input).arg(QLatin1String(tags[i])).arg(y);
            const double tip = l.markerTipX;
            const double size = internal::kTankMarkerSize;
            // Centroid of the triangle: filled only when in force
            const QRgb centroid = at(img, tip - size * 2.0 / 3.0, y);
            const bool filled = near(centroid, kLimit, 40);
            QVERIFY2(filled == (i == c.inForce), qPrintable(where + QStringLiteral(": centroid ") + rgb(centroid)));
            // The outline is there in both cases
            const int marker = countIn(limitInk, QRectF(tip - size - 1.0, y - size * 0.5 - 1.0, size + 2.0, size + 2.0), isInk);
            QVERIFY2(marker >= 6, qPrintable(where + QStringLiteral(": %1 marker pixels").arg(marker)));
            // The dashed line across the middle of the vessel, within half a pixel of its height
            const int line = countIn(limitInk, QRectF(v.center().x() - v.width() * 0.2, y - 0.5, v.width() * 0.4, 1.0), isInk);
            QVERIFY2(line > 0, qPrintable(where + QStringLiteral(": no limit line")));
            // The tag left of the marker, in the text colour, within 3 px of the height of the limit
            const int tag = countIn(textInk, QRectF(0.0, y - 3.0, l.tagRight + 1.0, 6.0), isInk);
            QVERIFY2((tag > 0) == (i == c.inForce), qPrintable(where + QStringLiteral(": %1 tag pixels").arg(tag)));
        }
        // Nothing else in the column of the markers and tags: nothing above the top limit
        const double topLimitY = v.bottom() - limits[3] / 200.0 * v.height();
        const QRectF column(0.0, 0.0, l.markerTipX + 1.0, topLimitY - 8.0);
        const int stray = countIn(limitInk, column, isInk) + countIn(textInk, column, isInk);
        QVERIFY2(stray == 0, qPrintable(QStringLiteral("input %1: %2 marker or tag pixels above the top limit").arg(c.input).arg(stray)));
    }
    // A limit outside the range that is not in force has no marker
    TankDisplay t;
    t.setShape(s);
    prepare(t, sizeFor(s));
    QVERIFY(t.setLimits(-20.0, kNaN, kNaN, 250.0));
    t.setValue(50.0);
    QCOMPARE(t.limitState(), LimitState::Normal);
    const QImage limitInk = inkOf(t, &TankDisplay::setLimitColor, kLimit);
    QCOMPARE(countIn(limitInk, QRectF(0.0, 0.0, t.width(), t.height()), isInk), 0);
}

// A limit line covers whole device rows: 1 logical px rounded to device pixels (at least 1), each row in the full
// limit colour, with nothing spread on the rows next to it. Limits at many fractions of a pixel
void tst_TankDisplay::limitLinesOnWholeDevicePixels()
{
    TankDisplay t;
    prepare(t);
    t.setValue(5.0);
    for (int k = 0; k < 16; ++k) {
        const double limit = 40.0 + k * 0.137;
        QVERIFY(t.setLimits(kNaN, kNaN, limit, kNaN));
        const QImage img = render(t);
        const double dpr = img.devicePixelRatio();
        const int rows = std::max(1, static_cast<int>(std::lround(dpr)));
        const QRectF v = layoutOf(t).vessel;
        const double y = v.bottom() - limit / 100.0 * v.height();
        const int top = static_cast<int>(std::floor((y - 3.0) * dpr));
        const int bottom = static_cast<int>(std::ceil((y + 3.0) * dpr));
        int columns = 0;
        int clean = 0;
        for (int px = static_cast<int>((v.left() + v.width() * 0.3) * dpr); px < (v.left() + v.width() * 0.7) * dpr; ++px) {
            int marked = 0;
            int full = 0;
            for (int py = top; py <= bottom; ++py) {
                const QRgb p = img.pixel(px, py);
                marked += near(p, kInterior) ? 0 : 1;
                full += near(p, kLimit) ? 1 : 0;
            }
            if (marked == 0) {
                continue; // a gap of the dashes
            }
            ++columns;
            // Every column of the line on exactly its rows; only the ends of a dash may be partly covered
            QVERIFY2(marked == rows,
                     qPrintable(QStringLiteral("limit %1 at y %2, column %3: %4 rows marked, %5 expected")
                                    .arg(limit).arg(y).arg(px).arg(marked).arg(rows)));
            clean += full == rows ? 1 : 0;
        }
        QVERIFY2(columns > 10 && 2 * clean >= columns,
                 qPrintable(QStringLiteral("limit %1 at y %2: %3 columns of line, %4 in the full limit colour")
                                .arg(limit).arg(y).arg(columns).arg(clean)));
    }
}

void tst_TankDisplay::everyActiveLimitStateIsVisible_data()
{
    addShapes();
}

// Every limit state in force is drawn: the filled marker and its tag at the limit, or at the end of the scale with
// the "beyond" sign past it when the limit lies outside the range. Limits inside, outside and both
void tst_TankDisplay::everyActiveLimitStateIsVisible()
{
    QFETCH(int, shape);
    const TankShape s = static_cast<TankShape>(shape);
    struct Config {
        const char *name;
        double minimum, maximum;
        std::array<double, 4> limits;
        std::array<double, 4> inputs; // one in each limit state: LL, L, H, HH
    };
    const Config configs[] = {
        {"inside", 0.0, 100.0, {{10.0, 30.0, 70.0, 90.0}}, {{5.0, 20.0, 80.0, 95.0}}},
        {"outside", 20.0, 80.0, {{5.0, 10.0, 90.0, 95.0}}, {{3.0, 7.0, 92.0, 97.0}}},
        // The limits inside far enough from the ends that their markers stay clear of the "beyond" signs
        {"both", 0.0, 100.0, {{-10.0, 30.0, 70.0, 120.0}}, {{-20.0, 20.0, 80.0, 130.0}}},
    };
    const LimitState states[4] = {LimitState::LowLow, LimitState::Low, LimitState::High, LimitState::HighHigh};
    const auto isInk = [](QRgb p) { return qRed(p) == 255; };
    // Five heights: the markers at different fractions of a pixel (the outline of a marker must not count as a sign)
    for (int extra = 0; extra < 5; ++extra)
    for (const Config &c : configs) {
        for (int i = 0; i < 4; ++i) {
            TankDisplay t;
            t.setShape(s);
            prepare(t, sizeFor(s) + QSize(0, extra));
            t.setRange(c.minimum, c.maximum);
            QVERIFY(t.setLimits(c.limits[0], c.limits[1], c.limits[2], c.limits[3]));
            t.setValue(c.inputs[static_cast<size_t>(i)]);
            QCOMPARE(t.limitState(), states[i]);
            const QImage img = render(t);
            const QImage limitInk = inkOf(t, &TankDisplay::setLimitColor, kLimit);
            const QImage textInk = inkOf(t, &TankDisplay::setTextColor, kText);
            const internal::TankLayout l = layoutOf(t);
            const QRectF v = l.vessel;
            const double limit = c.limits[static_cast<size_t>(i)];
            const bool below = limit < c.minimum;
            const bool above = limit > c.maximum;
            const double y = below ? v.bottom() : above ? v.top() : v.bottom() - (limit - c.minimum) / (c.maximum - c.minimum) * v.height();
            const QString where = QStringLiteral("%1, %2 (limit %3) at y %4, height %5")
                                      .arg(QLatin1String(c.name), stateName(states[i])).arg(limit).arg(y).arg(t.height());
            const double tip = l.markerTipX;
            const double size = internal::kTankMarkerSize;
            const QRgb centroid = at(img, tip - size * 2.0 / 3.0, y);
            QVERIFY2(near(centroid, kLimit, 40), qPrintable(where + QStringLiteral(": centroid ") + rgb(centroid)));
            // The tag, centred on the marker: within half a label line of it (less than 10 px with these fonts)
            const int tag = countIn(textInk, QRectF(0.0, y - 10.0, l.tagRight + 1.0, 20.0), isInk);
            const int tagCore = countIn(textInk, QRectF(0.0, y - 3.0, l.tagRight + 1.0, 6.0), isInk);
            QVERIFY2(tagCore > 0, qPrintable(where + QStringLiteral(": no tag")));
            // The "beyond" sign: past the marker, away from the scale; on neither side for a limit inside
            // The vertical reach of the marker as drawn: half its height, plus its outline. The outline is a pen of
            // kTankMarkerPen with bevel joins: at the corners of the base the outer edge of the slanted side ends
            // (pen / 2) · cos(slope) past the corner, the slope of the side being atan(half / size)
            const double half = size * 0.5;
            const double markerReach = half + internal::kTankMarkerPen * 0.5 * size / std::hypot(size, half);
            const double signEnd = half + internal::kTankBeyondGap + internal::kTankBeyondSign;
            // The area of the sign: the device pixels of the marker column wholly outside the reach of the marker,
            // up to one pixel past the end of the sign
            const double dpr = img.devicePixelRatio();
            const auto signInk = [&](double dir) {
                int n = 0;
                const int x0 = static_cast<int>(std::floor((tip - size) * dpr));
                const int x1 = static_cast<int>(std::ceil(tip * dpr));
                for (int py = 0; py < limitInk.height(); ++py) {
                    // Distance from the marker centre of the near and far edges of this row, on the side of dir
                    const double a = dir * ((py + (dir < 0 ? 1 : 0)) / dpr - y);
                    const double b = dir * ((py + (dir < 0 ? 0 : 1)) / dpr - y);
                    if (a < markerReach || b > signEnd + 1.0) {
                        continue;
                    }
                    for (int px = std::max(0, x0); px < std::min(limitInk.width(), x1); ++px) {
                        n += isInk(limitInk.pixel(px, py)) ? 1 : 0;
                    }
                }
                return n;
            };
            const int signAbove = signInk(-1.0);
            const int signBelow = signInk(1.0);
            QVERIFY2((above ? signAbove > 2 : signAbove == 0) && (below ? signBelow > 2 : signBelow == 0),
                     qPrintable(where + QStringLiteral(": %1 sign pixels above, %2 below").arg(signAbove).arg(signBelow)));
            const QRectF past(tip - size, y - signEnd, size, signEnd - half);
            const QRectF under(tip - size, y + half, size, signEnd - half);
            // Inside the widget
            QVERIFY2(past.top() >= 0.0 && under.bottom() <= t.height(), qPrintable(where));
            // One tag only: the other limits outside the range are not drawn, those inside are not tagged
            const int allTags = countIn(textInk, QRectF(0.0, 0.0, l.tagRight + 1.0, t.height()), isInk);
            QCOMPARE(allTags, tag);
        }
    }
}

void tst_TankDisplay::invalidVesselIsHatched_data()
{
    addShapes();
}

// #151: a NaN input leaves the vessel empty and hatched. The hatch colour is derived from the empty inside (its
// contrast is measured in tst_ReadoutContrast): here its pixels are those far from the inside colour
void tst_TankDisplay::invalidVesselIsHatched()
{
    QFETCH(int, shape);
    const TankShape s = static_cast<TankShape>(shape);
    TankDisplay t;
    t.setShape(s);
    prepare(t, sizeFor(s));
    t.setValue(60.0);
    t.setValue(kNaN);
    const QImage img = render(t);
    const QRectF v = layoutOf(t).vessel;
    const QPainterPath outline = internal::tankOutlinePath(static_cast<internal::TankOutline>(s), v);
    // Device pixels 3 logical px inside the outline in every direction (clear of the wall)
    QVector<QPoint> area;
    const double dpr = img.devicePixelRatio();
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QPointF p((x + 0.5) / dpr, (y + 0.5) / dpr);
            bool inside = true;
            for (const QPointF d : {QPointF(3, 0), QPointF(-3, 0), QPointF(0, 3), QPointF(0, -3), QPointF(2.2, 2.2),
                                    QPointF(-2.2, 2.2), QPointF(2.2, -2.2), QPointF(-2.2, -2.2)}) {
                inside = inside && outline.contains(p + d);
            }
            if (inside) {
                area.push_back(QPoint(x, y));
            }
        }
    }
    QVERIFY(area.size() > 500);
    // The pixels far from the empty inside, painted pure green for HatchTest
    const auto hatchMask = [](const QImage &from) {
        QImage mask(from.size(), QImage::Format_ARGB32);
        for (int y = 0; y < from.height(); ++y) {
            for (int x = 0; x < from.width(); ++x) {
                mask.setPixel(x, y, channelDistance(from.pixel(x, y), kInterior.rgb()) > 60 ? qRgb(0, 255, 0) : qRgb(0, 0, 0));
            }
        }
        return mask;
    };
    const QString error = HatchTest::hatchError(hatchMask(img), area);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    int content = 0;
    for (const QPoint &p : area) {
        content += near(img.pixel(p), kContent, 60) ? 1 : 0;
    }
    QCOMPARE(content, 0);
    // A valid input: no hatch
    t.setValue(60.0);
    const QImage valid = hatchMask(render(t));
    int hatch = 0;
    for (const QPoint &p : area) {
        // The empty inside above the fill: no hatch there
        const QPointF logical((p.x() + 0.5) / dpr, (p.y() + 0.5) / dpr);
        if (logical.y() < v.bottom() - 0.62 * v.height()) {
            hatch += HatchTest::isHatchPixel(valid.pixel(p)) ? 1 : 0;
        }
    }
    QCOMPARE(hatch, 0);
}

// #151 with the test font of tests/fonts: the readout of each state in the panel
void tst_TankDisplay::readoutStates()
{
    QString error;
    QFont font = ReadoutTest::readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // Twice the size (#51: the readout follows the widget font), so that each dash is several pixels wide
    font.setPixelSize(static_cast<int>(std::lround(2.0 * internal::effectivePixelSize(QApplication::font()))));
    const auto box = [](TankDisplay &t) {
        const QRectF pod = layoutOf(t).pod.adjusted(1.5, 1.5, -1.5, -1.5);
        const double dpr = t.grab().devicePixelRatio();
        return QRectF(pod.topLeft() * dpr, pod.size() * dpr).toRect();
    };
    struct Case {
        double input;
        int extraGroups; // the triangle
        bool up;
    };
    for (const Case &c : {Case{5.0, 1, true}, Case{-5.0, 1, false}}) {
        TankDisplay marked;
        marked.setFont(font);
        prepare(marked, QSize(240, 320));
        marked.setUnit(QString());
        marked.setRange(0.0, 1.0);
        marked.setPrecision(0);
        marked.setValue(c.input);
        const QImage mi = render(marked);
        TankDisplay plain;
        plain.setFont(font);
        prepare(plain, QSize(240, 320));
        plain.setUnit(QString());
        plain.setRange(std::min(0.0, c.input), std::max(1.0, c.input));
        plain.setPrecision(0);
        plain.setValue(c.input);
        const QImage pi = render(plain);
        const auto markedInk = ReadoutTest::readoutInkGroups(mi, box(marked));
        const auto plainInk = ReadoutTest::readoutInkGroups(pi, box(plain));
        const QString where = QStringLiteral("input %1").arg(c.input);
        QVERIFY2(!plainInk.isEmpty() && markedInk.size() == plainInk.size() + c.extraGroups,
                 qPrintable(where + QStringLiteral(": %1 ink groups, %2 for the valid text").arg(markedInk.size()).arg(plainInk.size())));
        const ReadoutTest::InkGroup &tri = markedInk.first();
        QVERIFY2(tri.top.size() >= 3, qPrintable(where));
        const int flat = c.up ? ReadoutTest::spread(tri.bottom) : ReadoutTest::spread(tri.top);
        const int pointed = c.up ? ReadoutTest::spread(tri.top) : ReadoutTest::spread(tri.bottom);
        QVERIFY2(flat <= 1 && pointed >= 2, qPrintable(where + QStringLiteral(": flat %1, pointed %2").arg(flat).arg(pointed)));
    }
    TankDisplay invalid;
    invalid.setFont(font);
    prepare(invalid, QSize(240, 320));
    invalid.setValue(kNaN);
    const QImage ii = render(invalid);
    const QString dashes = ReadoutTest::dashesError(ReadoutTest::readoutInkGroups(ii, box(invalid)));
    // Out of the font database again before the checks: without installed fonts the test font (digits only) would
    // stay the only font of the application in the tests that follow, and draw no limit tag
    QFontDatabase::removeAllApplicationFonts();
    QVERIFY2(dashes.isEmpty(), qPrintable(dashes));
}

void tst_TankDisplay::textsInsideTheWidgetFromTheMinimumUp_data()
{
    addShapes();
}

// From minimumSizeHint() up, with the high-high tag shown and a long readout: no text pixel on the outer pixel of
// the widget (a text cut by the edge would leave one), the end labels of the scale drawn at the ends of the vessel
void tst_TankDisplay::textsInsideTheWidgetFromTheMinimumUp()
{
    QFETCH(int, shape);
    const TankShape s = static_cast<TankShape>(shape);
    TankDisplay probe;
    probe.setShape(s);
    probe.setRange(0.0, 10000.0);
    probe.setPrecision(1);
    QVERIFY(probe.setLimits(500.0, 1000.0, 9000.0, 9500.0));
    const QSize min = probe.minimumSizeHint();
    for (int dw : {0, 1, 9, 40}) {
        for (int dh : {0, 1, 13, 50}) {
            TankDisplay t;
            t.setShape(s);
            prepare(t, min + QSize(dw, dh));
            t.setRange(0.0, 10000.0);
            t.setPrecision(1);
            QVERIFY(t.setLimits(500.0, 1000.0, 9000.0, 9500.0));
            t.setValue(9700.0);
            QCOMPARE(t.minimumSizeHint(), min);
            const QImage img = render(t);
            const QString where = QStringLiteral("%1x%2").arg(t.width()).arg(t.height());
            int edge = 0;
            for (int x = 0; x < img.width(); ++x) {
                edge += near(img.pixel(x, 0), kText, 120) + near(img.pixel(x, img.height() - 1), kText, 120);
            }
            for (int y = 0; y < img.height(); ++y) {
                edge += near(img.pixel(0, y), kText, 120) + near(img.pixel(img.width() - 1, y), kText, 120);
            }
            QVERIFY2(edge == 0, qPrintable(where + QStringLiteral(": %1 text pixels on the edge").arg(edge)));
            const internal::TankLayout l = layoutOf(t);
            for (double y : {l.vessel.top(), l.vessel.bottom()}) {
                const int label = countIn(img, QRectF(l.labelX, y - 6.0, t.width() - l.labelX, 12.0),
                                          [](QRgb p) { return near(p, kText, 90); });
                QVERIFY2(label > 0, qPrintable(where + QStringLiteral(": no end label at y %1").arg(y)));
            }
            const int tag = countIn(img, QRectF(0.0, 0.0, l.tagRight + 1.0, t.height()), [](QRgb p) { return near(p, kText, 90); });
            QVERIFY2(tag > 0, qPrintable(where + QStringLiteral(": no tag")));
        }
    }
}

void tst_TankDisplay::readoutNeverBelowSevenPoints_data()
{
    QTest::addColumn<int>("extraWidth");
    QTest::addColumn<double>("input");
    QTest::addColumn<QString>("unit");
    for (int dw : {0, 3, 10, 25, 60}) {
        // Up to 1e15: out-of-range inputs long enough to bring the readout down to 7 pt, or to leave the number out,
        // also with the system fonts
        for (double v : {42.5, 99999.9, 123456.7, 1e9, 1e12, 1e15, -1e15, -kInf, kInf, kNaN}) {
            for (const char *unit : {"", "m³/h"}) {
                QTest::newRow(qPrintable(QStringLiteral("+%1 px, %2 %3").arg(dw).arg(v).arg(QString::fromUtf8(unit))))
                    << dw << v << QString::fromUtf8(unit);
            }
        }
    }
}

// #60, #167: from the minimum width up the readout is never below 7 pt nor cut. When the number is shown its line is
// at least as wide as the number alone at 7 pt (the unit may be left out); otherwise only a valid readout may be
// missing, and an out-of-range one keeps a triangle not smaller than the 7 pt one. "---" is always whole
void tst_TankDisplay::readoutNeverBelowSevenPoints()
{
    QFETCH(int, extraWidth);
    QFETCH(double, input);
    QFETCH(QString, unit);
    TankDisplay t;
    t.setRange(0.0, 100000.0);
    t.setUnit(unit);
    const QSize min = t.minimumSizeHint();
    prepare(t, QSize(min.width() + extraWidth, min.height() + 40));
    t.setValue(input);
    const QVector<QPoint> ink = readoutInk(t);
    const ValueState state = t.valueState();
    const qreal dpr = t.grab().devicePixelRatio();
    // Inside the panel
    const QRectF pod = layoutOf(t).pod;
    const QRect podDevice = QRectF(pod.topLeft() * dpr, pod.size() * dpr).toAlignedRect();
    for (const QPoint &p : ink) {
        QVERIFY2(podDevice.contains(p), "readout ink outside the panel");
    }
    if (ink.isEmpty()) {
        QVERIFY2(state == ValueState::Valid, "only a valid readout may be left out");
        return;
    }
    const bool outOfRange = state == ValueState::Overrange || state == ValueState::Underrange;
    const QString number = state == ValueState::Invalid
        ? QStringLiteral("---")
        : (std::isinf(input) ? (input > 0 ? QStringLiteral(">100000.0") : QStringLiteral("<0.0"))
                             : QString::number(input, 'f', 1));
    const QSize full = sevenPointLineInk(number, outOfRange, t, dpr);
    const QRect line = boundsOf(ink);
    if (line.width() >= full.width() && line.height() >= full.height() - 1) {
        return;
    }
    QVERIFY2(outOfRange, qPrintable(QStringLiteral("readout %1 x %2 px, \"%3\" at 7 pt is %4 x %5 px")
                                        .arg(line.width()).arg(line.height()).arg(number).arg(full.width()).arg(full.height())));
    const QSize tri = sevenPointLineInk(QString(), true, t, dpr);
    QVERIFY2(line.width() >= tri.width() && line.height() >= tri.height(),
             qPrintable(QStringLiteral("readout %1 x %2 px: smaller than the 7 pt triangle (%3 x %4 px)")
                            .arg(line.width()).arg(line.height()).arg(tri.width()).arg(tri.height())));
}

// Without room the unit goes first: there is a width at which the number is shown without its unit (the same pixels
// as with no unit at all), and at a larger width the unit comes back
void tst_TankDisplay::unitGoesBeforeTheNumber()
{
    // A unit long enough not to fit with the number at 7 pt at the minimum width, also with the box glyphs drawn
    // without installed fonts (one em per character)
    const QString unit = QStringLiteral("m³ (15 °C)");
    for (double input : {87.5, 250.0}) {
        int droppedAt = -1;
        int shownAt = -1;
        TankDisplay sizing;
        sizing.setUnit(unit);
        const int minWidth = sizing.minimumSizeHint().width();
        for (int w = minWidth; w < minWidth + 200 && shownAt < 0; ++w) {
            TankDisplay withUnit;
            prepare(withUnit, QSize(w, 300));
            withUnit.setUnit(unit);
            withUnit.setValue(input);
            TankDisplay without;
            prepare(without, QSize(w, 300));
            without.setUnit(QString());
            without.setValue(input);
            const QVector<QPoint> a = readoutInk(withUnit);
            const QVector<QPoint> b = readoutInk(without);
            if (!a.isEmpty() && a == b && droppedAt < 0) {
                droppedAt = w;
            } else if (!a.isEmpty() && a != b && droppedAt >= 0) {
                shownAt = w;
            }
        }
        QVERIFY2(droppedAt >= 0, qPrintable(QStringLiteral("input %1: no width shows the number without the unit").arg(input)));
        QVERIFY2(shownAt > droppedAt, qPrintable(QStringLiteral("input %1: the unit never comes back").arg(input)));
    }
}

// The minimum follows what it is made of, and a change of it reaches the layout (updateGeometry())
void tst_TankDisplay::minimumSizeHintFollowsTheConfiguration()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *t = new TankDisplay;
    layout->addWidget(t);
    ScreenChange::LayoutRequests requests;
    host.installEventFilter(&requests);
    host.show(); // a hidden widget does not tell its layout
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const QSize base = t->minimumSizeHint();
    const auto check = [&](const char *what, const std::function<void()> &change, bool wider, bool taller) {
        const QSize before = t->minimumSizeHint();
        QCoreApplication::processEvents();
        requests.count = 0;
        change();
        QCoreApplication::processEvents();
        const QSize after = t->minimumSizeHint();
        QVERIFY2((after.width() > before.width()) == wider && (after.height() > before.height()) == taller,
                 qPrintable(QStringLiteral("%1: %2x%3 -> %4x%5").arg(QLatin1String(what)).arg(before.width())
                                .arg(before.height()).arg(after.width()).arg(after.height())));
        QVERIFY2(requests.count > 0, qPrintable(QStringLiteral("%1: the layout was not told").arg(QLatin1String(what))));
    };
    // The markers add the column of the tags, and taller ends when half a label line is less than the room of a
    // marker at an end of the scale and of its "beyond" sign
    QFont label = t->font();
    label.setBold(true);
    label.setPixelSize(std::max(internal::minimumReadoutPixelSize(t->logicalDpiY()), 10));
    const bool endsGrow = QFontMetricsF(label).height() * 0.5
        < internal::kTankMarkerSize * 0.5 + internal::kTankBeyondGap + internal::kTankBeyondSign;
    check("limits enabled", [&] { QVERIFY(t->setLimits(10.0, 20.0, 80.0, 90.0)); }, true, endsGrow);
    check("wider labels", [&] { t->setRange(0.0, 100000.0); }, true, false);
    // Labels with decimals: precision 0 rounds them to whole numbers (scaleLabelText())
    t->setRange(0.0, 1.0);
    t->setPrecision(0);
    check("decimals", [&] { t->setPrecision(1); }, true, false);
    t->setRange(0.0, 100000.0);
    check("larger font", [&] { t->setFont(FontTest::doubledFont()); }, true, true);
    check("horizontal cylinder", [&] { t->setShape(TankShape::HorizontalCylinder); }, true, false);
    t->setFont(QFont());
    t->setShape(TankShape::VerticalCylinder);
    QVERIFY(t->setLimits(kNaN, kNaN, kNaN, kNaN));
    t->setRange(0.0, 100.0);
    t->setPrecision(1);
    QCOMPARE(t->minimumSizeHint(), base);
    // Hiding the readout or the scale makes it smaller
    t->setDigitalDisplayVisible(false);
    QVERIFY(t->minimumSizeHint().height() < base.height());
    t->setDigitalDisplayVisible(true);
    t->setScaleVisible(false);
    QVERIFY(t->minimumSizeHint().width() < base.width());
}

void tst_TankDisplay::screenChangeOfAVisibleTankDisplay_data()
{
    ScreenChange::addScreens();
}

// #177: moved to a screen of another DPR or logical DPI, the display draws and sizes itself as a new one there, and
// its layout gets the new minimum once
void tst_TankDisplay::screenChangeOfAVisibleTankDisplay()
{
    QFETCH(double, dpr);
    QFETCH(int, dpi);
    const std::vector<std::pair<const char *, std::function<void(TankDisplay &)>>> configurations = {
        {"default", [](TankDisplay &) {}},
        {"limits, high-high in force", [](TankDisplay &t) { t.setLimits(5.0, 15.0, 85.0, 95.0); t.setValue(97.0); }},
        {"horizontal cylinder, 0-2.4 m", [](TankDisplay &t) {
             t.setShape(TankShape::HorizontalCylinder);
             t.setRange(0.0, 2.4);
             t.setUnit(QStringLiteral("m"));
             t.setValue(1.2);
         }},
        {"invalid hopper", [](TankDisplay &t) { t.setShape(TankShape::Hopper); t.setValue(kNaN); }},
    };
    int changed = 0;
    for (const auto &configuration : configurations) {
        bool minimumChanged = false;
        ScreenChange::checkScreenChange<TankDisplay>(configuration.second, dpr, dpi, &minimumChanged);
        if (QTest::currentTestFailed()) {
            qWarning("configuration: %s", configuration.first);
            return;
        }
        changed += minimumChanged ? 1 : 0;
    }
    // The texts are never below 7 pt: at DPI 120 and 144 the minimum grows
    if (dpi != 96) {
        QVERIFY2(changed > 0, "the minimum is the same on both screens for every configuration");
    }
}

void tst_TankDisplay::nonFiniteInput()
{
    TankDisplay t;
    prepare(t);
    t.setRange(kNaN, 50.0);
    t.setRange(0.0, kInf);
    t.setRange(5.0, 5.0);
    t.setRange(10.0, 5.0);
    QCOMPARE(t.minimum(), 0.0);
    QCOMPARE(t.maximum(), 100.0);
    t.setLimitHysteresis(-1.0);
    t.setLimitHysteresis(kNaN);
    t.setLimitHysteresis(kInf);
    QCOMPARE(t.limitHysteresis(), 0.0);
    t.setMajorTicks(0);
    t.setMinorTicks(-1);
    QCOMPARE(t.majorTicks(), 5);
    QCOMPARE(t.minorTicks(), 4);
    // ±Inf fill to the ends, NaN empty; nothing crashes at any size
    for (QSize size : {QSize(160, 260), QSize(1, 1), QSize(0, 0), QSize(20, 300), QSize(300, 20)}) {
        t.resize(size);
        for (double v : {kInf, -kInf, kNaN, 1e308, -1e308}) {
            t.setValue(v);
            (void)render(t);
        }
    }
    t.resize(160, 260);
    t.setValue(kInf);
    const QImage img = render(t);
    const QRectF v = layoutOf(t).vessel;
    QVERIFY(near(at(img, v.center().x(), v.top() + 3.0), kContent));
}

QTEST_MAIN(tst_TankDisplay)
#include "tst_TankDisplay.moc"
