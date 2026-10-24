// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LevelMeter.h>

#include "FontTestUtils.h"
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "internal/LevelMeterLayout.h"
#include "internal/LinearGaugeScale.h"

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

// --- #57 -----------------------------------------------------------------------------------------

// A meter whose zone colour paints only the scale (thresholds above the range, value at the minimum,
// continuous mode, no peak marker, no title); textColor paints the unit and the channel labels
std::unique_ptr<LevelMeter> labelMeter(Qt::Orientation o, double minimum, double maximum, const QString &unit,
                                       double pointSize, int channels)
{
    auto m = std::make_unique<LevelMeter>();
    m->setOrientation(o);
    m->setChannelCount(channels);
    m->setRange(minimum, maximum);
    m->setWarningThreshold(maximum + (maximum - minimum));
    m->setErrorThreshold(maximum + 2.0 * (maximum - minimum));
    m->setDisplayMode(LevelMeter::DisplayMode::Continuous);
    m->setPeakHoldEnabled(false);
    m->setUnit(unit);
    m->setTitle(QString());
    for (int c = 0; c < channels; ++c) {
        m->setValue(c, minimum);
    }
    QFont f = m->font();
    f.setPointSizeF(pointSize);
    m->setFont(f);
    return m;
}

std::vector<QPoint> differing(const QImage &a, const QImage &b)
{
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

// Pixels painted in textColor (unit, channel labels, title)
std::vector<QPoint> textInk(LevelMeter &m)
{
    m.setTextColor(QColor(255, 255, 255));
    const QImage a = grabMeter(m);
    m.setTextColor(QColor(255, 0, 0));
    return differing(a, grabMeter(m));
}

// Device-pixel ink box of a text drawn alone in the given font
QRect inkBoxAlone(const QString &text, const QFont &font, qreal dpr)
{
    const QFontMetricsF fm(font);
    const QSizeF logical(fm.horizontalAdvance(text) + 40.0, fm.height() + 40.0);
    QImage img((logical * dpr).toSize(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    {
        QPainter p(&img);
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

// Free device pixels between a box and a pixel along the axis that separates them
int separation(const QRect &box, const QPoint &p)
{
    const int gx = std::max(box.left() - p.x() - 1, p.x() - box.right() - 1);
    const int gy = std::max(box.top() - p.y() - 1, p.y() - box.bottom() - 1);
    return std::max(gx, gy);
}

// Groups pixels: two pixels are in the same cluster when fewer than `threshold` free pixels
// separate them along the axis that separates them
std::vector<QRect> clusters(const std::vector<QPoint> &ink, double threshold)
{
    const int reach = static_cast<int>(std::ceil(threshold));
    QHash<qint64, int> at;
    auto key = [](int x, int y) { return (static_cast<qint64>(y) << 32) | static_cast<quint32>(x); };
    for (int i = 0; i < static_cast<int>(ink.size()); ++i) {
        at.insert(key(ink[static_cast<size_t>(i)].x(), ink[static_cast<size_t>(i)].y()), i);
    }
    std::vector<int> parent(ink.size());
    for (size_t i = 0; i < parent.size(); ++i) {
        parent[i] = static_cast<int>(i);
    }
    auto root = [&](int i) {
        while (parent[static_cast<size_t>(i)] != i) {
            parent[static_cast<size_t>(i)] = parent[static_cast<size_t>(parent[static_cast<size_t>(i)])];
            i = parent[static_cast<size_t>(i)];
        }
        return i;
    };
    for (int i = 0; i < static_cast<int>(ink.size()); ++i) {
        const QPoint &p = ink[static_cast<size_t>(i)];
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) - 1 >= threshold) {
                    continue;
                }
                const auto it = at.constFind(key(p.x() + dx, p.y() + dy));
                if (it != at.constEnd()) {
                    parent[static_cast<size_t>(root(i))] = root(it.value());
                }
            }
        }
    }
    QHash<int, QRect> boxes;
    for (int i = 0; i < static_cast<int>(ink.size()); ++i) {
        boxes[root(i)] |= QRect(ink[static_cast<size_t>(i)], QSize(1, 1));
    }
    return std::vector<QRect>(boxes.cbegin(), boxes.cend());
}

double scaleMinGap(const QFont &widgetFont)
{
    return internal::scaleLabelMinGap(QFontMetricsF(internal::levelMeterScaleFont(widgetFont)).height());
}

} // namespace

