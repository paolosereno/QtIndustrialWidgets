// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

using namespace QtIndustrialWidgets;

namespace {

// #40: a meter whose zone colour paints only the scale ticks and labels: continuous mode, value at
// the minimum (no fill), no peak marker, thresholds above the range, no title, unit or channel labels
std::unique_ptr<LevelMeter> scaleOnlyMeter(Qt::Orientation o, double maximum, double pointSize, const QSize &size)
{
    auto m = std::make_unique<LevelMeter>();
    m->setOrientation(o);
    m->setChannelCount(1);
    m->setRange(0.0, maximum);
    m->setWarningThreshold(maximum * 2.0);
    m->setErrorThreshold(maximum * 3.0);
    m->setDisplayMode(LevelMeter::DisplayMode::Continuous);
    m->setPeakHoldEnabled(false);
    m->setUnit(QString());
    m->setTitle(QString());
    m->setValue(0.0);
    QFont f = m->font();
    f.setPointSizeF(pointSize);
    m->setFont(f);
    m->resize(size.isValid() ? size : m->minimumSizeHint());
    return m;
}

QImage grabMeter(LevelMeter &m)
{
    return m.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// Pixels painted in the zone colour (ticks and labels only, see scaleOnlyMeter())
std::vector<QPoint> scaleInk(LevelMeter &m)
{
    m.setNormalColor(QColor(255, 255, 255));
    const QImage a = grabMeter(m);
    m.setNormalColor(QColor(255, 0, 0));
    const QImage b = grabMeter(m);
    std::vector<QPoint> out;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

// Ink width (device px) of a text drawn alone with the scale font of the meter
// (LevelMeter.cpp: the widget font at 70 %, at least 6 pt)
int referenceInkWidth(const QString &text, const QFont &widgetFont, qreal dpr)
{
    QFont f = widgetFont;
    f.setPointSizeF(std::max(6.0, widgetFont.pointSizeF() * 0.7));
    const QFontMetricsF fm(f);
    const QSize logical = (QSizeF(fm.horizontalAdvance(text) + 40, fm.height() + 20)).toSize();
    QImage img(logical * dpr, QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QPointF(20, 10 + fm.ascent()), text);
    }
    int left = img.width(), right = -1;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixel(x, y) != qRgb(0, 0, 0)) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    }
    return right >= left ? right - left + 1 : 0;
}

// Runs of consecutive set indices: [first, last] pairs
std::vector<std::pair<int, int>> runs(const std::vector<bool> &set)
{
    std::vector<std::pair<int, int>> out;
    for (int i = 0; i < static_cast<int>(set.size()); ++i) {
        if (!set[static_cast<size_t>(i)]) continue;
        if (out.empty() || out.back().second != i - 1) out.push_back({i, i});
        else out.back().second = i;
    }
    return out;
}

struct TickLabel {
    double position;  // device px along the scale axis (tick centre)
    QString text;
    int inkFirst;     // device px along the label's long axis, -1 if no ink
    int inkLast;
};

// Splits the scale ink into ticks and labels. The labels lie beyond a gap after the ticks: right of
// them for a vertical meter, below them for a horizontal one. Linear range 0..maximum, 6 divisions.
std::vector<TickLabel> measureLabels(LevelMeter &m, double maximum)
{
    const std::vector<QPoint> ink = scaleInk(m);
    const QImage img = grabMeter(m);
    const bool vertical = m.orientation() == Qt::Vertical;
    // Across-axis projection: x for a vertical meter, y for a horizontal one
    std::vector<bool> across(static_cast<size_t>(vertical ? img.width() : img.height()), false);
    for (const QPoint &p : ink) across[static_cast<size_t>(vertical ? p.x() : p.y())] = true;
    const auto acrossRuns = runs(across);
    std::vector<TickLabel> out;
    if (acrossRuns.size() < 2) return out;
    const auto tickBand = acrossRuns.front();
    const int labelStart = acrossRuns[1].first;
    // Tick positions along the scale axis
    std::vector<bool> along(static_cast<size_t>(vertical ? img.height() : img.width()), false);
    for (const QPoint &p : ink) {
        const int a = vertical ? p.x() : p.y();
        if (a >= tickBand.first && a <= tickBand.second) along[static_cast<size_t>(vertical ? p.y() : p.x())] = true;
    }
    const auto tickRuns = runs(along);
    for (size_t i = 0; i < tickRuns.size(); ++i) {
        TickLabel t;
        t.position = (tickRuns[i].first + tickRuns[i].second) * 0.5;
        // vertical: bottom tick is the minimum; horizontal: left tick is the minimum
        const size_t index = vertical ? tickRuns.size() - 1 - i : i;
        t.text = QString::number(maximum * static_cast<double>(index) / 6.0, 'f', 0);
        t.inkFirst = -1;
        t.inkLast = -1;
        out.push_back(t);
    }
    // Each label pixel belongs to the nearest tick along the scale axis
    for (const QPoint &p : ink) {
        const int a = vertical ? p.x() : p.y();
        if (a < labelStart) continue;
        const double along = vertical ? p.y() : p.x();
        size_t best = 0;
        for (size_t i = 1; i < out.size(); ++i) {
            if (std::abs(out[i].position - along) < std::abs(out[best].position - along)) best = i;
        }
        const int extent = p.x(); // labels are horizontal text in both orientations
        TickLabel &t = out[best];
        t.inkFirst = t.inkFirst < 0 ? extent : std::min(t.inkFirst, extent);
        t.inkLast = std::max(t.inkLast, extent);
    }
    return out;
}

