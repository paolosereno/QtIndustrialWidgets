// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/RadialGauge.h>

#include "FontTestUtils.h"
#include "LabelInkTestUtils.h"

#include <memory>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <limits>

#include "ReadoutTestUtils.h"
#include "TextInkTestUtils.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/ValueClassification.h"

using namespace QtIndustrialWidgets;
using namespace ReadoutTest;

namespace {

struct GaugeConfig {
    int side{300};
    double minimum{0.0};
    double maximum{100.0};
    int precision{1};
    double startAngle{-135.0};
    double spanAngle{270.0};
};

// The gauge rendered at the screen device pixel ratio (QT_SCALE_FACTOR in ctest)
QImage renderGauge(RadialGauge &gauge)
{
    return gauge.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

void configure(RadialGauge &gauge, const GaugeConfig &c)
{
    gauge.setRange(c.minimum, c.maximum);
    gauge.setPrecision(c.precision);
    gauge.setStartAngle(c.startAngle);
    gauge.setSpanAngle(c.spanAngle);
    gauge.resize(c.side, c.side);
}

// Pixels that differ between two renders of the same size
QVector<QPoint> differingPixels(const QImage &a, const QImage &b)
{
    QVector<QPoint> out;
    for (int y = 0; y < a.height(); ++y) {
        const auto *la = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const auto *lb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            if (la[x] != lb[x]) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

QRect boundingBox(const QVector<QPoint> &points)
{
    QRect box;
    for (const QPoint &p : points) {
        box |= QRect(p, QSize(1, 1));
    }
    return box;
}

// Device-pixel ink box of the digital value. The value is drawn in errorColor (value >= error
// threshold, bands hidden so that errorColor colours nothing else): a transparent errorColor
// renders everything but the value.
QRect valueInkBox(RadialGauge &gauge)
{
    gauge.setThresholdBandsVisible(false);
    gauge.setWarningThreshold(gauge.minimum() + (gauge.maximum() - gauge.minimum()) * 0.5);
    gauge.setErrorThreshold(gauge.minimum() + (gauge.maximum() - gauge.minimum()) * 0.6);
    gauge.setValue(gauge.maximum());
    gauge.setErrorColor(QColor(0, 0, 0, 0));
    const QImage withoutValue = renderGauge(gauge);
    gauge.setErrorColor(Qt::red);
    return boundingBox(differingPixels(withoutValue, renderGauge(gauge)));
}

// #151: a gauge whose needle and readout can be told apart in a render. The needle is pure blue
// (its facets are lighter and darker shades of it), a hue nothing else uses with the bands hidden;
// the readout stays in the near-white textColor (thresholds above the range), without a unit.
void prepareForReadout(RadialGauge &gauge)
{
    configure(gauge, GaugeConfig{300, 0.0, 100.0, 1, -135.0, 270.0});
    gauge.setUnit(QString());
    gauge.setThresholdBandsVisible(false);
    gauge.setWarningThreshold(1000.0);
    gauge.setErrorThreshold(2000.0);
    gauge.setNeedleColor(QColor(0, 0, 255));
}

bool isNeedlePixel(QRgb p)
{
    return qBlue(p) > 150 && qBlue(p) - qRed(p) > 80 && qBlue(p) - qGreen(p) > 80;
}

QVector<QPoint> needlePixels(const QImage &image)
{
    QVector<QPoint> out;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (isNeedlePixel(line[x])) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

// Needle pixels outside \a box. The antialiased edges of the needle blend with what lies under them, so where
// the needle crosses the digital display they change with the readout
QVector<QPoint> needlePixelsOutside(const QImage &image, const QRect &box)
{
    QVector<QPoint> out;
    for (const QPoint &p : needlePixels(image)) {
        if (!box.contains(p)) {
            out.push_back(p);
        }
    }
    return out;
}

// Device-pixel box of the digital display: the pixels that change when it is shown
QRect displayBox(RadialGauge &gauge)
{
    gauge.setDigitalDisplayVisible(false);
    const QImage without = renderGauge(gauge);
    gauge.setDigitalDisplayVisible(true);
    return boundingBox(differingPixels(without, renderGauge(gauge)));
}

// Number of scale pixels (those that change with the scale and zone colours while the display is hidden)
// that the display covers
int scalePixelsCovered(RadialGauge &gauge)
{
    auto colours = [](RadialGauge &g, const std::array<QColor, 4> &c) {
        g.setScaleColor(c[0]);
        g.setNormalColor(c[1]);
        g.setWarningColor(c[2]);
        g.setErrorColor(c[3]);
    };
    const std::array<QColor, 4> first{QColor(1, 200, 255), QColor(200, 1, 255), QColor(255, 200, 1), QColor(1, 255, 200)};
    const std::array<QColor, 4> second{QColor(255, 120, 0), QColor(0, 255, 120), QColor(120, 0, 255), QColor(255, 0, 120)};

    gauge.setDigitalDisplayVisible(false);
    colours(gauge, first);
    const QImage hiddenFirst = renderGauge(gauge);
    colours(gauge, second);
    const QVector<QPoint> scale = differingPixels(hiddenFirst, renderGauge(gauge));
    if (scale.size() <= 100) {
        return -1; // no scale found: the check would prove nothing
    }

    gauge.setDigitalDisplayVisible(true);
    colours(gauge, first);
    const QImage shownFirst = renderGauge(gauge);
    colours(gauge, second);
    const QImage shownSecond = renderGauge(gauge);
    int covered = 0;
    for (const QPoint &p : scale) {
        covered += shownFirst.pixel(p) == shownSecond.pixel(p) ? 1 : 0;
    }
    return covered;
}

// The readout alone, in readoutColor: thresholds above the range, no bands, and a needle that hides nothing
void isolateReadout(RadialGauge &gauge)
{
    const double top = gauge.maximum() + 2.0 * (gauge.maximum() - gauge.minimum());
    gauge.setThresholdBandsVisible(false);
    gauge.setWarningThreshold(top);
    gauge.setErrorThreshold(top * 2.0 + 1.0);
    gauge.setNeedleColor(QColor(0, 0, 0, 0));
}

// Device-pixel ink of the readout of \a input (isolateReadout() first)
QVector<QPoint> readoutInkOf(RadialGauge &gauge, double input)
{
    gauge.setValue(input);
    gauge.setReadoutColor(QColor(255, 0, 255));
    const QImage first = renderGauge(gauge);
    gauge.setReadoutColor(QColor(255, 255, 0));
    return differingPixels(first, renderGauge(gauge));
}

// Device-pixel box of the pod alone, its rim included: display box without the unit above it
QRect podBox(RadialGauge &gauge)
{
    const QRect display = displayBox(gauge);
    gauge.setBezelColor(QColor(200, 30, 30));
    const QImage first = renderGauge(gauge);
    gauge.setBezelColor(QColor(30, 200, 30));
    return boundingBox(differingPixels(first, renderGauge(gauge))).intersected(display);
}

// Device-pixel ink of the scale labels, with the render it comes from. Same state and same text colour for
// all the renders to compare: what differs on these pixels has been covered
struct LabelInkRender {
    QImage image;
    QVector<QPoint> ink;
};

LabelInkRender labelInk(RadialGauge &gauge)
{
    gauge.setTextColor(QColor(255, 255, 0));
    const QImage other = renderGauge(gauge);
    gauge.setTextColor(QColor(255, 0, 255));
    LabelInkRender out;
    out.image = renderGauge(gauge);
    out.ink = differingPixels(out.image, other);
    return out;
}

// Device-pixel box of the scale label of \a value on the gauge at its size
QRect labelBox(const RadialGauge &gauge, double value)
{
    const internal::RadialScaleGeometry geo = internal::radialScaleGeometry(std::min(gauge.width(), gauge.height()));
    QFont font = gauge.font();
    font.setPixelSize(geo.labelFontPx);
    font.setBold(true);
    const QRectF text = QFontMetricsF(font).boundingRect(internal::scaleLabelText(value, gauge.precision()));
    const double frac = (value - gauge.minimum()) / (gauge.maximum() - gauge.minimum());
    const double angle = (gauge.startAngle() + frac * gauge.spanAngle() - 90.0) * M_PI / 180.0;
    const double x = gauge.width() * 0.5 + geo.labelRadius * std::cos(angle) - text.width() * 0.5;
    const double y = gauge.height() * 0.5 + geo.labelRadius * std::sin(angle) - text.height() * 0.5;
    const double dpr = gauge.devicePixelRatioF();
    return QRectF((x - 1.0) * dpr, (y - 1.0) * dpr, (text.width() + 2.0) * dpr, (text.height() + 2.0) * dpr).toAlignedRect();
}

struct KeptInk {
    int total{0};
    int kept{0};
};

// How much of the ink of a label, rendered with the display hidden, is unchanged with the display shown
KeptInk keptInk(const LabelInkRender &hidden, const LabelInkRender &shown, const QRect &box)
{
    KeptInk out;
    for (const QPoint &p : hidden.ink) {
        if (box.contains(p)) {
            ++out.total;
            out.kept += hidden.image.pixel(p) == shown.image.pixel(p) ? 1 : 0;
        }
    }
    return out;
}

// The readout of an out-of-range input holds one more ink group than the same text read as a valid value:
// a triangle with one flat side and the other narrowing to the apex. With \a font set on both gauges
// when given
void checkOutOfRangeMark(const QFont *font)
{
    struct Case {
        double minimum;
        double maximum;
        double input;
        bool up;
    };
    const Case cases[] = {{0.0, 100.0, 150.0, true}, {0.0, 100.0, -50.0, false}};
    for (const Case &c : cases) {
        RadialGauge outOfRange;
        prepareForReadout(outOfRange);
        if (font) {
            outOfRange.setFont(*font);
        }
        outOfRange.setValue(50.0);
        const QRect box = displayBox(outOfRange);
        outOfRange.setValue(c.input);

        // The same text read as a valid value, on a range that holds it
        RadialGauge valid;
        prepareForReadout(valid);
        if (font) {
            valid.setFont(*font);
        }
        valid.setRange(std::min(c.minimum, c.input), std::max(c.maximum, c.input));
        valid.setValue(c.input);
        const QRect validBox = displayBox(valid);

        const QVector<InkGroup> marked = readoutInkGroups(renderGauge(outOfRange), box);
        const QVector<InkGroup> plain = readoutInkGroups(renderGauge(valid), validBox);
        const QString where = QStringLiteral("input %1").arg(c.input);
        QVERIFY2(marked.size() == plain.size() + 1,
                 qPrintable(where + QStringLiteral(": %1 ink groups, %2 for the valid text").arg(marked.size()).arg(plain.size())));

        // The triangle: one flat side, and the other narrowing to the apex
        const InkGroup &tri = marked.first();
        QVERIFY2(tri.top.size() >= 3, qPrintable(where));
        const int flat = c.up ? spread(tri.bottom) : spread(tri.top);
        const int pointed = c.up ? spread(tri.top) : spread(tri.bottom);
        QVERIFY2(flat <= 1 && pointed >= 2,
                 qPrintable(where + QStringLiteral(": flat side spread %1, pointed side %2").arg(flat).arg(pointed)));
    }
}

} // namespace

class tst_RadialGauge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void textSizeIgnoresWidgetFontSize();
    void fontChangeRefreshesCache();
    void labelsFollowWidgetFont();
    void initTestCase();
    void defaultValues();
    void rangeAndClamping();
    void invalidRangeIgnored();
    void signalEmission();
    void noDuplicateSignal();
    void thresholdZones();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void scaleLabelsNotHiddenByDigitalDisplay_data();
    void scaleLabelsNotHiddenByDigitalDisplay();
    void valueTextInsideDigitalDisplay_data();
    void valueTextInsideDigitalDisplay();
    void valueFontNotReducedWhenLoweringAvoidsIt();
    void valueReadoutHasNoNegativeZero_data();
    void valueReadoutHasNoNegativeZero();
    void scaleLabelsHaveNoNegativeZero();
    void scaleLabelsKeepMinimumGap_data();
    void scaleLabelsKeepMinimumGap();
    void digitalDisplayIgnoresUndrawnLabels();
    void digitalDisplayNeverCoversScale_data();
    void digitalDisplayNeverCoversScale();
    void minimumSizeKeepsIndicatorAndEnds_data();
    void minimumSizeKeepsIndicatorAndEnds();
    void minimumSizeHintLimits();
    void minimumSizeHintFollowsConfiguration_data();
    void minimumSizeHintFollowsConfiguration();
    void displayNeverCutsALabel_data();
    void displayNeverCutsALabel();
    void valueFontNeverBelow7pt();
    void outOfRangeReadoutNeverBelow7pt();
    void minimumSizeHintMakesRoomForAWideFont();
    void nanHidesNeedleAndShowsDashes();
    void nanEmitsValueChangedOnce();
    void outOfRangeReadoutShowsTheTrueValue();
    void infiniteReadoutDiffersFromTheBound();
    void rangeChangeReclassifiesTheLastInput();
    void valueStateAndRawValue();
    void valueStateSignalOrder();
    void invalidInputKeepsZoneSignals();
    void readoutTextOfEveryState();
    void outOfRangeMarkIsAShape();
    void outOfRangeMarkWithoutTheGlyphInTheFont();
    void longOutOfRangeReadoutStaysInThePod();
    void unitShownForAnInvalidInput();
    void initialValueFollowsTheRange();
};

void tst_RadialGauge::initTestCase()
{
}

void tst_RadialGauge::defaultValues()
{
    RadialGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.spanAngle(), 270.0);
    QCOMPARE(gauge.precision(), 1);
}

void tst_RadialGauge::rangeAndClamping()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 200.0);
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 200.0);

    // Value above maximum should be clamped to maximum
    gauge.setValue(250.0);
    QCOMPARE(gauge.value(), 200.0);

    // Value below minimum should be clamped to minimum
    gauge.setValue(-50.0);
    QCOMPARE(gauge.value(), 0.0);

    // Valid value inside range
    gauge.setValue(125.0);
    QCOMPARE(gauge.value(), 125.0);
}