class tst_LevelMeter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fontChangeRefreshesCache();
    void horizontalLabelsBelowTheTicks_data();
    void horizontalLabelsBelowTheTicks();
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
    void sizeHintsFollowTheLabels();
    void scaleLabelsKeepMinimumGap_data();
    void scaleLabelsKeepMinimumGap();
    void endLabelsDistinctAtMinimumSize_data();
    void endLabelsDistinctAtMinimumSize();
    void contentInsideChassisAtMinimumSize_data();
    void contentInsideChassisAtMinimumSize();
    void unitKeepsMinimumGapFromScale_data();
    void unitKeepsMinimumGapFromScale();
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

// #40: with labels that fit the previous 36 px scale margin (dB scale, default font) sizeHint() is
// the one used before; the minimum now includes the scale (it was a fixed 45 px, narrower than the
// chassis margins, bars and 36 px scale together); wider labels make both grow; sizeHint() is never
// below minimumSizeHint()
void tst_LevelMeter::sizeHintsFollowTheLabels()
{
    LevelMeter db;
    db.setChannelCount(2);
    db.setRange(-60.0, 6.0);
    db.setUnit(QStringLiteral("dB"));
    QCOMPARE(db.sizeHint(), QSize(24 + 2 * 18 + 38, 220));
    QCOMPARE(db.minimumSizeHint(), QSize(22 + 2 * 6 + 4 + 36, 100));

    for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
        auto wide = scaleOnlyMeter(o, 100000.0, 13.0, QSize(200, 200));
        const QSize min = wide->minimumSizeHint();
        const QSize hint = wide->sizeHint();
        QVERIFY2(hint.width() >= min.width() && hint.height() >= min.height(),
                 qPrintable(QStringLiteral("sizeHint %1x%2 below minimumSizeHint %3x%4").arg(hint.width()).arg(hint.height()).arg(min.width()).arg(min.height())));
        QVERIFY2(o == Qt::Vertical ? min.width() > 45 : min.width() > 100,
                 qPrintable(QStringLiteral("minimumSizeHint %1x%2 did not grow for 6-digit labels at 13 pt").arg(min.width()).arg(min.height())));
    }
}

enum SizeKind { AtMinimum, AtHint, Large, Width220, Height150 };

void tst_LevelMeter::scaleLabelsKeepMinimumGap_data()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<QString>("unit");
    QTest::addColumn<double>("pointSize");
    QTest::addColumn<int>("sizeKind");

    for (double pt : {9.0, 11.0, 13.0}) {
        for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
            const char *on = o == Qt::Vertical ? "V" : "H";
            // dB ladder
            QTest::addRow("%s dB min %.0fpt", on, pt) << int(o) << -60.0 << 6.0 << QStringLiteral("dB") << pt << int(AtMinimum);
            QTest::addRow("%s dB hint %.0fpt", on, pt) << int(o) << -60.0 << 6.0 << QStringLiteral("dB") << pt << int(AtHint);
            QTest::addRow("%s dB 400 %.0fpt", on, pt) << int(o) << -60.0 << 6.0 << QStringLiteral("dB") << pt << int(Large);
        }
        // Linear scale: vertical at the fixed 100 px minimum height, 150 px and the hint
        QTest::addRow("V 0-100000 min %.0fpt", pt) << int(Qt::Vertical) << 0.0 << 100000.0 << QString() << pt << int(AtMinimum);
        QTest::addRow("V 0-100000 150 %.0fpt", pt) << int(Qt::Vertical) << 0.0 << 100000.0 << QString() << pt << int(Height150);
        QTest::addRow("V 0-100 min %.0fpt", pt) << int(Qt::Vertical) << 0.0 << 100.0 << QString() << pt << int(AtMinimum);
        // Horizontal: at minimumSizeHint() (every label, see tst_LevelMeterLayout) and below it
        QTest::addRow("H 0-100000 min %.0fpt", pt) << int(Qt::Horizontal) << 0.0 << 100000.0 << QString() << pt << int(AtMinimum);
        QTest::addRow("H 0-10000 min %.0fpt", pt) << int(Qt::Horizontal) << 0.0 << 10000.0 << QString() << pt << int(AtMinimum);
        // Labels all as wide as the widest: the gap at the minimum is exactly scaleLabelMinGap()
        QTest::addRow("H 100000-700000 min %.0fpt", pt) << int(Qt::Horizontal) << 100000.0 << 700000.0 << QString() << pt << int(AtMinimum);
        QTest::addRow("H 0-100000 220 %.0fpt", pt) << int(Qt::Horizontal) << 0.0 << 100000.0 << QString() << pt << int(Width220);
    }
}

