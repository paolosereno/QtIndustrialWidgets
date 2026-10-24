// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>

#include "FontTestUtils.h"

#include <memory>
#include "internal/CircularBargraphSegments.h"
#include <QtGui/QImage>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

namespace {

// Encodes segment states as a string ('.' Off, 'L' Lit, 'P' Peak, 'G' Ghost), index 0 first,
// so that QCOMPARE prints the whole arc on failure.
QString encodeStates(const std::vector<internal::SegmentState> &states)
{
    QString out;
    for (const internal::SegmentState st : states) {
        switch (st) {
        case internal::SegmentState::Off: out += QLatin1Char('.'); break;
        case internal::SegmentState::Lit: out += QLatin1Char('L'); break;
        case internal::SegmentState::Peak: out += QLatin1Char('P'); break;
        case internal::SegmentState::Ghost: out += QLatin1Char('G'); break;
        }
    }
    return out;
}

QString statesFor(double value, double peak, int n, bool dot, bool peakHold = true,
                  double minimum = 0.0, double maximum = 100.0,
                  internal::InputState state = internal::InputState::Valid, bool blinkOn = true)
{
    internal::SegmentInput in;
    in.state = state;
    in.blinkOn = blinkOn;
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

// Expected dimmed tint of an unlit segment, recomputed here: 25 % zone color over ghost.
QColor expectedTint(const QColor &zone)
{
    const auto mix = [](int z, int g) { return qRound(0.25 * z + 0.75 * g); };
    return QColor(mix(zone.red(), kGhost.red()), mix(zone.green(), kGhost.green()),
                  mix(zone.blue(), kGhost.blue()));
}

bool near(const QColor &a, const QColor &b)
{
    return std::abs(a.red() - b.red()) <= 1 && std::abs(a.green() - b.green()) <= 1
        && std::abs(a.blue() - b.blue()) <= 1 && a.alpha() == 255;
}

// 'N'/'W'/'E' lit zone color, 'n'/'w'/'e' dimmed tint of that zone, 'P' peak,
// 'G' plain ghost (Invalid), '?' anything else.
QChar classify(const QColor &c)
{
    if (c == kPeak) return QLatin1Char('P');
    if (c == kNormal) return QLatin1Char('N');
    if (c == kWarning) return QLatin1Char('W');
    if (c == kError) return QLatin1Char('E');
    if (c == kGhost) return QLatin1Char('G');
    if (near(c, expectedTint(kNormal))) return QLatin1Char('n');
    if (near(c, expectedTint(kWarning))) return QLatin1Char('w');
    if (near(c, expectedTint(kError))) return QLatin1Char('e');
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

// Zone letter of every segment of `w`, recomputed here from the documented rule: the zone
// containing the segment centre (>= threshold belongs to the upper zone).
QString expectedZones(const CircularBargraph &w)
{
    QString out;
    const int n = w.segmentCount();
    for (int i = 0; i < n; ++i) {
        const double c = w.minimum() + (i + 0.5) * (w.maximum() - w.minimum()) / n;
        out += (c >= w.errorThreshold()) ? QLatin1Char('E')
             : (c >= w.warningThreshold()) ? QLatin1Char('W') : QLatin1Char('N');
    }
    return out;
}

// Turns a state arc ('.', 'L', 'P', 'G') into the expected pixel classification of `w`.
QString expectedPixels(const CircularBargraph &w, const QString &stateArc)
{
    const QString zones = expectedZones(w);
    QString out = stateArc;
    for (int i = 0; i < out.size(); ++i) {
        if (out[i] == QLatin1Char('L')) {
            out[i] = zones[i];
        } else if (out[i] == QLatin1Char('.')) {
            out[i] = zones[i].toLower();
        }
    }
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
    void textSizeIgnoresWidgetFontSize();
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
    void peakDecayStartsAtHoldEndWithoutJump();
    void valueStateTransitions();
    void rawValueAndRangeRecompute();
    void peakInInvalidState();
    void segmentStatesOutOfRangeAndInvalid();
    void readoutText();
    void invalidStatePixels();
    void outOfRangeEndSegmentBlinks_data();
    void outOfRangeEndSegmentBlinks();
    void segmentZonesByPosition();
    void unlitTintColor();
    void zoneColorsByPositionPixels();
    void peakDecayStepIndependentOfFma();
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
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Valid);
    QCOMPARE(bargraph.rawValue(), 0.0);
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

    // NaN: Invalid, value() is NaN
    bargraph.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Invalid);
    QVERIFY(std::isnan(bargraph.value()));

    // Positive infinity saturates to maximum (Overrange)
    bargraph.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 100.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);