void addScaleRows()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<double>("pointSize");
    for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
        for (double maximum : {10000.0, 100000.0}) {
            for (double pt : {9.0, 11.0, 13.0}) {
                QTest::addRow("%s 0-%.0f %.0fpt", o == Qt::Vertical ? "V" : "H", maximum, pt) << static_cast<int>(o) << maximum << pt;
            }
        }
    }
}

} // namespace

class tst_LevelMeter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void channelValuesAndClamping();
    void peakHoldAndReset();
    void overloadSignal();
    void displayModes();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void scaleLabelsNotClipped_data() { addScaleRows(); }
    void scaleLabelsNotClipped();
    void horizontalLabelsDoNotOverlapAtMinimumSize_data();
    void horizontalLabelsDoNotOverlapAtMinimumSize();
};

void tst_LevelMeter::defaultValues()
{
    LevelMeter meter;
    QCOMPARE(meter.channelCount(), 2);
    QCOMPARE(meter.minimum(), -60.0);
    QCOMPARE(meter.maximum(), 6.0);
    QCOMPARE(meter.displayMode(), LevelMeter::DisplayMode::Segmented);
    QCOMPARE(meter.orientation(), Qt::Vertical);
    QCOMPARE(meter.isPeakHoldEnabled(), true);
}

void tst_LevelMeter::channelValuesAndClamping()
{
    LevelMeter meter;
    meter.setRange(-40.0, 10.0);

    // Below minimum
    meter.setValue(0, -60.0);
    QCOMPARE(meter.value(0), -40.0);

    // Above maximum
    meter.setValue(1, 25.0);
    QCOMPARE(meter.value(1), 10.0);

    // Valid values
    meter.setValue(0, -12.0);
    meter.setValue(1, -6.0);
    QCOMPARE(meter.value(0), -12.0);
    QCOMPARE(meter.value(1), -6.0);
}

void tst_LevelMeter::peakHoldAndReset()
{
    LevelMeter meter;
    meter.setRange(0.0, 100.0);
    meter.setValue(0, 80.0);

    QCOMPARE(meter.peakValue(0), 80.0);

    // Drop value lower: peak should hold at 80.0
    meter.setValue(0, 40.0);
    QCOMPARE(meter.value(0), 40.0);
    QCOMPARE(meter.peakValue(0), 80.0);

    meter.resetPeaks();
    QCOMPARE(meter.peakValue(0), 40.0);
}

void tst_LevelMeter::overloadSignal()
{
    LevelMeter meter;
    meter.setRange(-20.0, 10.0);
    meter.setErrorThreshold(0.0);

    QSignalSpy overloadSpy(&meter, &LevelMeter::overloadOccurred);
    meter.setValue(0, -5.0);
    QCOMPARE(overloadSpy.count(), 0);

    // Exceed error threshold
    meter.setValue(0, 2.5);
    QCOMPARE(overloadSpy.count(), 1);
    QCOMPARE(overloadSpy.takeFirst().at(0).toInt(), 0);
}

void tst_LevelMeter::displayModes()
{
    LevelMeter meter;
    meter.setDisplayMode(LevelMeter::DisplayMode::Continuous);
    QCOMPARE(meter.displayMode(), LevelMeter::DisplayMode::Continuous);

    meter.setOrientation(Qt::Horizontal);
    QCOMPARE(meter.orientation(), Qt::Horizontal);
}

void tst_LevelMeter::signalEmission()
{
    LevelMeter meter;
    meter.setRange(0.0, 100.0);

    QSignalSpy spy(&meter, &LevelMeter::valueChanged);
    meter.setValue(0, 55.0);

    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0);
    QCOMPARE(args.at(1).toDouble(), 55.0);
}