namespace {

// Scale label clusters of a meter (#57). Label pixels are the zone-colour pixels beyond the tick band
// (see labelMeter()), grouped into clusters: two pixels are in the same cluster when fewer than
// (minGap - 1) x DPR free device pixels separate them (1 px logical allowed for glyphs that exceed the
// font metrics the layout uses). The end ticks are located from the outermost tick ink.
struct ScaleScan {
    std::vector<QPoint> labelInk;
    std::vector<QRect> labels;
    QSize largest;     ///< largest label drawn alone (device px)
    double minGap{0};  ///< logical px
    double threshold{0};
    double minEnd{0};  ///< device position of the minimum end tick along the scale
    double maxEnd{0};
    qreal dpr{1};
    bool valid{false};
};

ScaleScan scanScale(LevelMeter &m, double minimum, double maximum, const QString &unit)
{
    ScaleScan scan;
    const bool vertical = m.orientation() == Qt::Vertical;
    const std::vector<QPoint> ink = scaleInk(m);
    const QImage img = grabMeter(m);
    scan.dpr = img.devicePixelRatio();
    std::vector<bool> across(static_cast<size_t>(vertical ? img.width() : img.height()), false);
    for (const QPoint &p : ink) across[static_cast<size_t>(vertical ? p.x() : p.y())] = true;
    const auto acrossRuns = runs(across);
    if (acrossRuns.size() < 2) return scan;
    const auto tickBand = acrossRuns.front();
    const int labelStart = acrossRuns[1].first;
    int alongLo = std::numeric_limits<int>::max();
    int alongHi = std::numeric_limits<int>::min();
    for (const QPoint &p : ink) {
        const int a = vertical ? p.x() : p.y();
        const int along = vertical ? p.y() : p.x();
        if (a >= labelStart) {
            scan.labelInk.push_back(p);
        } else if (a >= tickBand.first && a <= tickBand.second) {
            alongLo = std::min(alongLo, along);
            alongHi = std::max(alongHi, along);
        }
    }
    const QFont scaleFont = internal::levelMeterScaleFont(m.font());
    scan.minGap = scaleMinGap(m.font());
    scan.threshold = (scan.minGap - 1.0) * scan.dpr;
    const std::vector<double> ticks = internal::levelMeterTicks(minimum, maximum, unit);
    for (double t : ticks) {
        scan.largest = scan.largest.expandedTo(inkBoxAlone(internal::levelMeterTickText(t, minimum, maximum, unit), scaleFont, scan.dpr).size());
    }
    scan.labels = clusters(scan.labelInk, scan.threshold);
    // Pixel centres of the outermost tick ink; the end ticks of the ladder may lie inside the range
    const double low = alongLo + 0.5 * scan.dpr;
    const double high = alongHi + 1.0 - 0.5 * scan.dpr;
    auto tickAt = [&](double t) {
        const double norm = (t - minimum) / (maximum - minimum);
        return vertical ? high - norm * (high - low) : low + norm * (high - low);
    };
    scan.minEnd = tickAt(ticks.front());
    scan.maxEnd = tickAt(ticks.back());
    scan.valid = true;
    return scan;
}

// Along-scale distance from a position to a cluster (0 inside it)
double alongDistance(const QRect &c, bool vertical, double at)
{
    const double lo = vertical ? c.top() : c.left();
    const double hi = (vertical ? c.bottom() : c.right()) + 1.0;
    return std::max({lo - at, 0.0, at - hi});
}

// #57: the clusters nearest to the two end ticks are two distinct clusters, the one of the minimum
// on the side of the minimum. Empty when they are, otherwise what is wrong.
QString endLabelsDistinct(const ScaleScan &scan, bool vertical)
{
    if (scan.labels.empty()) return QStringLiteral("no label ink");
    auto nearest = [&](double at) {
        size_t best = 0;
        for (size_t i = 1; i < scan.labels.size(); ++i) {
            if (alongDistance(scan.labels[i], vertical, at) < alongDistance(scan.labels[best], vertical, at)) best = i;
        }
        return best;
    };
    const size_t lo = nearest(scan.minEnd);
    const size_t hi = nearest(scan.maxEnd);
    const QRect &a = scan.labels[lo];
    const QRect &b = scan.labels[hi];
    if (lo == hi) {
        return QStringLiteral("one label (ink %1x%2 at (%3, %4)) for both ends").arg(a.width()).arg(a.height()).arg(a.x()).arg(a.y());
    }
    const double ca = vertical ? a.center().y() : a.center().x();
    const double cb = vertical ? b.center().y() : b.center().x();
    if (vertical ? ca <= cb : ca >= cb) {
        return QStringLiteral("the label of the minimum (at %1) is not on the side of the minimum (maximum at %2)").arg(ca).arg(cb);
    }
    return QString();
}

} // namespace