    // Negative infinity saturates to minimum (Underrange)
    bargraph.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 0.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Underrange);

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
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), expectedPixels(bargraph, arc(24, 0, 12)));

    // Dot: a single cursor segment, the last one the bar would light.
    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Dot);
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), expectedPixels(bargraph, arc(24, 11, 1)));

    // Dot at the minimum still shows the cursor (zone color, not peak color).
    bargraph.setValue(0.0);
    bargraph.resetPeak();
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), expectedPixels(bargraph, arc(24, 0, 1)));

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
    QCOMPARE(sampleSegments(renderAt(bargraph, 1.0), bargraph), expectedPixels(bargraph, arc(24, 0, 0)));
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
            QCOMPARE(sampleSegments(img, bargraph), expectedPixels(bargraph, arc(24, 0, 12)));
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
        QCOMPARE(sampleSegments(img, bargraph), expectedPixels(bargraph, arc(24, 0, 18)));

        bargraph.setValue(85.0); // error zone, n = round(20.4) = 20
        img = renderAt(bargraph, dpr);
        QCOMPARE(sampleSegments(img, bargraph), expectedPixels(bargraph, arc(24, 0, 20)));

        bargraph.setValue(40.0); // normal zone, n = round(9.6) = 10, held peak at 85 -> idx 19
        img = renderAt(bargraph, dpr);
        QCOMPARE(sampleSegments(img, bargraph), expectedPixels(bargraph, arc(24, 0, 10, 19)));
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
        << arc(24, 0, 12, 18);
    QTest::newRow("bar16-180-warning") << 16 << -90.0 << 180.0 << false << 75.0 << 75.0
        << arc(16, 0, 12);
    QTest::newRow("bar32-360-error-peak") << 32 << 0.0 << 360.0 << false << 100.0 << 90.0
        << arc(32, 0, 29, 31);
    QTest::newRow("bar24-full-error") << 24 << -135.0 << 270.0 << false << 100.0 << 100.0
        << arc(24, 0, 24);
    QTest::newRow("bar48-minimum") << 48 << -135.0 << 270.0 << false << 0.0 << 0.0
        << arc(48, 0, 0);
    QTest::newRow("dot24-270-peak") << 24 << -135.0 << 270.0 << true << 50.0 << 30.0
        << arc(24, 6, 1, 11);
    QTest::newRow("dot48-minimum") << 48 << -135.0 << 270.0 << true << 0.0 << 0.0
        << arc(48, 0, 1);
    QTest::newRow("dot16-180-error") << 16 << -90.0 << 180.0 << true << 95.0 << 95.0
        << arc(16, 14, 1); // n = round(15.2) = 15
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

    QCOMPARE(sampleSegments(renderAt(bargraph, bargraph.devicePixelRatioF()), bargraph),
             expectedPixels(bargraph, expected));
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

void tst_CircularBargraph::peakDecayStartsAtHoldEndWithoutJump()
{
    // Hold shorter than 1 s: the first decay step must not cover the hold period.
    constexpr int holdMs = 300;
    constexpr double rate = 100.0; // units per second
    CircularBargraph bargraph;
    bargraph.setPeakHoldTimeMs(holdMs);
    bargraph.setPeakDecayRate(rate);

    QElapsedTimer clock;
    clock.start(); // started first: clock.elapsed() >= the widget's time since the peak
    bargraph.setValue(80.0);
    bargraph.setValue(20.0);

    // The ideal peak is 80 - rate * (t - hold) after the hold; the widget's own time is
    // never ahead of `clock`, so its peak can never be below that line.
    while (clock.elapsed() < holdMs + 3000 && bargraph.peakValue() > bargraph.value()) {
        QTest::qWait(5);
        const double peak = bargraph.peakValue();
        const double t = static_cast<double>(clock.elapsed());
        const double lowest = 80.0 - rate * std::max(0.0, t - holdMs) / 1000.0;
        QVERIFY2(peak >= lowest - 1e-9,
                 qPrintable(QStringLiteral("t = %1 ms: peak %2 below the decay line %3")
                                .arg(t).arg(peak).arg(lowest)));
    }
    QCOMPARE(bargraph.peakValue(), 20.0);
}

