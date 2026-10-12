// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtWidgets/QApplication>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include "internal/LinearGaugeScale.h"

#include <algorithm>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_LinearGauge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void defaultValues();
    void rangeAndClamping();
    void orientationSwitching();
    void thermometerMode();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void scaleLabelLayout_data();
    void scaleLabelLayout();
    void scaleLabelLayoutNonFinite();
    void scaleLabelMinGap();
    void scaleLabelsReadable_data();
    void scaleLabelsReadable();
};

namespace {

constexpr QRgb kLabelColor = 0xffff00ff; // magenta: used by nothing else in the gauge
constexpr QRgb kTickColor = 0xff0000ff;  // pure blue major ticks

bool isLabelPixel(QRgb p)
{
    // Anti-aliased magenta on the dark casing: red and blue well above green
    return qRed(p) >= 120 && qBlue(p) >= 120 && qGreen(p) * 2 <= qRed(p) && qGreen(p) * 2 <= qBlue(p);
}

bool isTickPixel(QRgb p)
{
    return qBlue(p) >= 200 && qRed(p) <= 60 && qGreen(p) <= 60;
}

struct Run {
    double start; // logical px along the scan axis
    double end;
};

// Runs of image columns (horizontal) or rows (vertical) that contain a pixel matching \a pred,
// merging runs closer than \a mergeGap logical px (a negative value merges nothing)
template <typename Pred>
QVector<Run> runsAlong(const QImage &img, Qt::Orientation axis, double mergeGap, Pred pred)
{
    const double dpr = img.devicePixelRatio();
    const int len = axis == Qt::Horizontal ? img.width() : img.height();
    const int across = axis == Qt::Horizontal ? img.height() : img.width();
    QVector<Run> runs;
    int runStart = -1;
    for (int a = 0; a <= len; ++a) {
        bool hit = false;
        for (int b = 0; a < len && b < across && !hit; ++b) {
            hit = pred(axis == Qt::Horizontal ? img.pixel(a, b) : img.pixel(b, a));
        }
        if (hit && runStart < 0) {
            runStart = a;
        } else if (!hit && runStart >= 0) {
            const Run run{runStart / dpr, a / dpr};
            if (!runs.isEmpty() && run.start - runs.last().end <= mergeGap) {
                runs.last().end = run.end;
            } else {
                runs.append(run);
            }
            runStart = -1;
        }
    }
    return runs;
}

// Most frequent non-label colour around the label pixels: the background the labels are drawn on
QRgb labelBackground(const QImage &img)
{
    QRect box;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (isLabelPixel(img.pixel(x, y))) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    QHash<QRgb, int> count;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            const QRgb p = img.pixel(x, y);
            if (!isLabelPixel(p)) {
                ++count[p];
            }
        }
    }
    QRgb best = qRgb(0, 0, 0);
    int bestCount = -1;
    for (auto it = count.cbegin(); it != count.cend(); ++it) {
        if (it.value() > bestCount) {
            bestCount = it.value();
            best = it.key();
        }
    }
    return best;
}

// One label rendered alone, with the gauge's label font, at the same DPR and on the same background
struct LabelInk {
    double minLength{1e9};   // length of its pixels along the scale axis (smallest over the phases)
    double maxLength{0.0};   // largest over the phases
    double maxInnerGap{0.0}; // largest space between glyphs of the label along the axis
};

LabelInk referenceInk(const QString &text, const QFont &font, Qt::Orientation axis, qreal dpr, QRgb background)
{
    // Glyph edges fall differently with the sub-pixel position (e.g. CoreText on macOS), so the
    // label is rendered at four sub-pixel phases along the axis
    LabelInk ink;
    const QFontMetricsF fm(font);
    const QSize logical = (fm.boundingRect(text).size() + QSizeF(8, 8)).toSize();
    for (int phase = 0; phase < 4; ++phase) {
        QImage img(logical * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(QColor::fromRgb(background));
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            p.setFont(font);
            p.setPen(QColor::fromRgb(kLabelColor));
            const double shift = phase * 0.25 / dpr; // a quarter device pixel per phase
            const QPointF offset = axis == Qt::Horizontal ? QPointF(shift, 0.0) : QPointF(0.0, shift);
            p.drawText(QRectF(offset, QSizeF(logical) - QSizeF(1.0, 1.0)), Qt::AlignCenter, text);
        }
        const QVector<Run> glyphs = runsAlong(img, axis, -1.0, isLabelPixel);
        if (glyphs.isEmpty()) {
            continue;
        }
        const double length = glyphs.last().end - glyphs.first().start;
        ink.minLength = std::min(ink.minLength, length);
        ink.maxLength = std::max(ink.maxLength, length);
        for (int i = 1; i < glyphs.size(); ++i) {
            ink.maxInnerGap = std::max(ink.maxInnerGap, glyphs[i].start - glyphs[i - 1].end);
        }
    }
    return ink;
}