// #57: drawn scale labels keep scaleLabelMinGap() (on the scale font height) and both ends carry a
// label. Each label is one cluster (scanScale()); labels closer than the minimum merge into a cluster
// larger than any label drawn alone (+1 px logical).
// Ends: on the dB rows at minimumSizeHint() the labels nearest to the two end ticks must be two
// distinct clusters, the minimum's on its side (endLabelsDistinct()); on the other rows some label ink
// lies within half a label (the largest drawn alone, along the scale) of each end tick. Which labels
// are drawn (the ends themselves, 0 dB, every linear label at the minimum) is checked by the helper
// tests (tst_LevelMeterLayout): without fonts (Windows offscreen CI) glyphs are boxes whose ink does
// not follow the font metrics, so the pixels cannot tell neighbouring labels apart reliably.
void tst_LevelMeter::scaleLabelsKeepMinimumGap()
{
    QFETCH(int, orientation);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(QString, unit);
    QFETCH(double, pointSize);
    QFETCH(int, sizeKind);
    const Qt::Orientation o = static_cast<Qt::Orientation>(orientation);
    const bool vertical = o == Qt::Vertical;

    auto m = labelMeter(o, minimum, maximum, unit, pointSize, 1);
    QSize size = m->minimumSizeHint();
    switch (sizeKind) {
    case AtHint: size = m->sizeHint(); break;
    case Large: size = vertical ? QSize(m->sizeHint().width(), 400) : QSize(400, m->sizeHint().height()); break;
    case Width220: size = QSize(220, m->minimumSizeHint().height()); break;
    case Height150: size = QSize(m->sizeHint().width(), 150); break;
    default: break;
    }
    m->resize(size);

    const ScaleScan scan = scanScale(*m, minimum, maximum, unit);
    QVERIFY2(scan.valid, "no tick band and label band");
    const qreal dpr = scan.dpr;
    const int tolerance = static_cast<int>(std::ceil(dpr));
    for (const QRect &c : scan.labels) {
        QVERIFY2(c.width() <= scan.largest.width() + tolerance && c.height() <= scan.largest.height() + tolerance,
                 qPrintable(QStringLiteral("label ink %1x%2 at (%3, %4) is larger than the largest label alone %5x%6: labels "
                                           "closer than %7 device px (minimum gap %8 px, %9x%10, DPR %11)")
                                .arg(c.width()).arg(c.height()).arg(c.x()).arg(c.y()).arg(scan.largest.width()).arg(scan.largest.height())
                                .arg(scan.threshold).arg(scan.minGap).arg(size.width()).arg(size.height()).arg(dpr)));
    }

    if (unit == QLatin1String("dB") && sizeKind == AtMinimum) {
        const QString problem = endLabelsDistinct(scan, vertical);
        QVERIFY2(problem.isEmpty(), qPrintable(QStringLiteral("%1 (%2x%3, DPR %4)").arg(problem).arg(size.width()).arg(size.height()).arg(dpr)));
        return;
    }
    const double halfLabel = 0.5 * (vertical ? scan.largest.height() : scan.largest.width());
    for (double at : {scan.minEnd, scan.maxEnd}) {
        const bool found = std::any_of(scan.labels.begin(), scan.labels.end(),
                                       [&](const QRect &c) { return alongDistance(c, vertical, at) <= halfLabel; });
        QVERIFY2(found, qPrintable(QStringLiteral("no label ink within %1 device px of the end tick at device %2 (%3x%4, DPR %5)")
                                       .arg(halfLabel).arg(at).arg(size.width()).arg(size.height()).arg(dpr)));
    }
}

void tst_LevelMeter::endLabelsDistinctAtMinimumSize_data()
{
    QTest::addColumn<double>("pointSize");
    for (double pt : {16.0, 20.0, 24.0}) {
        QTest::addRow("%.0fpt", pt) << pt;
    }
}