void tst_RadialGauge::invalidRangeIgnored()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    // Setting min >= max should be safely rejected
    gauge.setRange(150.0, 50.0);
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
}

void tst_RadialGauge::signalEmission()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);

    QSignalSpy valueSpy(&gauge, &RadialGauge::valueChanged);
    gauge.setValue(45.0);

    QCOMPARE(valueSpy.count(), 1);
    QCOMPARE(valueSpy.takeFirst().at(0).toDouble(), 45.0);
}

void tst_RadialGauge::noDuplicateSignal()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(45.0);

    QSignalSpy valueSpy(&gauge, &RadialGauge::valueChanged);
    // Setting identical value should not emit duplicate signal
    gauge.setValue(45.0);
    QCOMPARE(valueSpy.count(), 0);
}

void tst_RadialGauge::thresholdZones()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setWarningThreshold(75.0);
    gauge.setErrorThreshold(90.0);

    QCOMPARE(gauge.warningThreshold(), 75.0);
    QCOMPARE(gauge.errorThreshold(), 90.0);
}

void tst_RadialGauge::renderOffscreen()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(60.0);
    gauge.resize(250, 250);

    QPixmap pix(gauge.size());
    pix.fill(Qt::transparent);
    gauge.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_RadialGauge::extremeResizeNoCrash()
{
    RadialGauge gauge;
    // Tiny size
    gauge.resize(2, 2);
    QPixmap pixSmall(gauge.size());
    gauge.render(&pixSmall);

    // 4K Ultra HD size
    gauge.resize(3840, 2160);
    QPixmap pix4k(gauge.size());
    gauge.render(&pix4k);
    QVERIFY(!pix4k.isNull());
}

void tst_RadialGauge::nanAndInfinityResilience()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(50.0);
    QCOMPARE(gauge.value(), 50.0);

    // NaN input is shown as invalid, never as the last value (#151; it was held before)
    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(std::isnan(gauge.value()));

    // +Infinity must clamp to maximum
    gauge.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 100.0);

    // -Infinity must clamp to minimum
    gauge.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 0.0);

    // Invalid range setters with NaN or Inf must be ignored
    gauge.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(gauge.minimum(), 0.0);
    gauge.setRange(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.maximum(), 100.0);

    // Render offscreen to ensure no NaN warnings or painter faults occur
    gauge.resize(200, 200);
    QPixmap pix(gauge.size());
    gauge.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_RadialGauge::scaleLabelsNotHiddenByDigitalDisplay_data()
{
    QTest::addColumn<int>("side");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("precision");
    QTest::addColumn<double>("startAngle");
    QTest::addColumn<double>("spanAngle");

    struct Range { const char *name; double min; double max; int precision; };
    const Range ranges[] = {{"0-100", 0.0, 100.0, 1}, {"0-8000", 0.0, 8000.0, 0}, {"-1000-1000", -1000.0, 1000.0, 0}};
    struct Arc { const char *name; double start; double span; };
    const Arc arcs[] = {{"270", -135.0, 270.0}, {"240", -120.0, 240.0}};
    for (int side : {200, 260, 300, 400}) {
        for (const Range &r : ranges) {
            for (const Arc &a : arcs) {
                QTest::addRow("%d px, %s, arc %s", side, r.name, a.name)
                    << side << r.min << r.max << r.precision << a.start << a.span;
            }
        }
    }
}

// #43: the digital display pod (and its unit text) must not paint over any scale label.
// Scale-label pixels are those that change with textColor while the display is hidden (textColor
// then colours only the labels); none of them may change when the display is shown.
void tst_RadialGauge::scaleLabelsNotHiddenByDigitalDisplay()
{
    QFETCH(int, side);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, precision);
    QFETCH(double, startAngle);
    QFETCH(double, spanAngle);
    const GaugeConfig config{side, minimum, maximum, precision, startAngle, spanAngle};

    RadialGauge gauge;
    configure(gauge, config);
    gauge.setUnit(QStringLiteral("RPM"));
    gauge.setValue(minimum);

    gauge.setDigitalDisplayVisible(false);
    gauge.setTextColor(Qt::white);
    const QImage hiddenWhite = renderGauge(gauge);
    gauge.setTextColor(Qt::red);
    const QImage hiddenRed = renderGauge(gauge);
    gauge.setTextColor(Qt::white);
    gauge.setDigitalDisplayVisible(true);
    const QImage shown = renderGauge(gauge);

    const QVector<QPoint> labelPixels = differingPixels(hiddenWhite, hiddenRed);
    QVERIFY2(labelPixels.size() > 100, qPrintable(QStringLiteral("only %1 label pixels").arg(labelPixels.size())));

    int hidden = 0;
    QRect hiddenBox;
    for (const QPoint &p : labelPixels) {
        if (shown.pixel(p) != hiddenWhite.pixel(p)) {
            ++hidden;
            hiddenBox |= QRect(p, QSize(1, 1));
        }
    }
    QVERIFY2(hidden == 0, qPrintable(QStringLiteral("%1 of %2 scale-label pixels painted over by the digital display, "
                                                    "device box %3,%4 %5x%6 (DPR %7)")
                                         .arg(hidden).arg(labelPixels.size())
                                         .arg(hiddenBox.x()).arg(hiddenBox.y()).arg(hiddenBox.width()).arg(hiddenBox.height())
                                         .arg(shown.devicePixelRatio())));
}

