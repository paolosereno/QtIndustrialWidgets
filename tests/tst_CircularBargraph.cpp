// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include "internal/CircularBargraphSegments.h"
#include <QtGui/QImage>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

namespace {

// Encodes segment states as a string ('.' Off, 'L' Lit, 'P' Peak), index 0 first,
// so that QCOMPARE prints the whole arc on failure.
QString encodeStates(const std::vector<internal::SegmentState> &states)
{
    QString out;
    for (const internal::SegmentState st : states) {
        switch (st) {
        case internal::SegmentState::Off: out += QLatin1Char('.'); break;
        case internal::SegmentState::Lit: out += QLatin1Char('L'); break;
        case internal::SegmentState::Peak: out += QLatin1Char('P'); break;
        }
    }
    return out;
}

QString statesFor(double value, double peak, int n, bool dot, bool peakHold = true,
                  double minimum = 0.0, double maximum = 100.0)
{
    internal::SegmentInput in;
    in.value = value;
    in.minimum = minimum;
    in.maximum = maximum;
    in.peakValue = peak;
    in.segmentCount = n;
    in.dotMode = dot;
    in.peakHoldEnabled = peakHold;
    return encodeStates(internal::computeSegmentStates(in));
}

// Expected arc: `lit` Lit segments starting at `litFrom`, an optional Peak at `peakAt`.
QString arc(int n, int litFrom, int lit, int peakAt = -1)
{
    QString out(n, QLatin1Char('.'));
    for (int i = litFrom; i < litFrom + lit; ++i) {
        out[i] = QLatin1Char('L');
    }
    if (peakAt >= 0) {
        out[peakAt] = QLatin1Char('P');
    }
    return out;
}

// Counts pixels exactly equal to `color` (antialiased edges are blends, never exact).
int countExact(const QImage &img, const QColor &color)
{
    const QRgb target = color.rgb();
    int count = 0;
    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if ((line[x] | 0xff000000u) == target && qAlpha(line[x]) == 255) {
                ++count;
            }
        }
    }
    return count;
}