void tst_LevelMeter::renderOffscreen()
{
    LevelMeter meter;
    meter.setChannelCount(2);
    meter.setValue(0, -12.0);
    meter.setValue(1, -6.0);
    meter.resize(80, 220);

    QPixmap pix(meter.size());
    meter.render(&pix);
    QVERIFY(!pix.isNull());

    // Continuous and horizontal mode
    meter.setDisplayMode(LevelMeter::DisplayMode::Continuous);
    meter.setOrientation(Qt::Horizontal);
    meter.resize(220, 80);
    QPixmap pixH(meter.size());
    meter.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_LevelMeter::extremeResizeNoCrash()
{
    LevelMeter meter;
    meter.resize(2, 2);
    QPixmap pixSmall(meter.size());
    meter.render(&pixSmall);

    meter.resize(2560, 1440);
    QPixmap pixLarge(meter.size());
    meter.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_LevelMeter::nanAndInfinityResilience()
{
    LevelMeter meter;
    meter.setRange(-60.0, 6.0);
    meter.setValue(0, -10.0);
    QCOMPARE(meter.value(0), -10.0);

    // NaN input ignored
    meter.setValue(0, std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(meter.value(0), -10.0);

    // +Inf and -Inf clamped
    meter.setValue(0, std::numeric_limits<double>::infinity());
    QCOMPARE(meter.value(0), 6.0);
    meter.setValue(0, -std::numeric_limits<double>::infinity());
    QCOMPARE(meter.value(0), -60.0);

    // Range with NaN/Inf ignored
    meter.setRange(std::numeric_limits<double>::quiet_NaN(), 10.0);
    QCOMPARE(meter.minimum(), -60.0);

    // Render offscreen
    meter.resize(100, 300);
    QPixmap pix(meter.size());
    meter.render(&pix);
    QVERIFY(!pix.isNull());
}

// #40: every scale label is drawn whole: its ink is as wide (within 1 device px) as the same text
// drawn alone in the scale font. Generous sizes, at least minimumSizeHint().
void tst_LevelMeter::scaleLabelsNotClipped()
{
    QFETCH(int, orientation);
    QFETCH(double, maximum);
    QFETCH(double, pointSize);
    const Qt::Orientation o = static_cast<Qt::Orientation>(orientation);
    auto m = scaleOnlyMeter(o, maximum, pointSize, o == Qt::Vertical ? QSize(220, 360) : QSize(720, 160));
    m->resize(m->size().expandedTo(m->minimumSizeHint()));
    const qreal dpr = grabMeter(*m).devicePixelRatio();

    const std::vector<TickLabel> labels = measureLabels(*m, maximum);
    QCOMPARE(static_cast<int>(labels.size()), 7);
    for (const TickLabel &t : labels) {
        QVERIFY2(t.inkFirst >= 0, qPrintable(QStringLiteral("label '%1' has no ink").arg(t.text)));
        const int width = t.inkLast - t.inkFirst + 1;
        const int reference = referenceInkWidth(t.text, m->font(), dpr);
        QVERIFY2(std::abs(width - reference) <= 1, qPrintable(QStringLiteral("label '%1' is %2 device px wide, %3 when drawn alone (%4x%5, DPR %6)")
                                                                .arg(t.text).arg(width).arg(reference).arg(m->width()).arg(m->height()).arg(dpr)));
    }
}

void tst_LevelMeter::horizontalLabelsDoNotOverlapAtMinimumSize_data()
{
    QTest::addColumn<double>("maximum");
    QTest::addColumn<double>("pointSize");
    for (double maximum : {100.0, 10000.0, 100000.0}) {
        for (double pt : {9.0, 11.0, 13.0}) {
            QTest::addRow("0-%.0f %.0fpt", maximum, pt) << maximum << pt;
        }
    }
}

// #40: at minimumSizeHint() of a horizontal meter with a linear scale, every label is whole and the
// labels of neighbouring ticks do not overlap (same rule as IndustrialSwitch, #44)
void tst_LevelMeter::horizontalLabelsDoNotOverlapAtMinimumSize()
{
    QFETCH(double, maximum);
    QFETCH(double, pointSize);
    auto m = scaleOnlyMeter(Qt::Horizontal, maximum, pointSize, QSize());
    const qreal dpr = grabMeter(*m).devicePixelRatio();
    const std::vector<TickLabel> labels = measureLabels(*m, maximum);
    QCOMPARE(static_cast<int>(labels.size()), 7);
    for (size_t i = 0; i < labels.size(); ++i) {
        const TickLabel &t = labels[i];
        const int reference = referenceInkWidth(t.text, m->font(), dpr);
        QVERIFY2(t.inkFirst >= 0 && std::abs((t.inkLast - t.inkFirst + 1) - reference) <= 1,
                 qPrintable(QStringLiteral("label '%1' is not whole at minimumSizeHint %2x%3 (ink %4..%5, %6 px alone)")
                                .arg(t.text).arg(m->width()).arg(m->height()).arg(t.inkFirst).arg(t.inkLast).arg(reference)));
        if (i > 0) {
            QVERIFY2(labels[i - 1].inkLast < t.inkFirst,
                     qPrintable(QStringLiteral("labels '%1' and '%2' overlap at minimumSizeHint %3x%4")
                                    .arg(labels[i - 1].text, t.text).arg(m->width()).arg(m->height())));
        }
    }
}

QTEST_MAIN(tst_LevelMeter)
#include "tst_LevelMeter.moc"