void tst_RadialGauge::valueTextInsideDigitalDisplay_data()
{
    QTest::addColumn<int>("side");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("precision");

    QTest::addRow("200 px, 100000") << 200 << 100000.0 << 0;
    QTest::addRow("200 px, 100000.0") << 200 << 100000.0 << 1;
    QTest::addRow("200 px, 1000000.00") << 200 << 1000000.0 << 2;
    QTest::addRow("200 px, 10000000.000") << 200 << 10000000.0 << 3; // no room even below the labels
    QTest::addRow("300 px, 8000") << 300 << 8000.0 << 0;
}

// #43: the value text is never cut or abbreviated: the pod contains the whole value, even when a
// value too wide for the space between the labels makes the pod grow over them (on main a value
// wider than the pod is clipped at its edges).
// The value is drawn in errorColor (value >= error threshold, bands hidden so that errorColor
// colours nothing else): a transparent errorColor renders the pod alone.
void tst_RadialGauge::valueTextInsideDigitalDisplay()
{
    QFETCH(int, side);
    QFETCH(double, maximum);
    QFETCH(int, precision);

    RadialGauge gauge;
    configure(gauge, GaugeConfig{side, 0.0, maximum, precision, -135.0, 270.0});
    gauge.setUnit(QString());
    gauge.setThresholdBandsVisible(false);
    gauge.setWarningThreshold(maximum * 0.5);
    gauge.setErrorThreshold(maximum * 0.6);
    gauge.setValue(maximum);
    QVERIFY(gauge.value() >= gauge.errorThreshold());

    gauge.setDigitalDisplayVisible(false);
    const QImage noDisplay = renderGauge(gauge);
    gauge.setDigitalDisplayVisible(true);
    gauge.setErrorColor(QColor(0, 0, 0, 0));
    const QImage podOnly = renderGauge(gauge);
    gauge.setErrorColor(Qt::red);
    const QImage podAndValue = renderGauge(gauge);

    const QRect pod = boundingBox(differingPixels(noDisplay, podOnly));
    const QVector<QPoint> valuePixels = differingPixels(podOnly, podAndValue);

    // #60: the number needs its width at 7 pt within the chord of the scale at the bottom of a pod as high as
    // the text, at its preferred place. Without that room it is left out and the state indicator stays
    const internal::RadialScaleGeometry geo = internal::radialScaleGeometry(side);
    QFont valueFont = gauge.font();
    valueFont.setBold(true);
    valueFont.setPixelSize(std::max(internal::minimumReadoutPixelSize(gauge.logicalDpiY()), geo.labelFontPx));
    const QFontMetricsF valueMetrics(valueFont);
    const double scaleInner = geo.majorTickInner - internal::podLabelMargin(geo.radius);
    const double podBottom = geo.radius * 0.28 + valueMetrics.height();
    const double room = 2.0 * std::sqrt(std::max(0.0, scaleInner * scaleInner - podBottom * podBottom));
    const double needed = std::max(valueMetrics.horizontalAdvance(internal::formatFixed(0.0, precision)),
                                   valueMetrics.horizontalAdvance(internal::formatFixed(maximum, precision)))
                          + 2.0 * std::max(3.0, geo.radius * 0.04);
    if (needed > room) {
        QVERIFY2(valuePixels.isEmpty(), qPrintable(QStringLiteral("%1 px of a number with no room (%2 px needed, %3 px free)")
                                                       .arg(valuePixels.size()).arg(needed).arg(room)));
        isolateReadout(gauge);
        for (double input : {std::numeric_limits<double>::quiet_NaN(), maximum * 2.0, -maximum}) {
            const QVector<QPoint> ink = readoutInkOf(gauge, input);
            QVERIFY2(ink.size() >= 3, qPrintable(QStringLiteral("no state indicator for input %1").arg(input)));
            QVERIFY2(pod.contains(boundingBox(ink)), qPrintable(QStringLiteral("indicator for input %1 leaves the display").arg(input)));
        }
        return;
    }

    QVERIFY2(valuePixels.size() > 20, qPrintable(QStringLiteral("only %1 value pixels").arg(valuePixels.size())));
    const QRect value = boundingBox(valuePixels);

    // A value wider than the pod is clipped at the pod rectangle, so its ink reaches the pod edge.
    // The pod keeps at least max(3 px, 0.04 r) of padding on each side of the value; 1 px of that is
    // left to antialiasing, so the ink must stay at least 2 logical px inside the pod.
    const qreal dpr = podAndValue.devicePixelRatio();
    const int margin = qCeil(2.0 * dpr);
    const QRect inner = pod.adjusted(margin, 0, -margin, 0);
    QVERIFY2(inner.contains(value), qPrintable(QStringLiteral("value text %1,%2 %3x%4 is not inside the pod %5,%6 %7x%8 "
                                                              "with a %9 px margin (device pixels, DPR %10)")
                                                   .arg(value.x()).arg(value.y()).arg(value.width()).arg(value.height())
                                                   .arg(pod.x()).arg(pod.y()).arg(pod.width()).arg(pod.height())
                                                   .arg(margin).arg(dpr)));
}

// The input of layoutDigitalPod() for a gauge at its size, as the widget builds it
internal::PodLayoutInput podInputOf(const RadialGauge &gauge)
{
    const internal::RadialScaleGeometry geo = internal::radialScaleGeometry(std::min(gauge.width(), gauge.height()));
    QFont labelFont = gauge.font();
    labelFont.setPixelSize(geo.labelFontPx);
    labelFont.setBold(true);
    const QFontMetricsF fm(labelFont);

    std::vector<internal::LayoutRect> boxes;
    const int ticks = gauge.majorTicks();
    const double range = gauge.maximum() - gauge.minimum();
    for (int i = 0; i <= ticks; ++i) {
        const double frac = static_cast<double>(i) / ticks;
        const double rad = (gauge.startAngle() + frac * gauge.spanAngle() - 90.0) * M_PI / 180.0;
        const QRectF text = fm.boundingRect(internal::scaleLabelText(gauge.minimum() + frac * range, gauge.precision()));
        boxes.push_back({gauge.width() * 0.5 + geo.labelRadius * std::cos(rad) - text.width() * 0.5,
                         gauge.height() * 0.5 + geo.labelRadius * std::sin(rad) - text.height() * 0.5, text.width(), text.height()});
    }

    internal::PodLayoutInput in;
    in.centerX = gauge.width() * 0.5;
    in.centerY = gauge.height() * 0.5;
    in.radius = geo.radius;
    for (int i : internal::layoutRadialScaleLabels(boxes, internal::scaleLabelMinGap(fm.height())).drawn) {
        in.labelRects.push_back(boxes[static_cast<size_t>(i)]);
    }
    in.padding = std::max(3.0, geo.radius * 0.04);
    in.minimumFontPx = std::max(internal::minimumReadoutPixelSize(gauge.logicalDpiY()), geo.labelFontPx);
    in.preferredFontPx = std::max(in.minimumFontPx, static_cast<int>(geo.radius * 0.15));
    const QStringList texts{internal::formatFixed(gauge.minimum(), gauge.precision()),
                            internal::formatFixed(gauge.maximum(), gauge.precision())};
    QFont valueFont = gauge.font();
    valueFont.setBold(true);
    in.valueWidth = [=](int px) mutable {
        valueFont.setPixelSize(px);
        const QFontMetricsF metrics(valueFont);
        return std::max(metrics.horizontalAdvance(texts[0]), metrics.horizontalAdvance(texts[1]));
    };
    in.valueHeight = [=](int px) mutable {
        valueFont.setPixelSize(px);
        return QFontMetricsF(valueFont).height();
    };
    in.scaleInnerRadius = geo.majorTickInner - internal::podLabelMargin(geo.radius);
    in.scaleStartAngle = gauge.startAngle();
    in.scaleSpanAngle = gauge.spanAngle();
    return in;
}

// True if a pod as high as the text, at \a px, has a place (preferred, or below the labels) where it touches
// neither the scale nor a label: the only reason left to draw the value smaller would be a defect
bool podHasRoomAt(const internal::PodLayoutInput &in, int px)
{
    const double m = internal::podLabelMargin(in.radius);
    const double half = in.valueWidth(px) * 0.5 + in.padding;
    const double height = in.valueHeight(px);
    const double preferredTop = in.centerY + in.radius * 0.28;
    double lowest = preferredTop;
    for (const internal::LayoutRect &label : in.labelRects) {
        if (label.bottom() > in.centerY && label.x < in.centerX + in.radius * 0.34 && label.right() > in.centerX - in.radius * 0.34) {
            lowest = std::max(lowest, label.bottom() + m);
        }
    }
    for (double top : {preferredTop, lowest}) {
        const internal::LayoutRect pod{in.centerX - half, top, 2.0 * half, height};
        const bool onLabel = std::any_of(in.labelRects.begin(), in.labelRects.end(), [&](const internal::LayoutRect &label) {
            return internal::LayoutRect{label.x - m, label.y - m, label.w + 2.0 * m, label.h + 2.0 * m}.intersects(pod);
        });
        const double inner = in.radius * 0.95 - m;
        const double farthest = std::hypot(half, std::max(top, pod.bottom()) - in.centerY);
        if (!onLabel && farthest < inner && !internal::podReachesScale(in, half, top, pod.bottom())) {
            return true;
        }
    }
    return false;
}