QImage grabImage(CircularBargraph &w)
{
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// Distinct, fully opaque colors so that a sampled pixel identifies its segment state.
const QColor kNormal(0, 200, 0);
const QColor kWarning(230, 180, 0);
const QColor kError(220, 0, 0);
const QColor kPeak(255, 0, 255);
const QColor kGhost(40, 40, 40);
const QColor kDial(10, 10, 60);
const QColor kSentinel(1, 2, 3); // image background: "nothing painted here"

void applyTestColors(CircularBargraph &w)
{
    w.setNormalColor(kNormal);
    w.setWarningColor(kWarning);
    w.setErrorColor(kError);
    w.setPeakColor(kPeak);
    w.setGhostColor(kGhost);
    w.setDialColor(kDial);
    w.setDigitalDisplayVisible(false);
    w.setPeakHoldTimeMs(1000000); // no decay while a test runs
}

// Renders the widget alone (no background) into an image of the given device pixel ratio.
QImage renderAt(CircularBargraph &w, qreal dpr)
{
    QImage img((QSizeF(w.size()) * dpr).toSize(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(kSentinel);
    w.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    return img;
}

QChar classify(const QColor &c)
{
    if (c == kGhost) return QLatin1Char('.');
    if (c == kPeak) return QLatin1Char('P');
    if (c == kNormal) return QLatin1Char('N');
    if (c == kWarning) return QLatin1Char('W');
    if (c == kError) return QLatin1Char('E');
    return QLatin1Char('?');
}

// Samples the color at the centre of every segment and classifies it. The geometry is
// recomputed here from the documented layout (0 deg = 12 o'clock, clockwise; segment i spans
// [start + i*step, start + (i+1)*step]; ring between 0.66 and 0.90 of radius = side/2 - 4),
// independently of the painting code.
QString sampleSegments(const QImage &img, const CircularBargraph &w)
{
    const double dpr = img.devicePixelRatio();
    const double side = std::min(w.width(), w.height());
    const double cx = w.width() * 0.5;
    const double cy = w.height() * 0.5;
    const double rMid = (side * 0.5 - 4.0) * (0.90 + 0.66) * 0.5;
    const int n = w.segmentCount();
    const double step = w.spanAngle() / n;

    QString out;
    for (int i = 0; i < n; ++i) {
        const double ang = w.startAngle() + (i + 0.5) * step;
        const double rad = (ang - 90.0) * 3.14159265358979323846 / 180.0;
        const int px = static_cast<int>(std::floor((cx + rMid * std::cos(rad)) * dpr));
        const int py = static_cast<int>(std::floor((cy + rMid * std::sin(rad)) * dpr));
        out += classify(img.pixelColor(px, py));
    }
    return out;
}

// arc() with the Lit segments shown in the expected zone color letter.
QString zoneArc(int n, int litFrom, int lit, QChar zone, int peakAt = -1)
{
    QString out = arc(n, litFrom, lit, peakAt);
    out.replace(QLatin1Char('L'), zone);
    return out;
}

// True if every pixel outside the logical rectangle `inside` still holds the sentinel.
bool untouchedOutside(const QImage &img, const QRectF &inside)
{
    const double dpr = img.devicePixelRatio();
    const QRectF dev(inside.topLeft() * dpr, inside.size() * dpr);
    const QRgb sentinel = kSentinel.rgb();
    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (dev.contains(QPointF(x + 0.5, y + 0.5))) {
                continue;
            }
            if (line[x] != sentinel) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

class tst_CircularBargraph : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void settersAndClamping();
    void signalEmission();
    void nanAndInfinityResilience();
    void displayModes();
    void peakHoldAndDecay();
    void digitalDisplay();
    void extremeResizing();
    void offscreenRendering();
    void segmentStatesAtMinimum();
    void segmentStatesAtMaximum();
    void segmentStatesIntermediate();
    void segmentStatesPeak();
    void noPeakSegmentAtMinimum();
    void peakFollowsValueWhenHoldDisabled();
    void peakHoldThenMonotonicDecay();
    void thresholdsStayOrdered();
    void thresholdsOrderIndependent();
    void pixelColorAtSegmentCenters_data();
    void pixelColorAtSegmentCenters();
};

void tst_CircularBargraph::defaultValues()
{
    CircularBargraph bargraph;
    QCOMPARE(bargraph.minimum(), 0.0);
    QCOMPARE(bargraph.maximum(), 100.0);
    QCOMPARE(bargraph.value(), 0.0);
    QCOMPARE(bargraph.segmentCount(), 24);
    QCOMPARE(bargraph.startAngle(), -135.0);
    QCOMPARE(bargraph.spanAngle(), 270.0);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.warningThreshold(), 70.0);
    QCOMPARE(bargraph.errorThreshold(), 85.0);
    QCOMPARE(bargraph.isPeakHoldEnabled(), true);
    QCOMPARE(bargraph.peakHoldTimeMs(), 1200);
    QCOMPARE(bargraph.isDigitalDisplayVisible(), true);
    QCOMPARE(bargraph.precision(), 1);
    QCOMPARE(bargraph.unit(), QStringLiteral("%"));
}

void tst_CircularBargraph::settersAndClamping()
{
    CircularBargraph bargraph;
    bargraph.setRange(10.0, 50.0);
    QCOMPARE(bargraph.minimum(), 10.0);
    QCOMPARE(bargraph.maximum(), 50.0);

    // Below minimum clamp
    bargraph.setValue(5.0);
    QCOMPARE(bargraph.value(), 10.0);

    // Above maximum clamp
    bargraph.setValue(60.0);
    QCOMPARE(bargraph.value(), 50.0);

    // Valid intermediate value
    bargraph.setValue(30.0);
    QCOMPARE(bargraph.value(), 30.0);

    // Inverted range rejection
    bargraph.setRange(80.0, 20.0);
    QCOMPARE(bargraph.minimum(), 10.0);
    QCOMPARE(bargraph.maximum(), 50.0);

    // Segment count
    bargraph.setSegmentCount(32);
    QCOMPARE(bargraph.segmentCount(), 32);
    bargraph.setSegmentCount(2); // Too small (< 4), must be rejected
    QCOMPARE(bargraph.segmentCount(), 32);

    // Span angle
    bargraph.setSpanAngle(180.0);
    QCOMPARE(bargraph.spanAngle(), 180.0);
    bargraph.setSpanAngle(-10.0); // Invalid <= 0
    QCOMPARE(bargraph.spanAngle(), 180.0);
    bargraph.setSpanAngle(400.0); // Invalid > 360
    QCOMPARE(bargraph.spanAngle(), 180.0);
}

void tst_CircularBargraph::signalEmission()
{
    CircularBargraph bargraph;
    QSignalSpy spyValue(&bargraph, &CircularBargraph::valueChanged);
    QSignalSpy spyRange(&bargraph, &CircularBargraph::rangeChanged);
    QSignalSpy spyThreshold(&bargraph, &CircularBargraph::thresholdChanged);
    QSignalSpy spyAppearance(&bargraph, &CircularBargraph::appearanceChanged);

    // 1. Value change
    bargraph.setValue(45.0);
    QCOMPARE(spyValue.count(), 1);
    QCOMPARE(spyValue.takeFirst().at(0).toDouble(), 45.0);

    // Duplicate value must not emit signal
    bargraph.setValue(45.0);
    QCOMPARE(spyValue.count(), 0);

    // 2. Range change
    bargraph.setRange(0.0, 200.0);
    QCOMPARE(spyRange.count(), 1);
    auto rangeArgs = spyRange.takeFirst();
    QCOMPARE(rangeArgs.at(0).toDouble(), 0.0);
    QCOMPARE(rangeArgs.at(1).toDouble(), 200.0);

    // 3. Threshold change
    bargraph.setWarningThreshold(140.0);
    QCOMPARE(spyThreshold.count(), 1);
    bargraph.setErrorThreshold(180.0);
    QCOMPARE(spyThreshold.count(), 2);

    // 4. Appearance change on visual setters
    spyAppearance.clear();
    bargraph.setSegmentCount(48);
    QCOMPARE(spyAppearance.count(), 1);
    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(spyAppearance.count(), 2);
}

void tst_CircularBargraph::nanAndInfinityResilience()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setValue(42.0);
    QCOMPARE(bargraph.value(), 42.0);

    // NaN is ignored: the widget keeps the last valid value
    bargraph.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(bargraph.value(), 42.0);

    // Positive infinity saturates to maximum
    bargraph.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 100.0);

    // Negative infinity saturates to minimum
    bargraph.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 0.0);

    // Non-finite range bounds rejected
    bargraph.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(bargraph.minimum(), 0.0);
    bargraph.setRange(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.maximum(), 100.0);
}