// Groups pixel runs into labels: runs separated by at most \a maxInnerGap belong to one label
QVector<Run> groupIntoLabels(const QVector<Run> &runs, double maxInnerGap)
{
    QVector<Run> labels;
    for (const Run &r : runs) {
        if (!labels.isEmpty() && r.start - labels.last().end <= maxInnerGap) {
            labels.last().end = r.end;
        } else {
            labels.append(r);
        }
    }
    return labels;
}

QString scaleLabelText(double value, int precision)
{
    // Same formatting as LinearGauge::paintEvent()
    return precision == 0 ? QString::number(static_cast<qint64>(std::round(value)))
                          : QString::number(value, 'f', (value == std::floor(value)) ? 0 : 1);
}

} // namespace

void tst_LinearGauge::initTestCase()
{
    // QIW_TEST_FONT=<family> runs the suite with another application font, to check the
    // pixel tests against font metrics other than the platform default
    const QString family = qEnvironmentVariable("QIW_TEST_FONT");
    if (!family.isEmpty()) {
        QApplication::setFont(QFont(family));
        qInfo("Application font: %s", qPrintable(QFontInfo(QApplication::font()).family()));
    }
}

void tst_LinearGauge::defaultValues()
{
    LinearGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_LinearGauge::rangeAndClamping()
{
    LinearGauge gauge;
    gauge.setRange(-20.0, 80.0);
    QCOMPARE(gauge.minimum(), -20.0);
    QCOMPARE(gauge.maximum(), 80.0);

    // Below min
    gauge.setValue(-50.0);
    QCOMPARE(gauge.value(), -20.0);

    // Above max
    gauge.setValue(120.0);
    QCOMPARE(gauge.value(), 80.0);

    // Inside range
    gauge.setValue(25.0);
    QCOMPARE(gauge.value(), 25.0);
}

void tst_LinearGauge::orientationSwitching()
{
    LinearGauge gauge;
    gauge.setOrientation(Qt::Horizontal);
    QCOMPARE(gauge.orientation(), Qt::Horizontal);

    gauge.setOrientation(Qt::Vertical);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_LinearGauge::thermometerMode()
{
    LinearGauge gauge;
    gauge.setThermometerMode(true);
    QVERIFY(gauge.isThermometerMode());

    gauge.setThermometerMode(false);
    QVERIFY(!gauge.isThermometerMode());
}

void tst_LinearGauge::signalEmission()
{
    LinearGauge gauge;
    QSignalSpy spy(&gauge, &LinearGauge::valueChanged);

    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);
}

void tst_LinearGauge::renderOffscreen()
{
    LinearGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(72.0);
    gauge.setOrientation(Qt::Vertical);
    gauge.setThermometerMode(true);
    gauge.resize(80, 220);

    QPixmap pix(gauge.size());
    gauge.render(&pix);
    QVERIFY(!pix.isNull());

    // Horizontal test
    gauge.setOrientation(Qt::Horizontal);
    gauge.resize(220, 80);
    QPixmap pixH(gauge.size());
    gauge.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_LinearGauge::extremeResizeNoCrash()
{
    LinearGauge gauge;
    gauge.resize(1, 1);
    QPixmap pixSmall(gauge.size());
    gauge.render(&pixSmall);

    gauge.resize(2560, 1440);
    QPixmap pixLarge(gauge.size());
    gauge.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_LinearGauge::nanAndInfinityResilience()
{
    LinearGauge gauge;
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

    // Render offscreen in both orientations
    gauge.resize(80, 250);
    QPixmap pixV(gauge.size());
    gauge.render(&pixV);
    QVERIFY(!pixV.isNull());

    gauge.setOrientation(Qt::Horizontal);
    gauge.resize(250, 80);
    QPixmap pixH(gauge.size());
    gauge.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_LinearGauge::scaleLabelLayout_data()
{
    QTest::addColumn<QVector<double>>("extents"); // size of each major label along the scale
    QTest::addColumn<double>("length");
    QTest::addColumn<double>("minGap");
    QTest::addColumn<int>("step");
    QTest::addColumn<QList<int>>("indices");      // drawn labels

    QTest::newRow("ample space: every label") << QVector<double>(11, 10.0) << 500.0 << 5.0 << 1
                                              << QList<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    // Horizontal 0-250 at its 280x100 size hint, extents and scale length measured from a render:
    // pushing "250" inside the scale leaves 4.3 px after "200" with k = 2, so k = 5
    QTest::newRow("280x100: end push rejects k = 2")
        << QVector<double>{8, 17, 17, 17, 24, 24, 24, 24, 25, 25, 25} << 209.0 << 7.0 << 5 << QList<int>{0, 5, 10};
    // k = 3 or 4 would keep the gap here, but only divisors of 10 are allowed
    QTest::newRow("divisors only: 2 too tight, 5 chosen") << QVector<double>(11, 10.0) << 100.0 << 12.0 << 5
                                                          << QList<int>{0, 5, 10};
    QTest::newRow("prime majorTicks: first and last") << QVector<double>(8, 10.0) << 70.0 << 12.0 << 7
                                                      << QList<int>{0, 7};
    QTest::newRow("prime majorTicks: every label") << QVector<double>(8, 10.0) << 700.0 << 12.0 << 1
                                                   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7};
    QTest::newRow("majorTicks 0: single label") << QVector<double>{10.0} << 100.0 << 5.0 << 1 << QList<int>{0};
    QTest::newRow("majorTicks 1: both labels") << QVector<double>{10.0, 10.0} << 100.0 << 5.0 << 1 << QList<int>{0, 1};
    // Even first and last do not keep the gap: they are drawn anyway (decided in #31)
    QTest::newRow("degenerate: first and last anyway") << QVector<double>(11, 30.0) << 40.0 << 5.0 << 10
                                                       << QList<int>{0, 10};
    QTest::newRow("no labels") << QVector<double>() << 100.0 << 5.0 << 0 << QList<int>();
}

void tst_LinearGauge::scaleLabelLayout()
{
    QFETCH(QVector<double>, extents);
    QFETCH(double, length);
    QFETCH(double, minGap);
    QFETCH(int, step);
    QFETCH(QList<int>, indices);

    const std::vector<double> ext(extents.cbegin(), extents.cend());
    const internal::ScaleLabelLayout layout = internal::layoutScaleLabels(ext, length, minGap);

    QCOMPARE(layout.step, step);
    QList<int> drawn;
    for (const internal::ScaleLabelSlot &slot : layout.labels) {
        drawn.append(slot.index);
        QCOMPARE(slot.end - slot.start, ext[static_cast<size_t>(slot.index)]); // a label keeps its size
    }
    QCOMPARE(drawn, indices);
    if (layout.labels.empty()) {
        return;
    }

    const int n = static_cast<int>(ext.size()) - 1;
    if (n > 0) {
        QCOMPARE(n % layout.step, 0); // the step is a divisor of majorTicks
    }
    const internal::ScaleLabelSlot &first = layout.labels.front();
    const internal::ScaleLabelSlot &last = layout.labels.back();
    QCOMPARE(first.index, 0);
    QCOMPARE(last.index, n);
    if (ext.front() <= length && ext.back() <= length) { // the end labels stay inside the scale
        QVERIFY(first.start >= 0.0);
        QVERIFY(last.end <= length);
    }
    // Interior labels stay centred on their tick
    for (const internal::ScaleLabelSlot &slot : layout.labels) {
        if (slot.index > 0 && slot.index < n) {
            QCOMPARE((slot.start + slot.end) * 0.5, length * slot.index / n);
        }
    }
    // The gap holds between drawn neighbours, except in the degenerate first/last case
    if (layout.step < n || n <= 1) {
        for (size_t i = 1; i < layout.labels.size(); ++i) {
            QVERIFY2(layout.labels[i].start - layout.labels[i - 1].end >= minGap,
                     qPrintable(QStringLiteral("labels %1 and %2").arg(layout.labels[i - 1].index).arg(layout.labels[i].index)));
        }
    }
}

void tst_LinearGauge::scaleLabelLayoutNonFinite()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    // Non-finite extents count as 0: on a finite scale every label fits
    const internal::ScaleLabelLayout finiteScale = internal::layoutScaleLabels({10.0, nan, inf, 10.0}, 300.0, nan);
    QCOMPARE(finiteScale.step, 1);
    QCOMPARE(static_cast<int>(finiteScale.labels.size()), 4);

    // A non-finite scale length counts as 0: all ticks coincide, so only first and last are drawn
    const internal::ScaleLabelLayout zeroScale = internal::layoutScaleLabels({10.0, nan, inf, 10.0}, inf, 5.0);
    QCOMPARE(zeroScale.step, 3);
    QCOMPARE(static_cast<int>(zeroScale.labels.size()), 2);

    for (const internal::ScaleLabelLayout *layout : {&finiteScale, &zeroScale}) {
        for (const internal::ScaleLabelSlot &slot : layout->labels) {
            QVERIFY(std::isfinite(slot.start));
            QVERIFY(std::isfinite(slot.end));
        }
    }
}

void tst_LinearGauge::scaleLabelMinGap()
{
    QCOMPARE(internal::scaleLabelMinGap(13.5), 6.75);
    QCOMPARE(internal::scaleLabelMinGap(8.0), 4.0);
    QCOMPARE(internal::scaleLabelMinGap(6.0), 4.0);
    QCOMPARE(internal::scaleLabelMinGap(std::numeric_limits<double>::quiet_NaN()), 4.0);
}

void tst_LinearGauge::scaleLabelsReadable_data()
{
    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::addColumn<bool>("thermometer");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("precision");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("expectedLabels"); // -1: not checked

    // Configurations and sizes from the gallery Dashboard (#31)
    QTest::newRow("horizontal 0-250 at size hint") << Qt::Horizontal << false << 250.0 << 0 << QSize(280, 100) << -1;
    QTest::newRow("horizontal 0-250 at 219x55") << Qt::Horizontal << false << 250.0 << 0 << QSize(219, 55) << -1;
    QTest::newRow("vertical thermometer 0-120 at 103x166") << Qt::Vertical << true << 120.0 << 1 << QSize(103, 166) << -1;
    QTest::newRow("vertical 0-100 at 102x166") << Qt::Vertical << false << 100.0 << 0 << QSize(102, 166) << -1;
    // Ample space: nothing to thin out
    QTest::newRow("vertical 0-100 at 100x400: all labels") << Qt::Vertical << false << 100.0 << 0 << QSize(100, 400) << 11;
}

void tst_LinearGauge::scaleLabelsReadable()
{
    QFETCH(Qt::Orientation, orientation);
    QFETCH(bool, thermometer);
    QFETCH(double, maximum);
    QFETCH(int, precision);
    QFETCH(QSize, size);
    QFETCH(int, expectedLabels);

    LinearGauge gauge;
    gauge.setOrientation(orientation);
    gauge.setThermometerMode(thermometer);
    gauge.setRange(0.0, maximum);
    gauge.setValue(maximum * 0.5);
    gauge.setPrecision(precision);
    gauge.setMinorTicks(0);
    gauge.setTextColor(QColor::fromRgb(kLabelColor));
    gauge.setScaleColor(QColor::fromRgb(kTickColor));
    gauge.resize(size);
    const QImage img = gauge.grab().toImage();
    const qreal dpr = img.devicePixelRatio();
    const bool horizontal = orientation == Qt::Horizontal;

    // Label font and minimum gap as decided in #31 (font: LinearGauge::paintEvent())
    QFont font = gauge.font();
    font.setPixelSize(std::clamp(static_cast<int>(size.height() * (horizontal ? 0.12 : 0.04)), 8, 12));
    font.setBold(true);
    const double minGap = internal::scaleLabelMinGap(QFontMetricsF(font).height());
    const int majorTicks = gauge.majorTicks();

    // Reference: every label text rendered alone (same font, DPR and background)
    const QRgb background = labelBackground(img);
    QVector<LabelInk> ink;
    double widest = 0.0;
    double innerGap = 0.0;
    for (int i = 0; i <= majorTicks; ++i) {
        ink.append(referenceInk(scaleLabelText(maximum * i / majorTicks, precision), font, orientation, dpr, background));
        widest = std::max(widest, ink.last().maxLength);
        innerGap = std::max(innerGap, ink.last().maxInnerGap);
    }

    // Along the scale axis: labels (pixel runs grouped with the reference inner gap + 0.5 px) and
    // major ticks, ordered from the minimum end
    QVector<Run> labels = groupIntoLabels(runsAlong(img, orientation, -1.0, isLabelPixel), innerGap + 0.5);
    QVector<Run> ticks = runsAlong(img, orientation, 0.0, isTickPixel);
    if (!horizontal) { // rows grow downwards, the scale grows upwards
        std::reverse(labels.begin(), labels.end());
        std::reverse(ticks.begin(), ticks.end());
    }
    QCOMPARE(ticks.size(), majorTicks + 1);
    QVERIFY(labels.size() >= 2);
    const auto describe = [&labels]() {
        QStringList parts;
        for (const Run &r : labels) parts << QStringLiteral("%1-%2").arg(r.start, 0, 'f', 1).arg(r.end, 0, 'f', 1);
        return parts.join(QLatin1Char(' '));
    };

    // (a) no label run longer than the largest label: longer runs are merged labels
    for (const Run &r : labels) {
        QVERIFY2(r.end - r.start <= widest + 1.0,
                 qPrintable(QStringLiteral("run %1-%2 longer than the widest label (%3 px): labels merged [%4]")
                                .arg(r.start, 0, 'f', 1).arg(r.end, 0, 'f', 1).arg(widest, 0, 'f', 1).arg(describe())));
    }
    // (b) the decided minimum gap between drawn labels (1 px for anti-aliasing)
    for (int i = 1; i < labels.size(); ++i) {
        const double gap = horizontal ? labels[i].start - labels[i - 1].end : labels[i - 1].start - labels[i].end;
        QVERIFY2(gap >= minGap - 1.0, qPrintable(QStringLiteral("gap %1 px < minimum %2 px [%3]")
                                                     .arg(gap, 0, 'f', 1).arg(minGap, 0, 'f', 1).arg(describe())));
    }
    // (c) first and last labels drawn, inside the scale (between the end ticks), the last one whole
    const double scaleMin = (ticks.first().start + ticks.first().end) * 0.5;
    const double scaleMax = (ticks.last().start + ticks.last().end) * 0.5;
    const Run &first = labels.first();
    const Run &last = labels.last();
    if (horizontal) {
        QVERIFY2(first.start >= scaleMin - 1.0 && first.start <= scaleMin + ink.first().maxLength + 1.0,
                 qPrintable(QStringLiteral("first label %1-%2, scale starts at %3").arg(first.start).arg(first.end).arg(scaleMin)));
        QVERIFY2(last.end <= scaleMax + 1.0 && last.end >= scaleMax - ink.last().maxLength - 1.0,
                 qPrintable(QStringLiteral("last label %1-%2, scale ends at %3").arg(last.start).arg(last.end).arg(scaleMax)));
    } else {
        QVERIFY2(first.end <= scaleMin + 1.0 && first.end >= scaleMin - ink.first().maxLength - 1.0,
                 qPrintable(QStringLiteral("bottom label %1-%2, scale bottom at %3").arg(first.start).arg(first.end).arg(scaleMin)));
        QVERIFY2(last.start >= scaleMax - 1.0 && last.start <= scaleMax + ink.last().maxLength + 1.0,
                 qPrintable(QStringLiteral("top label %1-%2, scale top at %3").arg(last.start).arg(last.end).arg(scaleMax)));
    }
    // whole: at least the reference pixel length of the label, 0.5 px for anti-aliasing
    QVERIFY2(last.end - last.start >= ink.last().minLength - 0.5,
             qPrintable(QStringLiteral("last label %1 px long, its reference is %2 px: cut")
                            .arg(last.end - last.start, 0, 'f', 1).arg(ink.last().minLength, 0, 'f', 1)));
    qInfo("%d labels; reference inner gap %.1f px; last label %.1f px, reference %.1f px", int(labels.size()), innerGap,
          last.end - last.start, ink.last().minLength);
    // (d) no label pixel inside the digital display pod (LinearGauge::paintEvent() geometry)
    const QRectF pod = horizontal ? QRectF(size.width() - 55.0 + 4.0, 6.0, 55.0 - 10.0, size.height() - 12.0)
                                  : QRectF(6.0, 6.0, size.width() - 12.0, 24.0);
    for (int y = qFloor(pod.top() * dpr); y < qCeil(pod.bottom() * dpr); ++y) {
        for (int x = qFloor(pod.left() * dpr); x < qCeil(pod.right() * dpr); ++x) {
            if (isLabelPixel(img.pixel(x, y))) {
                QFAIL(qPrintable(QStringLiteral("label pixel inside the digital display at %1,%2").arg(x / dpr).arg(y / dpr)));
            }
        }
    }
    // (e) with ample space every label is drawn
    if (expectedLabels >= 0) {
        QCOMPARE(labels.size(), expectedLabels);
    }
}

QTEST_MAIN(tst_LinearGauge)
#include "tst_LinearGauge.moc"
