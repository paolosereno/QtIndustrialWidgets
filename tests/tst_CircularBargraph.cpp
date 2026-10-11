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

    // NaN must trigger fail-safe hold (value unchanged)
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
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Dot);

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
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
    bargraph.show();

    const QList<QSize> sizes = {
        QSize(0, 0),
        QSize(1, 1),
        QSize(2, 2),
        QSize(4, 4),
        QSize(10, 1000),
        QSize(1000, 10),
        QSize(3840, 2160)
    };

    for (const QSize &sz : sizes) {
        bargraph.resize(sz);
        bargraph.repaint();
    }
}

void tst_CircularBargraph::offscreenRendering()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setValue(75.0);
    bargraph.setWarningThreshold(60.0);
    bargraph.setErrorThreshold(80.0);
    bargraph.resize(200, 200);

    QPixmap pixmap(200, 200);
    pixmap.fill(Qt::transparent);
    bargraph.render(&pixmap);

    QVERIFY(!pixmap.isNull());
    QCOMPARE(pixmap.size(), QSize(200, 200));

    // HiDPI offscreen check
    pixmap.setDevicePixelRatio(2.0);
    bargraph.render(&pixmap);
    QVERIFY(!pixmap.isNull());
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

QTEST_MAIN(tst_CircularBargraph)
#include "tst_CircularBargraph.moc"