// #43 (review of PR #48): between the preferred pod place and the place below the labels, the one giving the
// larger value font is used. The reference is the same gauge with a 180-degree arc, whose end labels are level
// with the centre and leave the preferred place free. The value is as large as on the reference wherever the
// pod has room at that font without touching the scale (#60: the scale has precedence): this is computed from
// the geometry, and a gauge without that room says nothing here. The ink box of the same text may differ by
// 1 device px with the sub-pixel position of the text.
void tst_RadialGauge::valueFontNotReducedWhenLoweringAvoidsIt()
{
    int checked = 0;
    for (int side : {160, 180, 200, 220, 240, 260, 300}) {
        for (double maximum : {8000.0, 100000.0, 1000000.0}) {
            RadialGauge gauge;
            configure(gauge, GaugeConfig{side, 0.0, maximum, 0, -135.0, 270.0});
            gauge.setUnit(QString());
            RadialGauge reference;
            configure(reference, GaugeConfig{side, 0.0, maximum, 0, -90.0, 180.0});
            reference.setUnit(QString());

            const int referencePx = internal::layoutDigitalPod(podInputOf(reference)).valueFontPx;
            if (!podHasRoomAt(podInputOf(gauge), referencePx)) {
                qInfo("%d px, 0-%g: no room at %d px, value ink %dx%d device px, reference %dx%d", side, maximum, referencePx,
                      valueInkBox(gauge).width(), valueInkBox(gauge).height(), valueInkBox(reference).width(), valueInkBox(reference).height());
                continue;
            }
            ++checked;
            const QRect ink = valueInkBox(gauge);
            const QRect referenceInk = valueInkBox(reference);
            QVERIFY(!referenceInk.isEmpty());
            QVERIFY2(ink.height() >= referenceInk.height() - 1 && ink.width() >= referenceInk.width() - 1,
                     qPrintable(QStringLiteral("%1 px, 0-%2: value ink %3x%4 device px, reference (full font) %5x%6 (DPR %7)")
                                    .arg(side).arg(maximum).arg(ink.width()).arg(ink.height())
                                    .arg(referenceInk.width()).arg(referenceInk.height()).arg(gauge.grab().devicePixelRatio())));
        }
    }
    qInfo("%d of 21 gauges have room at the font of their reference", checked);
    QVERIFY(checked >= 3);
}

void tst_RadialGauge::valueReadoutHasNoNegativeZero_data()
{
    QTest::addColumn<int>("precision");
    QTest::addColumn<double>("magnitude");
    QTest::addRow("precision 1, 0.04") << 1 << 0.04;
    QTest::addRow("precision 0, 0.3") << 0 << 0.3;
}

// #46: a negative value that rounds to zero is shown without the sign: the readout of -v is drawn
// exactly like the readout of +v. The readout is the only text in textColor that depends on the
// value (thresholds above the range, so its colour stays textColor; the needle is transparent).
void tst_RadialGauge::valueReadoutHasNoNegativeZero()
{
    QFETCH(int, precision);
    QFETCH(double, magnitude);

    RadialGauge gauge;
    configure(gauge, GaugeConfig{300, -10.0, 10.0, precision, -135.0, 270.0});
    gauge.setUnit(QString());
    gauge.setWarningThreshold(9.0);
    gauge.setErrorThreshold(9.5);
    // Near zero the needle points straight up and covers the scale label "0" at 12 o'clock; at -v
    // and +v it covers slightly different label pixels (16 device px at DPR 1), which would make the
    // two text-ink sets differ for a reason unrelated to the readout. A transparent needle leaves
    // only the text to compare.
    gauge.setNeedleColor(QColor(0, 0, 0, 0));

    auto readoutInk = [&](double value) {
        gauge.setValue(value);
        gauge.setTextColor(Qt::white);
        const QImage white = renderGauge(gauge);
        gauge.setTextColor(Qt::red);
        return TextInk::sorted(TextInk::differingPixels(white, renderGauge(gauge)));
    };
    const QVector<QPoint> negative = readoutInk(-magnitude);
    const QVector<QPoint> positive = readoutInk(magnitude);
    QVERIFY(!positive.isEmpty());
    QVERIFY2(negative == positive,
             qPrintable(QStringLiteral("text ink differs between %1 and %2 (%3 vs %4 pixels, precision %5)")
                            .arg(-magnitude).arg(magnitude).arg(negative.size()).arg(positive.size()).arg(precision)));
}

// #46: range -0.1..0.1 with 5 major ticks: tick 2 is -0.02 and tick 3 is +0.02, both "0.0". The two
// labels sit symmetrically at -27 and +27 degrees (labels are 54 degrees apart); the label ink within
// +-15 degrees of each tick must be equally wide (a minus sign makes tick 2 wider). 1 device px is
// left to the sub-pixel position of the glyphs.
void tst_RadialGauge::scaleLabelsHaveNoNegativeZero()
{
    RadialGauge gauge;
    configure(gauge, GaugeConfig{300, -0.1, 0.1, 1, -135.0, 270.0});
    gauge.setMajorTicks(5);
    gauge.setDigitalDisplayVisible(false);

    gauge.setTextColor(Qt::white);
    const QImage white = renderGauge(gauge);
    gauge.setTextColor(Qt::red);
    const QImage red = renderGauge(gauge);
    const qreal dpr = white.devicePixelRatio();
    const QPointF centre(white.width() * 0.5, white.height() * 0.5);

    const QVector<QPoint> ink = TextInk::differingPixels(white, red);
    auto inkInSector = [&](double angleDeg) {
        QRect box;
        for (const QPoint &p : ink) {
            const double a = std::atan2(p.x() + 0.5 - centre.x(), centre.y() - (p.y() + 0.5)) * 180.0 / M_PI;
            if (std::abs(std::remainder(a - angleDeg, 360.0)) <= 15.0) {
                box |= QRect(p, QSize(1, 1));
            }
        }
        return box;
    };
    const QRect tick2 = inkInSector(-27.0);
    const QRect tick3 = inkInSector(27.0);
    QVERIFY(!tick2.isEmpty());
    QVERIFY(!tick3.isEmpty());
    QVERIFY2(std::abs(tick2.width() - tick3.width()) <= 1,
             qPrintable(QStringLiteral("label at -0.02 is %1 device px wide, label at +0.02 is %2 (DPR %3)")
                            .arg(tick2.width()).arg(tick3.width()).arg(dpr)));
}


void tst_RadialGauge::scaleLabelsKeepMinimumGap_data()
{
    QTest::addColumn<int>("side");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("precision");

    struct Range { const char *name; double min; double max; int precision; };
    // Wide values that overlap on main (#50) and narrow ones where every label fits
    const Range ranges[] = {{"0-100000", 0.0, 100000.0, 0}, {"0-10000000", 0.0, 10000000.0, 0},
                            {"0-100", 0.0, 100.0, 1}, {"-1000-1000", -1000.0, 1000.0, 0}};
    for (int side : {200, 300}) {
        for (const Range &r : ranges) {
            QTest::addRow("%d px, %s", side, r.name) << side << r.min << r.max << r.precision;
        }
    }
}

// #50: drawn scale labels keep the LinearGauge minimum gap (scaleLabelMinGap() on the label font
// height) and the first and last labels are always drawn. Label pixels are those that change with
// textColor while the display is hidden. They are grouped into clusters: two pixels belong to the
// same cluster when fewer than (minGap - 1) x DPR free device pixels separate them along the axis
// that separates them (1 px logical allowed for glyphs that exceed the font bounding rectangle the
// layout uses). The glyphs of one label are closer than that, so each label is one cluster; two
// labels closer than the minimum merge into one cluster, which is then larger than any label
// drawn alone (+1 px logical).
void tst_RadialGauge::scaleLabelsKeepMinimumGap()
{
    QFETCH(int, side);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, precision);
    const GaugeConfig config{side, minimum, maximum, precision, -135.0, 270.0};

    RadialGauge gauge;
    configure(gauge, config);
    gauge.setDigitalDisplayVisible(false);
    gauge.setValue(minimum);
    gauge.setTextColor(Qt::white);
    const QImage white = renderGauge(gauge);
    gauge.setTextColor(Qt::red);
    const QImage red = renderGauge(gauge);
    const qreal dpr = white.devicePixelRatio();
    const QVector<QPoint> ink = differingPixels(white, red);
    QVERIFY2(ink.size() > 100, qPrintable(QStringLiteral("only %1 label pixels").arg(ink.size())));

    const internal::RadialScaleGeometry geo = internal::radialScaleGeometry(side);
    QFont labelFont = gauge.font();
    labelFont.setPixelSize(geo.labelFontPx);
    labelFont.setBold(true);
    const double minGap = internal::scaleLabelMinGap(QFontMetricsF(labelFont).height());
    const double threshold = (minGap - 1.0) * dpr;

    // Largest label drawn alone. Every row has integer tick values, which the gauge prints as
    // integers at any precision (scaleLabelText())
    const int majorTicks = gauge.majorTicks();
    QSize largest;
    for (int i = 0; i <= majorTicks; ++i) {
        const double v = minimum + (maximum - minimum) * i / majorTicks;
        QCOMPARE(v, std::round(v));
        largest = largest.expandedTo(LabelInk::inkBoxAlone(QString::number(static_cast<qint64>(v)), labelFont, dpr).size());
    }
    const int tolerance = static_cast<int>(std::ceil(dpr));

    const QVector<QRect> clusters = LabelInk::inkClusters(ink, threshold);

    for (const QRect &c : qAsConst(clusters)) {
        QVERIFY2(c.width() <= largest.width() + tolerance && c.height() <= largest.height() + tolerance,
                 qPrintable(QStringLiteral("label ink %1x%2 at (%3, %4) is larger than the largest label alone %5x%6: "
                                           "labels closer than %7 device px (minimum gap %8 px, DPR %9)")
                                .arg(c.width()).arg(c.height()).arg(c.x()).arg(c.y())
                                .arg(largest.width()).arg(largest.height()).arg(threshold).arg(minGap).arg(dpr)));
    }

    // The first and last labels: the cluster nearest to the label centre is nearer to it than to the
    // centre of any other label, i.e. it is that label (the glyph ink need not contain the centre:
    // with the box glyphs of a platform without fonts it lies above the middle of the line)
    const QPointF centre(side * 0.5 * dpr, side * 0.5 * dpr);
    std::vector<QPointF> labelCentres;
    for (int i = 0; i <= majorTicks; ++i) {
        const double rad = (gauge.startAngle() + gauge.spanAngle() * i / majorTicks - 90.0) * M_PI / 180.0;
        labelCentres.push_back(centre + QPointF(std::cos(rad), std::sin(rad)) * geo.labelRadius * dpr);
    }
    auto distance = [](const QRect &r, const QPointF &p) {
        const double dx = std::max({static_cast<double>(r.left()) - p.x(), 0.0, p.x() - (r.right() + 1)});
        const double dy = std::max({static_cast<double>(r.top()) - p.y(), 0.0, p.y() - (r.bottom() + 1)});
        return std::hypot(dx, dy);
    };
    for (int i : {0, majorTicks}) {
        const QPointF p = labelCentres[static_cast<size_t>(i)];
        QRect nearest;
        double best = std::numeric_limits<double>::infinity();
        for (const QRect &c : qAsConst(clusters)) {
            if (distance(c, p) < best) {
                best = distance(c, p);
                nearest = c;
            }
        }
        int owner = i;
        for (int j = 0; j <= majorTicks; ++j) {
            if (distance(nearest, labelCentres[static_cast<size_t>(j)]) < distance(nearest, labelCentres[static_cast<size_t>(owner)])) {
                owner = j;
            }
        }
        QVERIFY2(owner == i,
                 qPrintable(QStringLiteral("label %1 is not drawn: the nearest ink %2x%3 at (%4, %5), %6 device px from its "
                                           "centre (%7, %8), is nearer to label %9")
                                .arg(i).arg(nearest.width()).arg(nearest.height()).arg(nearest.x()).arg(nearest.y())
                                .arg(best).arg(p.x()).arg(p.y()).arg(owner)));
    }
}

