// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/RadialGauge.h>

#include "FontTestUtils.h"

#include <memory>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <limits>

#include "TextInkTestUtils.h"
#include "internal/LinearGaugeScale.h"
#include "internal/RadialGaugeLayout.h"

using namespace QtIndustrialWidgets;

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

    // NaN input must be ignored (fail-safe hold)
    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(gauge.value(), 50.0);

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

// #43 (review of PR #48): between the preferred pod place and the place below the labels, the
// one giving the larger value font is used. At 200 px with 0-100000 the value "100000" fits at the
// full font only below the end labels; the reference is the same gauge with a 180-degree arc, whose
// end labels are level with the centre and leave the preferred place free. The two ink heights
// may differ by 1 device px with the vertical sub-pixel position of the text.
void tst_RadialGauge::valueFontNotReducedWhenLoweringAvoidsIt()
{
    RadialGauge gauge;
    configure(gauge, GaugeConfig{200, 0.0, 100000.0, 0, -135.0, 270.0});
    gauge.setUnit(QString());
    const QRect ink = valueInkBox(gauge);

    RadialGauge reference;
    configure(reference, GaugeConfig{200, 0.0, 100000.0, 0, -90.0, 180.0});
    reference.setUnit(QString());
    const QRect referenceInk = valueInkBox(reference);

    QVERIFY(!ink.isEmpty());
    QVERIFY(!referenceInk.isEmpty());
    QVERIFY2(ink.height() >= referenceInk.height() - 1,
             qPrintable(QStringLiteral("value ink %1x%2 device px, reference (full font) %3x%4 (DPR %5)")
                            .arg(ink.width()).arg(ink.height()).arg(referenceInk.width()).arg(referenceInk.height())
                            .arg(gauge.grab().devicePixelRatio())));
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

// Device-pixel ink box of a text drawn alone in the given font at the given device pixel ratio
QRect inkBoxAlone(const QString &text, const QFont &font, qreal dpr)
{
    const QFontMetricsF fm(font);
    const QSizeF logical(fm.horizontalAdvance(text) + 40.0, fm.height() + 40.0);
    QImage img((logical * dpr).toSize(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(QPointF(20.0, 20.0 + fm.ascent()), text);
    }
    QRect box;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixel(x, y) != qRgb(0, 0, 0)) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    return box;
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
        largest = largest.expandedTo(inkBoxAlone(QString::number(static_cast<qint64>(v)), labelFont, dpr).size());
    }
    const int tolerance = static_cast<int>(std::ceil(dpr));

    // Clusters (union-find over the ink pixels, neighbours searched on a grid)
    const int reach = static_cast<int>(std::ceil(threshold));
    QHash<qint64, int> at;
    auto key = [](int x, int y) { return (static_cast<qint64>(y) << 32) | static_cast<quint32>(x); };
    for (int i = 0; i < ink.size(); ++i) {
        at.insert(key(ink[i].x(), ink[i].y()), i);
    }
    std::vector<int> parent(static_cast<size_t>(ink.size()));
    for (int i = 0; i < ink.size(); ++i) {
        parent[static_cast<size_t>(i)] = i;
    }
    std::function<int(int)> root = [&](int i) {
        while (parent[static_cast<size_t>(i)] != i) {
            parent[static_cast<size_t>(i)] = parent[static_cast<size_t>(parent[static_cast<size_t>(i)])];
            i = parent[static_cast<size_t>(i)];
        }
        return i;
    };
    for (int i = 0; i < ink.size(); ++i) {
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                // free pixels between the two along the separating axis
                const int free = std::max(std::abs(dx), std::abs(dy)) - 1;
                if (free >= threshold) {
                    continue;
                }
                const auto it = at.constFind(key(ink[i].x() + dx, ink[i].y() + dy));
                if (it != at.constEnd()) {
                    parent[static_cast<size_t>(root(i))] = root(it.value());
                }
            }
        }
    }
    QHash<int, QRect> clusters;
    for (int i = 0; i < ink.size(); ++i) {
        clusters[root(i)] |= QRect(ink[i], QSize(1, 1));
    }

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

QTEST_MAIN(tst_RadialGauge)
#include "tst_RadialGauge.moc"