void tst_CircularBargraph::valueStateTransitions()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CircularBargraph bargraph; // range 0..100, value 0, Valid
    QSignalSpy spyValue(&bargraph, &CircularBargraph::valueChanged);
    QSignalSpy spyState(&bargraph, &CircularBargraph::valueStateChanged);

    // Entering Invalid: valueChanged(NaN) once, state signal once.
    bargraph.setValue(nan);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Invalid);
    QVERIFY(std::isnan(bargraph.value()));
    QVERIFY(std::isnan(bargraph.rawValue()));
    QCOMPARE(spyValue.count(), 1);
    QVERIFY(std::isnan(spyValue.takeFirst().at(0).toDouble()));
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyState.takeFirst().at(0).value<CircularBargraph::ValueState>(),
             CircularBargraph::ValueState::Invalid);

    // Repeated NaN: no signal.
    bargraph.setValue(nan);
    QCOMPARE(spyValue.count(), 0);
    QCOMPARE(spyState.count(), 0);

    // Leaving Invalid: the valid value is emitted.
    bargraph.setValue(50.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Valid);
    QCOMPARE(spyValue.count(), 1);
    QCOMPARE(spyValue.takeFirst().at(0).toDouble(), 50.0);
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyState.takeFirst().at(0).value<CircularBargraph::ValueState>(),
             CircularBargraph::ValueState::Valid);

    // Overrange: value() at maximum, raw kept.
    bargraph.setValue(120.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);
    QCOMPARE(bargraph.value(), 100.0);
    QCOMPARE(bargraph.rawValue(), 120.0);
    QCOMPARE(spyState.count(), 1);
    spyState.clear();
    spyValue.clear();
    bargraph.setValue(inf); // still Overrange, value() unchanged: no signal
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);
    QCOMPARE(bargraph.rawValue(), inf);
    QCOMPARE(spyState.count(), 0);
    QCOMPARE(spyValue.count(), 0);

    // Underrange.
    bargraph.setValue(-5.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Underrange);
    QCOMPARE(bargraph.value(), 0.0);
    QCOMPARE(bargraph.rawValue(), -5.0);
    bargraph.setValue(-inf);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Underrange);

    // Bounds themselves are Valid.
    bargraph.setValue(100.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Valid);
    bargraph.setValue(0.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Valid);
}

void tst_CircularBargraph::rawValueAndRangeRecompute()
{
    CircularBargraph bargraph; // 0..100
    QSignalSpy spyState(&bargraph, &CircularBargraph::valueStateChanged);
    QSignalSpy spyValue(&bargraph, &CircularBargraph::valueChanged);

    bargraph.setValue(120.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);
    QCOMPARE(bargraph.value(), 100.0);
    spyState.clear();
    spyValue.clear();

    // Widening the range brings the raw input back into range.
    bargraph.setRange(0.0, 200.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Valid);
    QCOMPARE(bargraph.value(), 120.0);
    QCOMPARE(bargraph.rawValue(), 120.0);
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyValue.count(), 1);
    QCOMPARE(spyValue.takeFirst().at(0).toDouble(), 120.0);

    // Narrowing it puts it out of range again, on either side.
    bargraph.setRange(0.0, 50.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);
    QCOMPARE(bargraph.value(), 50.0);
    bargraph.setRange(130.0, 150.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Underrange);
    QCOMPARE(bargraph.value(), 130.0);

    // ±Inf stay out of range whatever the range.
    bargraph.setValue(std::numeric_limits<double>::infinity());
    bargraph.setRange(-1e300, 1e300);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Overrange);
    QCOMPARE(bargraph.value(), 1e300);

    // NaN stays Invalid across range changes, and does not re-emit valueChanged.
    bargraph.setValue(std::numeric_limits<double>::quiet_NaN());
    spyValue.clear();
    bargraph.setRange(0.0, 10.0);
    QCOMPARE(bargraph.valueState(), CircularBargraph::ValueState::Invalid);
    QVERIFY(std::isnan(bargraph.value()));
    QCOMPARE(spyValue.count(), 0);
}

void tst_CircularBargraph::peakInInvalidState()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CircularBargraph bargraph;
    bargraph.setPeakHoldTimeMs(1000000);
    bargraph.setValue(80.0);
    bargraph.setValue(30.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    bargraph.setValue(nan);
    QVERIFY(std::isnan(bargraph.peakValue()));

    // The first valid value resets the peak, which is then held as usual.
    bargraph.setValue(40.0);
    QCOMPARE(bargraph.peakValue(), 40.0);
    bargraph.setValue(20.0);
    QCOMPARE(bargraph.peakValue(), 40.0);

    // Peak hold disabled: the peak is the value, NaN included.
    bargraph.setPeakHoldEnabled(false);
    bargraph.setValue(nan);
    QVERIFY(std::isnan(bargraph.peakValue()));
    bargraph.setValue(10.0);
    QCOMPARE(bargraph.peakValue(), 10.0);
}