void tst_RadialGauge::digitalDisplayNeverCoversScale_data()
{
    QTest::addColumn<int>("side");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<double>("startAngle");
    QTest::addColumn<double>("spanAngle");
    QTest::addColumn<QString>("unit");

    struct Range { const char *name; double min; double max; };
    const Range ranges[] = {{"-100000-100000", -100000.0, 100000.0}, {"0-10000000", 0.0, 10000000.0},
                            {"0-100", 0.0, 100.0}, {"-1000-1000", -1000.0, 1000.0}, {"0-100000", 0.0, 100000.0}};
    struct Arc { double start; double span; };
    for (const Arc arc : {Arc{0.0, 360.0}, Arc{-180.0, 360.0}, Arc{-150.0, 300.0}, Arc{-135.0, 270.0}, Arc{-120.0, 240.0}}) {
        for (int side : {90, 100, 120, 160, 200, 300}) {
            for (const Range &r : ranges) {
                for (const char *unit : {"", "RPM"}) {
                    QTest::addRow("%g+%g, %d px, %s %s", arc.start, arc.span, side, r.name, unit)
                        << side << r.min << r.max << arc.start << arc.span << QString::fromLatin1(unit);
                }
            }
        }
    }
}

// #60(a): the pod and the unit never cover the threshold band or a tick. The scale pixels are those
// that change with the scale and zone colours while the display is hidden; with the display
// shown they must all still change.
void tst_RadialGauge::digitalDisplayNeverCoversScale()
{
    QFETCH(int, side);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(double, startAngle);
    QFETCH(double, spanAngle);
    QFETCH(QString, unit);

    RadialGauge gauge;
    configure(gauge, GaugeConfig{side, minimum, maximum, 0, startAngle, spanAngle});
    gauge.setUnit(unit);
    gauge.setValue(minimum);
    gauge.setWarningThreshold(minimum + 0.6 * (maximum - minimum));
    gauge.setErrorThreshold(minimum + 0.8 * (maximum - minimum));

    const int covered = scalePixelsCovered(gauge);
    QVERIFY2(covered == 0, qPrintable(QStringLiteral("%1 scale pixels covered").arg(covered)));
}

namespace {

struct ScaleRange {
    const char *name;
    double minimum;
    double maximum;
    int precision;
};
const ScaleRange kScaleRanges[] = {{"0-100.0", 0.0, 100.0, 1},          {"0-100", 0.0, 100.0, 0},
                                   {"0-250.0", 0.0, 250.0, 1},          {"0-8000", 0.0, 8000.0, 0},
                                   {"-1000-1000", -1000.0, 1000.0, 0},  {"0-100000", 0.0, 100000.0, 0},
                                   {"-100000-100000", -100000.0, 100000.0, 0}, {"0-10000000", 0.0, 10000000.0, 0}};
struct ScaleArc {
    double start;
    double span;
};
const ScaleArc kScaleArcs[] = {{-135.0, 270.0}, {-120.0, 240.0}, {0.0, 360.0}};

void addScaleColumns()
{
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("precision");
    QTest::addColumn<double>("startAngle");
    QTest::addColumn<double>("spanAngle");
    QTest::addColumn<QString>("unit");
}

void addScaleRows()
{
    for (const ScaleArc arc : kScaleArcs) {
        for (const ScaleRange &r : kScaleRanges) {
            for (const char *unit : {"", "RPM"}) {
                QTest::addRow("%g+%g, %s %s", arc.start, arc.span, r.name, unit)
                    << r.minimum << r.maximum << r.precision << arc.start << arc.span << QString::fromLatin1(unit);
            }
        }
    }
}

} // namespace

void tst_RadialGauge::minimumSizeKeepsIndicatorAndEnds_data()
{
    addScaleColumns();
    addScaleRows();
}

// #60(a): from minimumSizeHint() up, Invalid, Underrange and Overrange show their indicator inside the pod,
// the display covers no scale pixel, and the first and last labels are whole. The last label is left out only
// on a span where it lies on the first one. When even sizeHint() cannot keep both ends the result is
// sizeHint() (documented): the indicator and the scale are still checked.
void tst_RadialGauge::minimumSizeKeepsIndicatorAndEnds()
{
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, precision);
    QFETCH(double, startAngle);
    QFETCH(double, spanAngle);
    QFETCH(QString, unit);

    RadialGauge gauge;
    configure(gauge, GaugeConfig{300, minimum, maximum, precision, startAngle, spanAngle});
    gauge.setUnit(unit);
    const int hint = gauge.minimumSizeHint().width();
    QCOMPARE(gauge.minimumSizeHint().height(), hint);
    const int ceiling = gauge.sizeHint().width();
    const bool endsRequired = hint < ceiling;
    const double range = maximum - minimum;

    for (int side : {hint, hint + 1, hint + 2, hint + 5, hint + 10, hint + 20, hint + 40, ceiling, 300, 400}) {
        if (side < hint) {
            continue;
        }
        const QString where = QStringLiteral("side %1, hint %2: ").arg(side).arg(hint);
        gauge.resize(side, side);
        gauge.setValue(minimum);
        gauge.setWarningThreshold(minimum + 0.6 * range);
        gauge.setErrorThreshold(minimum + 0.8 * range);
        gauge.setThresholdBandsVisible(true);
        const int covered = scalePixelsCovered(gauge);
        QVERIFY2(covered == 0, qPrintable(where + QStringLiteral("%1 scale pixels covered").arg(covered)));

        isolateReadout(gauge);
        for (double input : {std::numeric_limits<double>::quiet_NaN(), maximum + range, minimum - range}) {
            const QString state = where + QStringLiteral("input %1: ").arg(input);
            const QVector<QPoint> ink = readoutInkOf(gauge, input);
            QVERIFY2(ink.size() >= 3, qPrintable(state + QStringLiteral("no state indicator")));
            const QRect pod = podBox(gauge);
            const QRect box = boundingBox(ink);
            QVERIFY2(pod.contains(box),
                     qPrintable(state + QStringLiteral("readout %1,%2 %3x%4 outside the pod %5,%6 %7x%8")
                                    .arg(box.x()).arg(box.y()).arg(box.width()).arg(box.height())
                                    .arg(pod.x()).arg(pod.y()).arg(pod.width()).arg(pod.height())));
        }

        if (!endsRequired) {
            continue;
        }
        gauge.setValue(std::numeric_limits<double>::quiet_NaN());
        gauge.setDigitalDisplayVisible(false);
        const LabelInkRender hidden = labelInk(gauge);
        gauge.setDigitalDisplayVisible(true);
        const LabelInkRender shown = labelInk(gauge);
        const KeptInk first = keptInk(hidden, shown, labelBox(gauge, minimum));
        QVERIFY2(first.total > 0 && first.kept == first.total,
                 qPrintable(where + QStringLiteral("first label: %1 of %2 pixels").arg(first.kept).arg(first.total)));
        if (spanAngle < 360.0) {
            const KeptInk last = keptInk(hidden, shown, labelBox(gauge, maximum));
            QVERIFY2(last.total > 0 && last.kept == last.total,
                     qPrintable(where + QStringLiteral("last label: %1 of %2 pixels").arg(last.kept).arg(last.total)));
        }
    }
}