void tst_CircularBargraph::displayModes()
{
    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.resize(220, 220);
    bargraph.setValue(50.0); // n = 12 of 24
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), zoneArc(24, 0, 12, QLatin1Char('N')));

    // Dot: a single cursor segment, the last one the bar would light.
    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Dot);
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), zoneArc(24, 11, 1, QLatin1Char('N')));

    // Dot at the minimum still shows the cursor (zone color, not peak color).
    bargraph.setValue(0.0);
    bargraph.resetPeak();
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), zoneArc(24, 0, 1, QLatin1Char('N')));

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), zoneArc(24, 0, 0, QLatin1Char('N')));
}

void tst_CircularBargraph::peakHoldAndDecay()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setPeakHoldEnabled(true);

    bargraph.setValue(80.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    // Value drops, peak stays at 80.0
    bargraph.setValue(30.0);
    QCOMPARE(bargraph.value(), 30.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    // Manual reset returns peak to current value
    bargraph.resetPeak();
    QCOMPARE(bargraph.peakValue(), 30.0);

    // Disabling peak hold resets peak value
    bargraph.setValue(90.0);
    QCOMPARE(bargraph.peakValue(), 90.0);
    bargraph.setValue(50.0);
    bargraph.setPeakHoldEnabled(false);
    QCOMPARE(bargraph.peakValue(), 50.0);
}

void tst_CircularBargraph::digitalDisplay()
{
    CircularBargraph bargraph;
    bargraph.setPrecision(2);
    QCOMPARE(bargraph.precision(), 2);

    bargraph.setUnit(QStringLiteral("bar"));
    QCOMPARE(bargraph.unit(), QStringLiteral("bar"));

    bargraph.setDigitalDisplayVisible(false);
    QCOMPARE(bargraph.isDigitalDisplayVisible(), false);
}

void tst_CircularBargraph::extremeResizing()
{
    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.setValue(50.0); // n = 12 of 24
    const qreal dpr = bargraph.devicePixelRatioF();

    // Too small to hold the ring (radius = side/2 - 4 <= 6): nothing is painted.
    const QList<QSize> tooSmall = {QSize(0, 0), QSize(1, 1), QSize(2, 2), QSize(4, 4),
                                   QSize(20, 20), QSize(10, 1000), QSize(1000, 10)};
    for (const QSize &sz : tooSmall) {
        bargraph.resize(sz);
        QCOMPARE(bargraph.size(), sz);
        const QImage img = renderAt(bargraph, dpr);
        QVERIFY2(untouchedOutside(img, QRectF()),
                 qPrintable(QStringLiteral("painted at %1x%2").arg(sz.width()).arg(sz.height())));
    }

    // Drawable sizes: the dial is painted, and only inside the centred square. Above 100 px
    // the segments are large enough for their centre pixel to be solid: check every state.
    const QList<QSize> drawable = {QSize(40, 1000), QSize(1000, 40), QSize(120, 1000),
                                   QSize(1000, 120), QSize(200, 200), QSize(3840, 2160)};
    for (const QSize &sz : drawable) {
        bargraph.resize(sz);
        QCOMPARE(bargraph.size(), sz);
        const QImage img = renderAt(bargraph, dpr);
        const double side = std::min(sz.width(), sz.height());
        const QRectF square((sz.width() - side) * 0.5, (sz.height() - side) * 0.5, side, side);
        const QString where = QStringLiteral("%1x%2").arg(sz.width()).arg(sz.height());
        QVERIFY2(!untouchedOutside(img, QRectF()), qPrintable(QStringLiteral("nothing painted at ") + where));
        QVERIFY2(untouchedOutside(img, square), qPrintable(QStringLiteral("painted outside the dial at ") + where));
        if (side >= 100) {
            QCOMPARE(sampleSegments(img, bargraph), zoneArc(24, 0, 12, QLatin1Char('N')));
        }
    }
}

void tst_CircularBargraph::offscreenRendering()
{
    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.setRange(0.0, 100.0);
    bargraph.setWarningThreshold(60.0);
    bargraph.setErrorThreshold(80.0);
    bargraph.resize(200, 200);

    for (const qreal dpr : {1.0, 2.0}) {
        bargraph.resetPeak();
        bargraph.setValue(75.0); // warning zone, n = 18 of 24
        QImage img = renderAt(bargraph, dpr);
        QCOMPARE(img.size(), QSize(200, 200) * dpr);
        QCOMPARE(sampleSegments(img, bargraph), zoneArc(24, 0, 18, QLatin1Char('W')));

        bargraph.setValue(85.0); // error zone, n = round(20.4) = 20
        img = renderAt(bargraph, dpr);
        QCOMPARE(sampleSegments(img, bargraph), zoneArc(24, 0, 20, QLatin1Char('E')));

        bargraph.setValue(40.0); // normal zone, n = round(9.6) = 10, held peak at 85 -> idx 19
        img = renderAt(bargraph, dpr);
        QCOMPARE(sampleSegments(img, bargraph), zoneArc(24, 0, 10, QLatin1Char('N'), 19));
    }
}

void tst_CircularBargraph::pixelColorAtSegmentCenters_data()
{
    QTest::addColumn<int>("segments");
    QTest::addColumn<double>("startAngle");
    QTest::addColumn<double>("spanAngle");
    QTest::addColumn<bool>("dot");
    QTest::addColumn<double>("peak");
    QTest::addColumn<double>("value");
    QTest::addColumn<QString>("expected");

    QTest::newRow("bar24-270-normal-peak") << 24 << -135.0 << 270.0 << false << 80.0 << 50.0
        << zoneArc(24, 0, 12, QLatin1Char('N'), 18);
    QTest::newRow("bar16-180-warning") << 16 << -90.0 << 180.0 << false << 75.0 << 75.0
        << zoneArc(16, 0, 12, QLatin1Char('W'));
    QTest::newRow("bar32-360-error-peak") << 32 << 0.0 << 360.0 << false << 100.0 << 90.0
        << zoneArc(32, 0, 29, QLatin1Char('E'), 31);
    QTest::newRow("bar24-full-error") << 24 << -135.0 << 270.0 << false << 100.0 << 100.0
        << zoneArc(24, 0, 24, QLatin1Char('E'));
    QTest::newRow("bar48-minimum") << 48 << -135.0 << 270.0 << false << 0.0 << 0.0
        << zoneArc(48, 0, 0, QLatin1Char('N'));
    QTest::newRow("dot24-270-peak") << 24 << -135.0 << 270.0 << true << 50.0 << 30.0
        << zoneArc(24, 6, 1, QLatin1Char('N'), 11);
    QTest::newRow("dot48-minimum") << 48 << -135.0 << 270.0 << true << 0.0 << 0.0
        << zoneArc(48, 0, 1, QLatin1Char('N'));
    QTest::newRow("dot16-180-error") << 16 << -90.0 << 180.0 << true << 95.0 << 95.0
        << zoneArc(16, 14, 1, QLatin1Char('E')); // n = round(15.2) = 15
}

void tst_CircularBargraph::pixelColorAtSegmentCenters()
{
    QFETCH(int, segments);
    QFETCH(double, startAngle);
    QFETCH(double, spanAngle);
    QFETCH(bool, dot);
    QFETCH(double, peak);
    QFETCH(double, value);
    QFETCH(QString, expected);

    CircularBargraph bargraph; // thresholds 70 / 85
    applyTestColors(bargraph);
    bargraph.setSegmentCount(segments);
    bargraph.setStartAngle(startAngle);
    bargraph.setSpanAngle(spanAngle);
    bargraph.setDisplayMode(dot ? CircularBargraph::DisplayMode::Dot : CircularBargraph::DisplayMode::Bar);
    bargraph.resize(240, 240);
    bargraph.setValue(peak);
    bargraph.setValue(value);

    QCOMPARE(sampleSegments(renderAt(bargraph, bargraph.devicePixelRatioF()), bargraph), expected);
}

void tst_CircularBargraph::segmentStatesAtMinimum()
{
    // Bar: value and peak at minimum -> nothing lit, no peak segment.
    QCOMPARE(statesFor(0.0, 0.0, 24, false), arc(24, 0, 0));
    QCOMPARE(statesFor(0.0, 0.0, 24, false, false), arc(24, 0, 0));
    QCOMPARE(statesFor(-5.0, -5.0, 16, false, true, -5.0, 5.0), arc(16, 0, 0));

    // Dot: a valid value always lights one segment, in the zone color (Lit, not Peak).
    QCOMPARE(statesFor(0.0, 0.0, 24, true), arc(24, 0, 1));
    QCOMPARE(statesFor(0.0, 0.0, 24, true, false), arc(24, 0, 1));
}

void tst_CircularBargraph::segmentStatesAtMaximum()
{
    QCOMPARE(statesFor(100.0, 100.0, 24, false), arc(24, 0, 24));
    QCOMPARE(statesFor(100.0, 100.0, 24, true), arc(24, 23, 1));
    QCOMPARE(statesFor(5.0, 5.0, 16, false, true, -5.0, 5.0), arc(16, 0, 16));
}

void tst_CircularBargraph::segmentStatesIntermediate()
{
    // Bar: n = round(f * N) lit segments.
    QCOMPARE(statesFor(50.0, 50.0, 24, false), arc(24, 0, 12));
    QCOMPARE(statesFor(10.0, 10.0, 24, false), arc(24, 0, 2));   // 2.4
    QCOMPARE(statesFor(2.5, 2.5, 24, false), arc(24, 0, 1));     // 0.6
    QCOMPARE(statesFor(1.0, 1.0, 24, false), arc(24, 0, 0));     // 0.24
    QCOMPARE(statesFor(75.0, 75.0, 16, false), arc(16, 0, 12));

    // Dot: the last segment the bar would light, max(n - 1, 0).
    QCOMPARE(statesFor(50.0, 50.0, 24, true), arc(24, 11, 1));
    QCOMPARE(statesFor(30.0, 30.0, 24, true), arc(24, 6, 1));    // n = round(7.2) = 7
    QCOMPARE(statesFor(1.0, 1.0, 24, true), arc(24, 0, 1));      // n = 0
    QCOMPARE(statesFor(75.0, 75.0, 16, true), arc(16, 11, 1));
}

void tst_CircularBargraph::segmentStatesPeak()
{
    // Peak segment = last segment the bar would light for the peak value: 50% -> 12/24.
    QCOMPARE(statesFor(30.0, 50.0, 24, false), arc(24, 0, 7, 11));
    QCOMPARE(statesFor(50.0, 80.0, 24, false), arc(24, 0, 12, 18)); // round(19.2) = 19
    QCOMPARE(statesFor(0.0, 100.0, 24, false), arc(24, 0, 0, 23));

    // Peak at or below the bar top is not drawn.
    QCOMPARE(statesFor(50.0, 50.0, 24, false), arc(24, 0, 12));
    QCOMPARE(statesFor(50.0, 51.0, 24, false), arc(24, 0, 12));     // round(12.24) = 12

    // Peak hold disabled: never a peak segment.
    QCOMPARE(statesFor(30.0, 50.0, 24, false, false), arc(24, 0, 7));

    // Dot: peak drawn only above the dot; the dot keeps its zone color.
    QCOMPARE(statesFor(30.0, 50.0, 24, true), arc(24, 6, 1, 11));
    QCOMPARE(statesFor(0.0, 3.0, 24, true), arc(24, 0, 1));         // round(0.72) = 1 -> idx 0 == dot
    QCOMPARE(statesFor(0.0, 10.0, 24, true), arc(24, 0, 1, 1));     // round(2.4) = 2 -> idx 1
}

void tst_CircularBargraph::noPeakSegmentAtMinimum()
{
    CircularBargraph bargraph;
    bargraph.setDigitalDisplayVisible(false);
    bargraph.setPeakColor(QColor(255, 0, 255));
    bargraph.setPeakHoldEnabled(true);
    bargraph.resize(200, 200);
    bargraph.setValue(0.0);

    QCOMPARE(bargraph.peakValue(), 0.0);
    QCOMPARE(countExact(grabImage(bargraph), QColor(255, 0, 255)), 0);

    // Control: a held peak above the bar is painted with the peak color.
    bargraph.setValue(60.0);
    bargraph.setValue(10.0);
    QVERIFY(countExact(grabImage(bargraph), QColor(255, 0, 255)) > 50);
}

void tst_CircularBargraph::peakFollowsValueWhenHoldDisabled()
{
    CircularBargraph bargraph;
    bargraph.setDigitalDisplayVisible(false);
    bargraph.setPeakColor(QColor(255, 0, 255));
    bargraph.resize(200, 200);
    bargraph.setPeakHoldEnabled(false);

    const double values[] = {80.0, 30.0, 55.0, 0.0, 100.0, 12.5};
    for (const double v : values) {
        bargraph.setValue(v);
        QCOMPARE(bargraph.peakValue(), bargraph.value());
    }

    // Falling value, then re-enabling peak hold: no phantom peak.
    bargraph.setValue(90.0);
    bargraph.setValue(20.0);
    QCOMPARE(bargraph.peakValue(), 20.0);
    bargraph.setPeakHoldEnabled(true);
    QCOMPARE(bargraph.peakValue(), 20.0);
    QCOMPARE(countExact(grabImage(bargraph), QColor(255, 0, 255)), 0);

    // With hold enabled again, a new fall is held as usual.
    bargraph.setValue(70.0);
    bargraph.setValue(40.0);
    QCOMPARE(bargraph.peakValue(), 70.0);
}

void tst_CircularBargraph::peakHoldThenMonotonicDecay()
{
    constexpr int holdMs = 1000;
    CircularBargraph bargraph;
    bargraph.setPeakHoldTimeMs(holdMs);
    bargraph.setPeakDecayRate(200.0); // 60 units -> ~0.3 s of decay

    QElapsedTimer clock;
    clock.start(); // started before the peak is set: clock.elapsed() >= widget hold time
    bargraph.setValue(80.0);
    bargraph.setValue(20.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    qint64 firstDropMs = -1;
    double previous = bargraph.peakValue();
    while (clock.elapsed() < holdMs + 5000 && bargraph.peakValue() > bargraph.value()) {
        QTest::qWait(10);
        const double peak = bargraph.peakValue();
        QVERIFY2(peak <= previous, "peak must never rise while decaying");
        QVERIFY2(peak >= bargraph.value(), "peak must never fall below the value");
        if (firstDropMs < 0 && peak < 80.0) {
            firstDropMs = clock.elapsed();
        }
        previous = peak;
    }

    QVERIFY2(firstDropMs >= holdMs, qPrintable(QStringLiteral("decay began after %1 ms").arg(firstDropMs)));
    QCOMPARE(bargraph.peakValue(), 20.0);
}

void tst_CircularBargraph::thresholdsStayOrdered()
{
    CircularBargraph bargraph; // warning 70, error 85
    QSignalSpy spy(&bargraph, &CircularBargraph::thresholdChanged);

    // Raising warning above error drags error up with it (one signal, both values).
    bargraph.setWarningThreshold(90.0);
    QCOMPARE(bargraph.warningThreshold(), 90.0);
    QCOMPARE(bargraph.errorThreshold(), 90.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 90.0);
    QCOMPARE(spy.at(0).at(1).toDouble(), 90.0);
    spy.clear();

    // Lowering error below warning drags warning down with it.
    bargraph.setErrorThreshold(50.0);
    QCOMPARE(bargraph.warningThreshold(), 50.0);
    QCOMPARE(bargraph.errorThreshold(), 50.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 50.0);
    QCOMPARE(spy.at(0).at(1).toDouble(), 50.0);
    spy.clear();

    // A consistent change moves only its own threshold.
    bargraph.setErrorThreshold(80.0);
    QCOMPARE(bargraph.warningThreshold(), 50.0);
    QCOMPARE(bargraph.errorThreshold(), 80.0);
    bargraph.setWarningThreshold(80.0); // equal is allowed
    QCOMPARE(bargraph.warningThreshold(), 80.0);
    QCOMPARE(bargraph.errorThreshold(), 80.0);
    QCOMPARE(spy.count(), 2);
}

void tst_CircularBargraph::thresholdsOrderIndependent()
{
    struct Pair { double warning; double error; };
    const Pair targets[] = {{90.0, 95.0}, {40.0, 50.0}, {10.0, 99.0}, {80.0, 80.0}, {-20.0, -10.0}};

    for (const Pair &t : targets) {
        CircularBargraph warningFirst; // starts at 70 / 85
        warningFirst.setWarningThreshold(t.warning);
        warningFirst.setErrorThreshold(t.error);
        QCOMPARE(warningFirst.warningThreshold(), t.warning);
        QCOMPARE(warningFirst.errorThreshold(), t.error);

        CircularBargraph errorFirst;
        errorFirst.setErrorThreshold(t.error);
        errorFirst.setWarningThreshold(t.warning);
        QCOMPARE(errorFirst.warningThreshold(), t.warning);
        QCOMPARE(errorFirst.errorThreshold(), t.error);

        // Same through the property system (the path used by Designer .ui files).
        CircularBargraph viaProperties;
        QVERIFY(viaProperties.setProperty("errorThreshold", t.error));
        QVERIFY(viaProperties.setProperty("warningThreshold", t.warning));
        QCOMPARE(viaProperties.warningThreshold(), t.warning);
        QCOMPARE(viaProperties.errorThreshold(), t.error);
    }
}

QTEST_MAIN(tst_CircularBargraph)
#include "tst_CircularBargraph.moc"