// #57: a horizontal dB meter (unit "dB") at minimumSizeHint() with a large font (16, 20, 24 pt): both
// ends carry a label (two distinct clusters, the minimum's on its side, see endLabelsDistinct()) and
// all the label ink lies inside the chassis (#44 rule at the minimum). Found on Windows CI, whose box
// glyphs are wider than the font metrics. Large sizes, not letter spacing: spaced glyphs would split a
// label into several clusters.
void tst_LevelMeter::endLabelsDistinctAtMinimumSize()
{
    QFETCH(double, pointSize);
    auto m = labelMeter(Qt::Horizontal, -60.0, 6.0, QStringLiteral("dB"), pointSize, 1);
    m->resize(m->minimumSizeHint());

    const ScaleScan scan = scanScale(*m, -60.0, 6.0, QStringLiteral("dB"));
    QVERIFY2(scan.valid, "no tick band and label band");
    const QString problem = endLabelsDistinct(scan, false);
    QVERIFY2(problem.isEmpty(), qPrintable(QStringLiteral("%1 (minimumSizeHint %2x%3, DPR %4)")
                                               .arg(problem).arg(m->width()).arg(m->height()).arg(scan.dpr)));
    const QRectF chassis(3.0 * scan.dpr, 3.0 * scan.dpr, (m->width() - 6.0) * scan.dpr, (m->height() - 6.0) * scan.dpr);
    for (const QPoint &p : scan.labelInk) {
        QVERIFY2(p.x() >= chassis.left() && p.x() + 1 <= chassis.right() && p.y() >= chassis.top() && p.y() + 1 <= chassis.bottom(),
                 qPrintable(QStringLiteral("label ink at device (%1, %2) outside the chassis at minimumSizeHint %3x%4 (DPR %5)")
                                .arg(p.x()).arg(p.y()).arg(m->width()).arg(m->height()).arg(scan.dpr)));
    }
}

void tst_LevelMeter::contentInsideChassisAtMinimumSize_data()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<int>("channels");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("unit");
    for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
        for (int channels : {1, 2, 4}) {
            for (const QString &title : {QString(), QStringLiteral("Mix")}) {
                for (const QString &unit : {QString(), QStringLiteral("dB")}) {
                    QTest::addRow("%s %dch title '%s' unit '%s'", o == Qt::Vertical ? "V" : "H", channels, qPrintable(title),
                                  qPrintable(unit))
                        << int(o) << channels << title << unit;
                }
            }
        }
    }
}

// #57: at minimumSizeHint() the bars, the scale, the channel labels, the title and the unit are drawn
// inside the chassis (rect() less 3 px on each side). Bars full in the zone colour; textColor paints
// the channel labels, the title and the unit.
void tst_LevelMeter::contentInsideChassisAtMinimumSize()
{
    QFETCH(int, orientation);
    QFETCH(int, channels);
    QFETCH(QString, title);
    QFETCH(QString, unit);

    auto m = labelMeter(static_cast<Qt::Orientation>(orientation), -60.0, 6.0, unit, 9.0, channels);
    m->setFont(QFont());
    m->setTitle(title);
    for (int c = 0; c < channels; ++c) {
        m->setValue(c, 6.0);
    }
    m->resize(m->minimumSizeHint());
    const qreal dpr = grabMeter(*m).devicePixelRatio();
    std::vector<QPoint> ink = scaleInk(*m);
    const std::vector<QPoint> text = textInk(*m);
    ink.insert(ink.end(), text.begin(), text.end());
    QVERIFY(!ink.empty());
    const QRectF chassis(3.0 * dpr, 3.0 * dpr, (m->width() - 6.0) * dpr, (m->height() - 6.0) * dpr);
    for (const QPoint &p : ink) {
        QVERIFY2(p.x() >= chassis.left() && p.x() + 1 <= chassis.right() && p.y() >= chassis.top() && p.y() + 1 <= chassis.bottom(),
                 qPrintable(QStringLiteral("ink at device (%1, %2) outside the chassis at minimumSizeHint %3x%4 (DPR %5)")
                                .arg(p.x()).arg(p.y()).arg(m->width()).arg(m->height()).arg(dpr)));
    }
}