// #60(a): the limits of minimumSizeHint(): not below the former fixed 90 x 90, not above sizeHint()
void tst_RadialGauge::minimumSizeHintLimits()
{
    for (const ScaleArc arc : kScaleArcs) {
        for (double maximum : {1.0, 100.0, 1e7, 1e15, 1e30}) {
            for (int precision : {0, 6}) {
                RadialGauge gauge;
                gauge.setRange(0.0, maximum);
                gauge.setPrecision(precision);
                gauge.setStartAngle(arc.start);
                gauge.setSpanAngle(arc.span);
                gauge.setUnit(QStringLiteral("kWh/m3"));
                const int hint = gauge.minimumSizeHint().width();
                QVERIFY2(hint >= 90 && hint <= gauge.sizeHint().width(),
                         qPrintable(QStringLiteral("span %1, 0-%2, precision %3: %4").arg(arc.span).arg(maximum).arg(precision).arg(hint)));
            }
        }
    }
    // Both ends of a 270-degree scale have to fit around the display; on a full turn the first one is enough
    RadialGauge wide;
    configure(wide, GaugeConfig{300, 0.0, 100000.0, 0, -135.0, 270.0});
    wide.setUnit(QStringLiteral("RPM"));
    RadialGauge turn;
    configure(turn, GaugeConfig{300, 0.0, 100000.0, 0, 0.0, 360.0});
    turn.setUnit(QStringLiteral("RPM"));
    QVERIFY2(wide.minimumSizeHint().width() > turn.minimumSizeHint().width(),
             qPrintable(QStringLiteral("270 degrees: %1, 360 degrees: %2")
                            .arg(wide.minimumSizeHint().width()).arg(turn.minimumSizeHint().width())));
    QVERIFY(turn.minimumSizeHint().width() < turn.sizeHint().width());
}

void tst_RadialGauge::minimumSizeHintFollowsConfiguration_data()
{
    using Change = std::function<void(RadialGauge &)>;
    QTest::addColumn<Change>("change");
    QTest::addColumn<bool>("hintChanges");

    QTest::addRow("range") << Change([](RadialGauge &g) { g.setRange(0.0, 10000000.0); }) << true;
    QTest::addRow("maximum") << Change([](RadialGauge &g) { g.setMaximum(10000000.0); }) << true;
    QTest::addRow("precision") << Change([](RadialGauge &g) { g.setPrecision(6); }) << true;
    QTest::addRow("unit") << Change([](RadialGauge &g) { g.setUnit(QStringLiteral("kWh/m3 per hour")); }) << true;
    QTest::addRow("start angle") << Change([](RadialGauge &g) { g.setStartAngle(-90.0); }) << true;
    QTest::addRow("span angle") << Change([](RadialGauge &g) { g.setSpanAngle(240.0); }) << true;
    QTest::addRow("major ticks") << Change([](RadialGauge &g) { g.setMajorTicks(4); }) << false;
    QTest::addRow("display hidden") << Change([](RadialGauge &g) { g.setDigitalDisplayVisible(false); }) << true;
    QTest::addRow("font") << Change([](RadialGauge &g) { g.setFont(FontTest::otherFont(g.font())); }) << false;
}

// #60(a): minimumSizeHint() is that of a new gauge with the same configuration after every setter it depends
// on, and the layout that holds the gauge sees it without being invalidated by hand (updateGeometry()).
void tst_RadialGauge::minimumSizeHintFollowsConfiguration()
{
    using Change = std::function<void(RadialGauge &)>;
    QFETCH(Change, change);
    QFETCH(bool, hintChanges);

    QWidget parent;
    auto *layout = new QVBoxLayout(&parent);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *gauge = new RadialGauge(&parent);
    gauge->setRange(0.0, 100.0);
    layout->addWidget(gauge);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));

    const int before = gauge->minimumSizeHint().width();
    QCOMPARE(layout->minimumSize().width(), before);

    change(*gauge);
    const int after = gauge->minimumSizeHint().width();

    RadialGauge fresh;
    fresh.setRange(0.0, 100.0);
    change(fresh);
    QCOMPARE(after, fresh.minimumSizeHint().width());
    if (hintChanges) {
        QVERIFY2(after != before, qPrintable(QStringLiteral("still %1").arg(after)));
    }
    QCOMPARE(layout->minimumSize().width(), after);
}

void tst_RadialGauge::displayNeverCutsALabel_data()
{
    QTest::addColumn<int>("side");
    addScaleColumns();
    for (int side : {90, 100, 120, 160, 200, 300}) {
        for (const ScaleArc arc : kScaleArcs) {
            for (const ScaleRange &r : kScaleRanges) {
                for (const char *unit : {"", "RPM"}) {
                    QTest::addRow("%d px, %g+%g, %s %s", side, arc.start, arc.span, r.name, unit)
                        << side << r.minimum << r.maximum << r.precision << arc.start << arc.span << QString::fromLatin1(unit);
                }
            }
        }
    }
}

// #43, #60(a): the value has precedence over the labels, but a label is never cut: with the display shown each
// one is either whole or not drawn at all, at every size, also below minimumSizeHint()
void tst_RadialGauge::displayNeverCutsALabel()
{
    QFETCH(int, side);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, precision);
    QFETCH(double, startAngle);
    QFETCH(double, spanAngle);
    QFETCH(QString, unit);

    RadialGauge gauge;
    configure(gauge, GaugeConfig{side, minimum, maximum, precision, startAngle, spanAngle});
    gauge.setUnit(unit);
    isolateReadout(gauge);
    gauge.setValue(minimum + 0.5 * (maximum - minimum));

    gauge.setDigitalDisplayVisible(false);
    const LabelInkRender hidden = labelInk(gauge);
    gauge.setDigitalDisplayVisible(true);
    const LabelInkRender shown = labelInk(gauge);
    const int ticks = gauge.majorTicks();
    for (int i = 0; i <= ticks; ++i) {
        const double value = minimum + (maximum - minimum) * i / ticks;
        const KeptInk ink = keptInk(hidden, shown, labelBox(gauge, value));
        QVERIFY2(ink.kept == 0 || ink.kept == ink.total,
                 qPrintable(QStringLiteral("label %1 cut: %2 of %3 pixels").arg(value).arg(ink.kept).arg(ink.total)));
    }
}

// #60(a): the value is never below 7 pt: wherever it is drawn, its digits are as high as at 7 pt
void tst_RadialGauge::valueFontNeverBelow7pt()
{
    int checked = 0;
    for (int side : {90, 100, 110, 120, 140, 160}) {
        for (double maximum : {100.0, 8000.0, 100000.0}) {
            RadialGauge gauge;
            configure(gauge, GaugeConfig{side, 0.0, maximum, 0, -135.0, 270.0});
            gauge.setUnit(QString());
            const QRect ink = valueInkBox(gauge);
            if (ink.isEmpty()) {
                continue; // no room: left out, never smaller (see minimumSizeKeepsIndicatorAndEnds)
            }
            ++checked;
            QFont font = gauge.font();
            font.setPixelSize(internal::minimumReadoutPixelSize(gauge.logicalDpiY()));
            font.setBold(true);
            const QRect reference = LabelInk::inkBoxAlone(internal::formatFixed(maximum, 0), font, gauge.devicePixelRatioF());
            // The height of the ink can vary by 1 device px with the vertical sub-pixel position of the text
            QVERIFY2(ink.height() >= reference.height() - 1,
                     qPrintable(QStringLiteral("%1 px, 0-%2: ink %3 px high, %4 at 7 pt").arg(side).arg(maximum).arg(ink.height()).arg(reference.height())));
        }
    }
    QVERIFY(checked >= 6);
}

// #50: the digital display is laid out against the drawn labels only. On a 360-degree arc from
// -100000 to 100000 with 10 major ticks, every other label is drawn (0, 2, ..., 8; the last one
// lies on the first): the same labels as with 5 major ticks. The label under the pod (index 5, "0")
// is not drawn, so the pod and the value must be exactly those of the 5-tick gauge.
void tst_RadialGauge::digitalDisplayIgnoresUndrawnLabels()
{
    auto make = [](int majorTicks) {
        auto g = std::make_unique<RadialGauge>();
        configure(*g, GaugeConfig{300, -100000.0, 100000.0, 0, 0.0, 360.0});
        g->setMajorTicks(majorTicks);
        g->setUnit(QStringLiteral("RPM"));
        return g;
    };
    auto labelPixels = [](RadialGauge &g) {
        g.setDigitalDisplayVisible(false);
        g.setTextColor(Qt::white);
        const QImage white = renderGauge(g);
        g.setTextColor(Qt::red);
        const QImage red = renderGauge(g);
        g.setDigitalDisplayVisible(true);
        return differingPixels(white, red);
    };

    auto ten = make(10);
    auto five = make(5);
    const QVector<QPoint> tenLabels = labelPixels(*ten);
    QVERIFY(tenLabels.size() > 100);
    QVERIFY2(tenLabels == labelPixels(*five), "the 10-tick gauge does not draw the labels of the 5-tick gauge");

    const QRect tenValue = valueInkBox(*ten);
    const QRect fiveValue = valueInkBox(*five);
    QVERIFY(!fiveValue.isEmpty());
    QVERIFY2(tenValue == fiveValue,
             qPrintable(QStringLiteral("value ink %1x%2 at (%3, %4) with 10 ticks, %5x%6 at (%7, %8) with 5")
                            .arg(tenValue.width()).arg(tenValue.height()).arg(tenValue.x()).arg(tenValue.y())
                            .arg(fiveValue.width()).arg(fiveValue.height()).arg(fiveValue.x()).arg(fiveValue.y())));
}