void tst_CircularBargraph::segmentStatesOutOfRangeAndInvalid()
{
    using IS = internal::InputState;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    // Invalid: all ghost, in both modes, whatever the peak.
    QCOMPARE(statesFor(nan, nan, 24, false, true, 0.0, 100.0, IS::Invalid), QString(24, QLatin1Char('G')));
    QCOMPARE(statesFor(nan, 50.0, 16, true, true, 0.0, 100.0, IS::Invalid), QString(16, QLatin1Char('G')));

    // Overrange: full bar, last segment blinking; never shown as Peak.
    QCOMPARE(statesFor(100.0, 100.0, 24, false, true, 0.0, 100.0, IS::Overrange, true), arc(24, 0, 24));
    QCOMPARE(statesFor(100.0, 100.0, 24, false, true, 0.0, 100.0, IS::Overrange, false), arc(24, 0, 23));
    QCOMPARE(statesFor(100.0, 100.0, 24, true, true, 0.0, 100.0, IS::Overrange, true), arc(24, 23, 1));
    QCOMPARE(statesFor(100.0, 100.0, 24, true, true, 0.0, 100.0, IS::Overrange, false), arc(24, 0, 0));

    // Underrange: only the first segment, blinking; a held peak above is still shown.
    QCOMPARE(statesFor(0.0, 0.0, 24, false, true, 0.0, 100.0, IS::Underrange, true), arc(24, 0, 1));
    QCOMPARE(statesFor(0.0, 0.0, 24, false, true, 0.0, 100.0, IS::Underrange, false), arc(24, 0, 0));
    QCOMPARE(statesFor(0.0, 0.0, 24, true, true, 0.0, 100.0, IS::Underrange, true), arc(24, 0, 1));
    QCOMPARE(statesFor(0.0, 0.0, 24, true, true, 0.0, 100.0, IS::Underrange, false), arc(24, 0, 0));
    QCOMPARE(statesFor(0.0, 50.0, 24, false, true, 0.0, 100.0, IS::Underrange, false), arc(24, 0, 0, 11));
    QCOMPARE(statesFor(0.0, 3.0, 24, false, true, 0.0, 100.0, IS::Underrange, false), arc(24, 0, 0)); // peak idx 0 = blink
}

void tst_CircularBargraph::readoutText()
{
    using IS = internal::InputState;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(internal::formatReadout(IS::Valid, 42.31, 0.0, 100.0, 1), QStringLiteral("42.3"));
    QCOMPARE(internal::formatReadout(IS::Valid, 7.0, 0.0, 100.0, 0), QStringLiteral("7"));
    QCOMPARE(internal::formatReadout(IS::Overrange, 100.0, 0.0, 100.0, 1), QStringLiteral(">100.0"));
    QCOMPARE(internal::formatReadout(IS::Underrange, -20.0, -20.0, 80.0, 2), QStringLiteral("<-20.00"));
    QCOMPARE(internal::formatReadout(IS::Invalid, nan, 0.0, 100.0, 1), QStringLiteral("---"));
    // #46: no negative zero in the readout, also for the out-of-range forms
    QCOMPARE(internal::formatReadout(IS::Valid, -0.04, -1.0, 1.0, 1), QStringLiteral("0.0"));
    QCOMPARE(internal::formatReadout(IS::Valid, -0.3, -1.0, 1.0, 0), QStringLiteral("0"));
    QCOMPARE(internal::formatReadout(IS::Valid, -0.05, -1.0, 1.0, 1), QStringLiteral("-0.1"));
    QCOMPARE(internal::formatReadout(IS::Underrange, -2.0, -0.04, 1.0, 1), QStringLiteral("<0.0"));
    QCOMPARE(internal::formatReadout(IS::Overrange, 2.0, -1.0, -0.04, 1), QStringLiteral(">0.0"));

    QCOMPARE(internal::classifyInput(nan, 0.0, 100.0), IS::Invalid);
    QCOMPARE(internal::classifyInput(100.0, 0.0, 100.0), IS::Valid);
    QCOMPARE(internal::classifyInput(0.0, 0.0, 100.0), IS::Valid);
    QCOMPARE(internal::classifyInput(100.5, 0.0, 100.0), IS::Overrange);
    QCOMPARE(internal::classifyInput(-0.5, 0.0, 100.0), IS::Underrange);
    QCOMPARE(internal::classifyInput(std::numeric_limits<double>::infinity(), 0.0, 100.0), IS::Overrange);
    QCOMPARE(internal::classifyInput(-std::numeric_limits<double>::infinity(), 0.0, 100.0), IS::Underrange);
}