void tst_LevelMeter::unitKeepsMinimumGapFromScale_data()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<QString>("unit");
    QTest::addColumn<double>("pointSize");
    QTest::addColumn<int>("sizeKind");
    for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
        for (double pt : {9.0, 11.0, 13.0}) {
            for (int kind : {int(AtMinimum), int(AtHint)}) {
                const char *on = o == Qt::Vertical ? "V" : "H";
                const char *kn = kind == AtMinimum ? "min" : "hint";
                QTest::addRow("%s dB %s %.0fpt", on, kn, pt) << int(o) << -60.0 << 6.0 << QStringLiteral("dB") << pt << kind;
                QTest::addRow("%s 0-10000 RPM %s %.0fpt", on, kn, pt) << int(o) << 0.0 << 10000.0 << QStringLiteral("RPM") << pt << kind;
            }
        }
    }
}

// #57: the unit text keeps scaleLabelMinGap() (1 px logical allowed, as for the labels) from every
// scale pixel (ticks and labels). The channel labels are empty, so textColor paints only the unit.
void tst_LevelMeter::unitKeepsMinimumGapFromScale()
{
    QFETCH(int, orientation);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(QString, unit);
    QFETCH(double, pointSize);
    QFETCH(int, sizeKind);

    auto m = labelMeter(static_cast<Qt::Orientation>(orientation), minimum, maximum, unit, pointSize, 1);
    m->setChannelLabels({QString()}); // empty, not " ": without fonts a space is drawn as a box
    m->resize(sizeKind == AtHint ? m->sizeHint() : m->minimumSizeHint());
    const qreal dpr = grabMeter(*m).devicePixelRatio();
    QRect unitBox;
    for (const QPoint &p : textInk(*m)) unitBox |= QRect(p, QSize(1, 1));
    QVERIFY2(!unitBox.isEmpty(), "the unit is not drawn");
    const double required = (scaleMinGap(m->font()) - 1.0) * dpr;
    int closest = std::numeric_limits<int>::max();
    QPoint where;
    for (const QPoint &p : scaleInk(*m)) {
        if (separation(unitBox, p) < closest) {
            closest = separation(unitBox, p);
            where = p;
        }
    }
    QVERIFY2(closest >= required,
             qPrintable(QStringLiteral("unit ink %1x%2 at (%3, %4) is %5 device px from the scale ink at (%6, %7), at least %8 "
                                       "required (%9x%10, DPR %11)")
                            .arg(unitBox.width()).arg(unitBox.height()).arg(unitBox.x()).arg(unitBox.y()).arg(closest)
                            .arg(where.x()).arg(where.y()).arg(required).arg(m->width()).arg(m->height()).arg(dpr)));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_LevelMeter::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<LevelMeter>();
        if (font) g->setFont(*font);
        g->setTitle(QStringLiteral("AUDIO"));
        g->resize(170, 320);
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

void tst_LevelMeter::horizontalLabelsBelowTheTicks_data()
{
    QTest::addColumn<double>("pointSize");
    QTest::addColumn<QString>("unit");
    for (double pt : {16.0, 20.0, 24.0}) {
        QTest::addRow("dB %.0fpt", pt) << pt << QStringLiteral("dB");
        QTest::addRow("linear %.0fpt", pt) << pt << QString();
    }
}

// #51 (fixed in #61): on a horizontal meter with a large font the scale labels never rise over the
// ticks. The rows holding scale ink (zone colour, see labelMeter()) must form a tick band and, after
// at least one empty row, a label band; if the text rises over the ticks the two merge into one.
void tst_LevelMeter::horizontalLabelsBelowTheTicks()
{
    QFETCH(double, pointSize);
    QFETCH(QString, unit);
    const double minimum = unit.isEmpty() ? 0.0 : -60.0;
    const double maximum = unit.isEmpty() ? 100000.0 : 6.0;
    auto m = labelMeter(Qt::Horizontal, minimum, maximum, unit, pointSize, 1);
    m->resize(m->sizeHint());
    const std::vector<QPoint> ink = scaleInk(*m);
    const QImage img = grabMeter(*m);
    const qreal dpr = img.devicePixelRatio();
    std::vector<bool> rows(static_cast<size_t>(img.height()), false);
    for (const QPoint &p : ink) rows[static_cast<size_t>(p.y())] = true;
    const auto bands = runs(rows);
    QVERIFY2(bands.size() >= 2, qPrintable(QStringLiteral("%1 band of scale ink: label text over the ticks (%2x%3, DPR %4)")
                                               .arg(bands.size()).arg(m->width()).arg(m->height()).arg(dpr)));
}

QTEST_MAIN(tst_LevelMeter)
#include "tst_LevelMeter.moc"