// #60(a): an out-of-range readout that does not fit is left out, the triangle staying, rather than drawn smaller:
// what remains is never lower than the triangle at 7 pt
void tst_RadialGauge::outOfRangeReadoutNeverBelow7pt()
{
    for (int side : {90, 100, 120, 160, 200}) {
        for (double maximum : {100.0, 8000.0, 100000.0}) {
            for (double factor : {1.5, 1000.0, 1.0e6}) {
                RadialGauge gauge;
                configure(gauge, GaugeConfig{side, 0.0, maximum, 0, -135.0, 270.0});
                gauge.setUnit(QString());
                isolateReadout(gauge);
                const QRect ink = boundingBox(readoutInkOf(gauge, maximum * factor));
                QFont font = gauge.font();
                font.setPixelSize(internal::minimumReadoutPixelSize(gauge.logicalDpiY()));
                font.setBold(true);
                const double triangle = QFontMetricsF(font).capHeight() * 0.9 * gauge.devicePixelRatioF();
                QVERIFY2(ink.height() >= std::floor(triangle) - 1,
                         qPrintable(QStringLiteral("%1 px, input %2: ink %3 px high, the triangle at 7 pt %4")
                                        .arg(side).arg(maximum * factor).arg(ink.height()).arg(triangle)));
            }
        }
    }
}

// #60(a): minimumSizeHint() makes room for the state indicator: with fonts so wide that "---" does not fit at
// 90 px, the indicator stays clear of the scale at the size it returns. The width of a stretched font depends on
// the platform, so several are tried: those that reach the ceiling say nothing, and at least one must not
void tst_RadialGauge::minimumSizeHintMakesRoomForAWideFont()
{
    int checked = 0;
    for (int stretch : {150, 200, 250, 300, 400}) {
        RadialGauge gauge;
        configure(gauge, GaugeConfig{300, 0.0, 100.0, 0, 0.0, 360.0});
        gauge.setUnit(QString());
        QFont wide = gauge.font();
        wide.setStretch(stretch);
        gauge.setFont(wide);

        const int hint = gauge.minimumSizeHint().width();
        if (hint <= 90 || hint >= gauge.sizeHint().width()) {
            continue;
        }
        ++checked;
        gauge.resize(hint, hint);
        gauge.setWarningThreshold(60.0);
        gauge.setErrorThreshold(80.0);
        gauge.setValue(0.0);
        const int covered = scalePixelsCovered(gauge);
        QVERIFY2(covered == 0, qPrintable(QStringLiteral("stretch %1: %2 scale pixels covered at %3 px").arg(stretch).arg(covered).arg(hint)));
        isolateReadout(gauge);
        QVERIFY(readoutInkOf(gauge, std::numeric_limits<double>::quiet_NaN()).size() >= 3);
    }
    QVERIFY2(checked > 0, "no stretched font gives a size between the floor and the ceiling");
}

// #51: the scale labels (digital display hidden) are drawn into the static cache with the widget font (family, style, spacing),
// not with the application font: a widget created with another font of the same size draws other
// text pixels. Before #51 the cached text used painter.font() (the application font on a pixmap)
void tst_RadialGauge::labelsFollowWidgetFont()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<RadialGauge>();
        if (font) g->setFont(*font);
        g->setDigitalDisplayVisible(false);
        g->resize(300, 300);
        return g;
    };
    auto standard = make(nullptr);
    const QFont other = FontTest::otherFont(standard->font());
    auto changed = make(&other);
    const int pixels = FontTest::differingPixelCount(FontTest::render(*standard), FontTest::render(*changed));
    QVERIFY2(pixels > 100, qPrintable(QStringLiteral("only %1 pixels change with the widget font").arg(pixels)));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_RadialGauge::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<RadialGauge>();
        if (font) g->setFont(*font);
        g->resize(300, 300);
        return g;
    };
    // Shown widgets: grab() on a hidden one delivers a pending resize event, which refreshes the
    // cache by itself and hides the defect
    auto painted = make(nullptr);
    painted->show();
    QVERIFY(QTest::qWaitForWindowExposed(painted.get()));
    const QFont other = FontTest::otherFont(painted->font());
    const QImage before = FontTest::render(*painted);
    painted->setFont(other);
    QCoreApplication::processEvents();
    const QImage after = FontTest::render(*painted);
    // One window at a time: the active one has the focus (IndustrialKnob draws a focus ring)
    painted->hide();
    auto fresh = make(&other);
    fresh->show();
    QVERIFY(QTest::qWaitForWindowExposed(fresh.get()));
    const QImage expected = FontTest::render(*fresh);
    QVERIFY2(FontTest::differingPixelCount(before, expected) > 100, "the font change is not visible: the test checks nothing");
    const int stale = FontTest::differingPixelCount(after, expected);
    QVERIFY2(stale == 0, qPrintable(QStringLiteral("%1 pixels differ from a widget created with the new font").arg(stale)));
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_RadialGauge::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<RadialGauge>();
        if (font) g->setFont(*font);
        g->setDigitalDisplayVisible(false);
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

// #151: a NaN input is shown as invalid, never as the last value: no needle, readout "---"
void tst_RadialGauge::nanHidesNeedleAndShowsDashes()
{
    RadialGauge gauge;
    prepareForReadout(gauge);
    gauge.setValue(50.0);
    const QRect box = displayBox(gauge);
    QVERIFY(!box.isEmpty());

    const QImage valid = renderGauge(gauge);
    QVERIFY(!needlePixels(valid).isEmpty());

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QVERIFY2(std::isnan(gauge.value()), qPrintable(QStringLiteral("value() = %1").arg(gauge.value())));

    const QImage invalid = renderGauge(gauge);
    QCOMPARE(needlePixels(invalid).size(), 0);
    // "---" (ReadoutTest::dashesError()), where the held "50.0" gives four ink groups
    const QVector<InkGroup> held = readoutInkGroups(valid, box);
    QVERIFY2(held.size() == 4, qPrintable(QStringLiteral("%1 ink groups for \"50.0\"").arg(held.size())));
    const QString dashes = dashesError(readoutInkGroups(invalid, box));
    QVERIFY2(dashes.isEmpty(), qPrintable(dashes));

    // Without the digital display the needle is hidden too
    gauge.setDigitalDisplayVisible(false);
    QCOMPARE(needlePixels(renderGauge(gauge)).size(), 0);
}

// #151: entering the invalid state emits valueChanged(NaN) once; a repeated NaN emits nothing
void tst_RadialGauge::nanEmitsValueChangedOnce()
{
    RadialGauge gauge;
    gauge.setValue(50.0);
    QSignalSpy spy(&gauge, &RadialGauge::valueChanged);

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(spy.count(), 1);
    QVERIFY(std::isnan(spy.at(0).at(0).toDouble()));

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(spy.count(), 1);

    gauge.setValue(50.0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toDouble(), 50.0);
}

// #151: a finite value above the range keeps the needle at the stop, but the readout is not the
// one of the maximum itself: it shows the true value
void tst_RadialGauge::outOfRangeReadoutShowsTheTrueValue()
{
    RadialGauge gauge;
    prepareForReadout(gauge);
    gauge.setValue(100.0);
    const QRect box = displayBox(gauge);
    const QImage atMaximum = renderGauge(gauge);

    gauge.setValue(150.0);
    QCOMPARE(gauge.value(), 100.0);
    const QImage above = renderGauge(gauge);
    QCOMPARE(TextInk::sorted(needlePixelsOutside(above, box)), TextInk::sorted(needlePixelsOutside(atMaximum, box)));
    QVERIFY2(above.copy(box) != atMaximum.copy(box), "the readout of 150 equals the readout of the maximum");

    gauge.setValue(0.0);
    const QImage atMinimum = renderGauge(gauge);
    gauge.setValue(-20.0);
    QCOMPARE(gauge.value(), 0.0);
    const QImage below = renderGauge(gauge);
    QCOMPARE(TextInk::sorted(needlePixelsOutside(below, box)), TextInk::sorted(needlePixelsOutside(atMinimum, box)));
    QVERIFY2(below.copy(box) != atMinimum.copy(box), "the readout of -20 equals the readout of the minimum");
}

// #151: +Inf / -Inf keep the needle at the stop, and the readout is not the one of the bound
void tst_RadialGauge::infiniteReadoutDiffersFromTheBound()
{
    RadialGauge gauge;
    prepareForReadout(gauge);
    gauge.setValue(100.0);
    const QRect box = displayBox(gauge);
    const QImage atMaximum = renderGauge(gauge);

    gauge.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 100.0);
    const QImage plusInf = renderGauge(gauge);
    QCOMPARE(TextInk::sorted(needlePixelsOutside(plusInf, box)), TextInk::sorted(needlePixelsOutside(atMaximum, box)));
    QVERIFY2(plusInf.copy(box) != atMaximum.copy(box), "the readout of +Inf equals the readout of the maximum");

    gauge.setValue(0.0);
    const QImage atMinimum = renderGauge(gauge);
    gauge.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 0.0);
    const QImage minusInf = renderGauge(gauge);
    QCOMPARE(TextInk::sorted(needlePixelsOutside(minusInf, box)), TextInk::sorted(needlePixelsOutside(atMinimum, box)));
    QVERIFY2(minusInf.copy(box) != atMinimum.copy(box), "the readout of -Inf equals the readout of the minimum");
}

// #151: a range change classifies the last input again instead of keeping the clamped value
void tst_RadialGauge::rangeChangeReclassifiesTheLastInput()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(150.0);
    QCOMPARE(gauge.value(), 100.0);

    gauge.setRange(0.0, 200.0);
    QCOMPARE(gauge.value(), 150.0);

    gauge.setRange(0.0, 120.0);
    QCOMPARE(gauge.value(), 120.0);

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    gauge.setRange(0.0, 300.0);
    QVERIFY(std::isnan(gauge.value()));
}