void tst_CircularBargraph::invalidStatePixels()
{
    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.resize(220, 220);
    bargraph.setValue(90.0);
    bargraph.setValue(50.0); // held peak at 90
    bargraph.setValue(std::numeric_limits<double>::quiet_NaN());

    for (const auto mode : {CircularBargraph::DisplayMode::Bar, CircularBargraph::DisplayMode::Dot}) {
        bargraph.setDisplayMode(mode);
        const QImage img = renderAt(bargraph, bargraph.devicePixelRatioF());
        QCOMPARE(sampleSegments(img, bargraph), QString(24, QLatin1Char('G')));
        QCOMPARE(countExact(img, kPeak), 0);
    }
}

void tst_CircularBargraph::outOfRangeEndSegmentBlinks_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<bool>("dot");
    QTest::addColumn<int>("blinkIndex");
    QTest::addColumn<QString>("onPhase");
    QTest::addColumn<QString>("offPhase");

    QTest::newRow("over-bar") << 150.0 << false << 23
        << arc(24, 0, 24) << arc(24, 0, 23);
    QTest::newRow("over-dot") << std::numeric_limits<double>::infinity() << true << 23
        << arc(24, 23, 1) << arc(24, 0, 0);
    QTest::newRow("under-bar") << -10.0 << false << 0
        << arc(24, 0, 1) << arc(24, 0, 0);
    QTest::newRow("under-dot") << -std::numeric_limits<double>::infinity() << true << 0
        << arc(24, 0, 1) << arc(24, 0, 0);
}

void tst_CircularBargraph::outOfRangeEndSegmentBlinks()
{
    QFETCH(double, input);
    QFETCH(bool, dot);
    QFETCH(int, blinkIndex);
    QFETCH(QString, onPhase);
    QFETCH(QString, offPhase);

    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.setDisplayMode(dot ? CircularBargraph::DisplayMode::Dot : CircularBargraph::DisplayMode::Bar);
    bargraph.resize(220, 220);
    const qreal dpr = bargraph.devicePixelRatioF();
    onPhase = expectedPixels(bargraph, onPhase);
    offPhase = expectedPixels(bargraph, offPhase);

    QElapsedTimer clock;
    clock.start(); // before entering the state: clock.elapsed() >= time in the state
    bargraph.setValue(input);
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), onPhase); // starts lit

    // Toggles at ~2 Hz (250 ms half period): never before 200 ms, and both phases are seen.
    qint64 firstToggleMs = -1;
    bool sawOff = false;
    bool sawOnAgain = false;
    while (clock.elapsed() < 3000 && !(sawOff && sawOnAgain)) {
        QTest::qWait(10);
        const QString states = sampleSegments(renderAt(bargraph, dpr), bargraph);
        QVERIFY2(states == onPhase || states == offPhase, qPrintable(states));
        if (states == offPhase && !sawOff) {
            sawOff = true;
            firstToggleMs = clock.elapsed();
        } else if (states == onPhase && sawOff) {
            sawOnAgain = true;
        }
    }
    QVERIFY2(sawOff && sawOnAgain, "the end segment did not blink");
    QVERIFY2(firstToggleMs >= 200, qPrintable(QStringLiteral("first toggle after %1 ms").arg(firstToggleMs)));
    QCOMPARE(onPhase.at(blinkIndex) != offPhase.at(blinkIndex), true);
}