void tst_RadialGauge::valueStateAndRawValue()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    QCOMPARE(gauge.valueState(), ValueState::Valid);

    struct Step {
        double input;
        ValueState state;
        double value;
    };
    const Step steps[] = {
        {50.0, ValueState::Valid, 50.0},        {150.0, ValueState::Overrange, 100.0},
        {inf, ValueState::Overrange, 100.0},    {-20.0, ValueState::Underrange, 0.0},
        {-inf, ValueState::Underrange, 0.0},    {100.0, ValueState::Valid, 100.0},
        {0.0, ValueState::Valid, 0.0},          {nan, ValueState::Invalid, nan},
        {75.0, ValueState::Valid, 75.0},
    };
    for (const Step &step : steps) {
        gauge.setValue(step.input);
        const QString where = QStringLiteral("input %1").arg(step.input);
        QVERIFY2(gauge.valueState() == step.state, qPrintable(where));
        if (std::isnan(step.value)) {
            QVERIFY2(std::isnan(gauge.value()) && std::isnan(gauge.rawValue()), qPrintable(where));
        } else {
            QVERIFY2(gauge.value() == step.value, qPrintable(where));
            QVERIFY2(gauge.rawValue() == step.input, qPrintable(where));
        }
    }
    QCOMPARE(gauge.property("valueState").value<ValueState>(), ValueState::Valid);
    QCOMPARE(gauge.property("rawValue").toDouble(), 75.0);
}

void tst_RadialGauge::valueStateSignalOrder()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(50.0);

    QStringList events;
    connect(&gauge, &RadialGauge::valueChanged, &gauge,
            [&events](double v) { events << QStringLiteral("value %1").arg(v); });
    connect(&gauge, &RadialGauge::valueStateChanged, &gauge,
            [&events](ValueState st) { events << QStringLiteral("state %1").arg(static_cast<int>(st)); });

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(events, QStringList({QStringLiteral("value nan"), QStringLiteral("state 3")}));
    events.clear();
    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(events.isEmpty());

    gauge.setValue(150.0);
    QCOMPARE(events, QStringList({QStringLiteral("value 100"), QStringLiteral("state 2")}));
    events.clear();
    // Same value() and state: the readout changes, no signal
    gauge.setValue(160.0);
    QVERIFY(events.isEmpty());

    // The range change emits valueChanged as before #151, then the new state
    gauge.setRange(0.0, 200.0);
    QCOMPARE(events, QStringList({QStringLiteral("value 160"), QStringLiteral("state 0")}));
}

// #151: the zones of an invalid input are unknown, so the zone signals keep their last state
void tst_RadialGauge::invalidInputKeepsZoneSignals()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setWarningThreshold(70.0);
    gauge.setErrorThreshold(85.0);
    QSignalSpy warning(&gauge, &RadialGauge::warningExceeded);
    QSignalSpy error(&gauge, &RadialGauge::errorExceeded);

    gauge.setValue(90.0);
    QCOMPARE(error.count(), 1);
    QCOMPARE(error.at(0).at(0).toBool(), true);

    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(error.count(), 1);
    QCOMPARE(warning.count(), 0);

    gauge.setValue(50.0);
    QCOMPARE(error.count(), 2);
    QCOMPARE(error.at(1).at(0).toBool(), false);
}

void tst_RadialGauge::readoutTextOfEveryState()
{
    using IS = internal::InputState;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QCOMPARE(internal::gaugeReadoutText(IS::Valid, 42.31, 0.0, 100.0, 1), QStringLiteral("42.3"));
    QCOMPARE(internal::gaugeReadoutText(IS::Overrange, 8350.0, 0.0, 8000.0, 0), QStringLiteral("8350"));
    QCOMPARE(internal::gaugeReadoutText(IS::Underrange, -12.5, 0.0, 100.0, 1), QStringLiteral("-12.5"));
    QCOMPARE(internal::gaugeReadoutText(IS::Overrange, inf, 0.0, 8000.0, 0), QStringLiteral(">8000"));
    QCOMPARE(internal::gaugeReadoutText(IS::Underrange, -inf, -20.0, 80.0, 2), QStringLiteral("<-20.00"));
    QCOMPARE(internal::gaugeReadoutText(IS::Invalid, nan, 0.0, 100.0, 1), QStringLiteral("---"));
    QCOMPARE(internal::gaugeReadoutText(IS::Valid, -0.04, -1.0, 1.0, 1), QStringLiteral("0.0"));
}

// #151: the out-of-range readout carries a triangle, so the state is not told by colour alone: one more ink
// group than the same text read as a valid value, pointing up above the range and down below it
void tst_RadialGauge::outOfRangeMarkIsAShape()
{
    checkOutOfRangeMark(nullptr);
}

// #151: the mark is a painted shape, so a widget font without the triangle characters still shows it
void tst_RadialGauge::outOfRangeMarkWithoutTheGlyphInTheFont()
{
    QString error;
    const QFont font = readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QFontMetricsF fm(font);
    QVERIFY2(!fm.inFontUcs4(0x25B2) && !fm.inFontUcs4(0x25BC), "the test font has a triangle glyph");
    for (const QChar c : QStringLiteral("0123456789.-<>")) {
        QVERIFY2(fm.inFontUcs4(c.unicode()), qPrintable(QStringLiteral("the test font has no glyph for '%1'").arg(c)));
    }
    qInfo("test font: %s", qPrintable(QFontInfo(font).family()));
    checkOutOfRangeMark(&font);
}

// #151: an out-of-range value wider than the pod's valid values is drawn smaller, inside the pod
void tst_RadialGauge::longOutOfRangeReadoutStaysInThePod()
{
    RadialGauge gauge;
    prepareForReadout(gauge);
    gauge.setValue(50.0);
    const QRect box = displayBox(gauge);

    gauge.setValue(123456789.0);
    QCOMPARE(gauge.valueState(), ValueState::Overrange);
    gauge.setDigitalDisplayVisible(false);
    const QImage without = renderGauge(gauge);
    gauge.setDigitalDisplayVisible(true);
    const QImage with = renderGauge(gauge);
    QVector<QPoint> display = differingPixels(without, with);
    // The needle at the stop crosses the pod; its blended edge pixels are not the readout
    display.erase(std::remove_if(display.begin(), display.end(),
                                 [&](const QPoint &p) { return isNeedlePixel(with.pixel(p)); }),
                  display.end());
    const QRect drawn = boundingBox(display);
    QVERIFY2(box.contains(drawn),
             qPrintable(QStringLiteral("readout box %1,%2 %3x%4 outside the pod %5,%6 %7x%8")
                            .arg(drawn.x()).arg(drawn.y()).arg(drawn.width()).arg(drawn.height())
                            .arg(box.x()).arg(box.y()).arg(box.width()).arg(box.height())));
}

// #151: the unit label stays on the dial for an invalid input, drawn as for a valid value
void tst_RadialGauge::unitShownForAnInvalidInput()
{
    QString error;
    const QFont font = readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const auto make = [&font](RadialGauge &g, const QString &unit, double value) {
        prepareForReadout(g);
        g.setFont(font);
        g.setUnit(unit);
        g.setValue(value);
    };
    // The unit label: the pixels that the unit changes on a valid gauge (value 0, needle away from the label)
    RadialGauge validUnit;
    make(validUnit, QStringLiteral("88"), 0.0);
    RadialGauge validPlain;
    make(validPlain, QString(), 0.0);
    const QImage withUnit = renderGauge(validUnit);
    // The readout box of the gauge without a unit (with one, displayBox() also holds the unit label, which is
    // drawn only with the display)
    const QRect box = displayBox(validPlain);
    const QVector<QPoint> label = differingPixels(withUnit, renderGauge(validPlain));

    RadialGauge invalidUnit;
    make(invalidUnit, QStringLiteral("88"), std::numeric_limits<double>::quiet_NaN());
    const QImage invalid = renderGauge(invalidUnit);
    int outside = 0;
    int different = 0;
    for (const QPoint &p : label) {
        if (box.contains(p) || isNeedlePixel(withUnit.pixel(p))) {
            continue;
        }
        ++outside;
        different += invalid.pixel(p) != withUnit.pixel(p) ? 1 : 0;
    }
    QVERIFY2(outside > 0, "no unit label outside the display");
    QVERIFY2(different == 0,
             qPrintable(QStringLiteral("%1 of the %2 unit label pixels differ for an invalid input").arg(different).arg(outside)));
}

// #151: before the first setValue() there is no input to classify: the initial value follows the range, as
// before #151, so that configuring a range that does not hold it shows the minimum, not an out-of-range readout
void tst_RadialGauge::initialValueFollowsTheRange()
{
    RadialGauge fresh;
    fresh.resize(QSize(240, 240));
    QSignalSpy states(&fresh, &RadialGauge::valueStateChanged);
    fresh.setRange(10.0, 100.0);
    QCOMPARE(fresh.valueState(), ValueState::Valid);
    QCOMPARE(fresh.value(), 10.0);
    QCOMPARE(fresh.rawValue(), 10.0);
    QCOMPARE(states.count(), 0);

    // Drawn as the minimum given as an input
    RadialGauge given;
    given.resize(QSize(240, 240));
    given.setRange(10.0, 100.0);
    given.setValue(10.0);
    QVERIFY(renderGauge(fresh) == renderGauge(given));

    fresh.setRange(-50.0, -10.0);
    QCOMPARE(fresh.valueState(), ValueState::Valid);
    QCOMPARE(fresh.value(), -10.0);
    QCOMPARE(states.count(), 0);

    // An input given with setValue() is classified, even if it is the initial value
    RadialGauge explicitZero;
    explicitZero.setValue(0.0);
    explicitZero.setRange(10.0, 100.0);
    QCOMPARE(explicitZero.valueState(), ValueState::Underrange);
    QCOMPARE(explicitZero.rawValue(), 0.0);
    QCOMPARE(explicitZero.value(), 10.0);
}

QTEST_MAIN(tst_RadialGauge)
#include "tst_RadialGauge.moc"