void tst_CircularBargraph::segmentZonesByPosition()
{
    const auto zonesOf = [](double mn, double mx, double w, double e, int n) {
        QString out;
        for (const internal::Zone z : internal::computeSegmentZones(mn, mx, w, e, n)) {
            out += (z == internal::Zone::Error) ? QLatin1Char('E')
                 : (z == internal::Zone::Warning) ? QLatin1Char('W') : QLatin1Char('N');
        }
        return out;
    };
    // Centres 5, 15, ..., 95: a centre on a threshold belongs to the upper zone.
    QCOMPARE(zonesOf(0.0, 100.0, 35.0, 75.0, 10), QStringLiteral("NNNWWWWEEE"));
    QCOMPARE(zonesOf(0.0, 100.0, 36.0, 76.0, 10), QStringLiteral("NNNNWWWWEE"));
    // Thresholds outside the range, equal thresholds, offset range.
    QCOMPARE(zonesOf(0.0, 100.0, -10.0, 200.0, 10), QStringLiteral("WWWWWWWWWW"));
    QCOMPARE(zonesOf(0.0, 100.0, 200.0, 300.0, 10), QStringLiteral("NNNNNNNNNN"));
    QCOMPARE(zonesOf(0.0, 100.0, 50.0, 50.0, 10), QStringLiteral("NNNNNEEEEE"));
    QCOMPARE(zonesOf(-50.0, 50.0, -15.0, 25.0, 10), QStringLiteral("NNNWWWWEEE"));
    QCOMPARE(zonesOf(0.0, 100.0, 70.0, 85.0, 4), QStringLiteral("NNNE")); // 12.5 37.5 62.5 87.5
}

void tst_CircularBargraph::unlitTintColor()
{
    QCOMPARE(internal::unlitTint(QColor(200, 100, 0), QColor(40, 40, 40)), QColor(80, 55, 30));
    QCOMPARE(internal::unlitTint(QColor(255, 255, 255), QColor(0, 0, 0)), QColor(64, 64, 64));
    QCOMPARE(internal::unlitTint(QColor(10, 20, 30), QColor(10, 20, 30)), QColor(10, 20, 30));
}

void tst_CircularBargraph::zoneColorsByPositionPixels()
{
    // 10 segments, centres 5..95; warning 35, error 75 -> zones NNNWWWWEEE.
    CircularBargraph bargraph;
    applyTestColors(bargraph);
    bargraph.setSegmentCount(10);
    bargraph.setWarningThreshold(35.0);
    bargraph.setErrorThreshold(75.0);
    bargraph.resize(240, 240);
    const qreal dpr = bargraph.devicePixelRatioF();

    bargraph.setValue(60.0); // 6 lit: lit segments keep their own zone, unlit show the tint
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), QStringLiteral("NNNWWWweee"));

    bargraph.setValue(100.0);
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), QStringLiteral("NNNWWWWEEE"));

    bargraph.setValue(0.0);
    bargraph.resetPeak();
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), QStringLiteral("nnnwwwweee"));

    // Dot: the dot takes the zone of its own position.
    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    bargraph.setValue(40.0); // n = 4 -> segment 3
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), QStringLiteral("nnnWwwweee"));
    bargraph.setValue(20.0); // n = 2 -> segment 1
    bargraph.resetPeak();
    QCOMPARE(sampleSegments(renderAt(bargraph, dpr), bargraph), QStringLiteral("nNnwwwweee"));
}

void tst_CircularBargraph::peakDecayStepIndependentOfFma()
{
    // Exact steps.
    internal::PeakDecayStep s = internal::decayPeakStep(30.0, 20.0, 25.0, 0.2); // drop 5
    QCOMPARE(s.finished, false);
    QCOMPARE(s.peak, 25.0);
    s = internal::decayPeakStep(25.0, 20.0, 25.0, 0.2);
    QCOMPARE(s.finished, true);
    QCOMPARE(s.peak, 20.0);
    s = internal::decayPeakStep(21.0, 20.0, 25.0, 0.2); // overshoot clamps to the value
    QCOMPARE(s.finished, true);
    QCOMPARE(s.peak, 20.0);

    // 25 * 0.161 is not exact in binary. With the old form peak - rate * dt <= value, a build
    // that rounds the product first (no FMA) says decay has finished, a fused multiply-add says
    // it has not. The decision must not depend on how the compiler contracts the expression:
    // peak - value <= rate * dt gives the same answer in both builds (here: not finished, which
    // is also the exact-arithmetic answer for these inputs, though not for every input).
    s = internal::decayPeakStep(24.025000000000002, 20.0, 25.0, 0.161);
    QCOMPARE(s.finished, false);
    QVERIFY(s.peak >= 20.0);
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_CircularBargraph::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<CircularBargraph>();
        if (font) g->setFont(*font);
        g->resize(220, 220);
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

QTEST_MAIN(tst_CircularBargraph)
#include "tst_CircularBargraph.moc"
