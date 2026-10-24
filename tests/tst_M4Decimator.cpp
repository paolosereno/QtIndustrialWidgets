// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <internal/DecimatedTrace.h>
#include <internal/M4Decimator.h>
#include <internal/StripChartGeometry.h>
#include <cmath>
#include <random>
#include <set>
#include <vector>

using namespace QtIndustrialWidgets::internal;

class tst_M4Decimator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void drawnColumnExtent();
    void columnStrokeStaysWithinPolylineExtent();
    void spikePreservation();
    void vertexBound();
    void bucketAggregatesAreIndependentOfWindowStart();
    void incrementalEqualsBatch();
    void propertyTestIncrementalEqualsBatch();
    void mulDivExact();
    void bucketGridBoundaries();
    void bucketGridMoreBucketsThanUnits();
    void mulDivSaturationEdges();
};

// #90: the decimated trace, drawn as StripChart draws it (internal::drawDecimatedSegment()), lights in every device
// column exactly the vertical extent the raw samples call for. The expected extent of a column comes only from the raw
// samples whose bucket is that column (PlotXMapping): their minimum and maximum, and the half connections to the
// adjacent columns of the same trace, which end halfway between the column's last raw sample and the next column's
// first one (and the other way round). A column holding a non-finite sample stays blank (#11).
//
// Tolerance: one pixel row at each end of the extent, from Qt's pixel model for aliased drawing: a one-pixel pen
// renders the pixel to the right of and below the mathematical point, so a fractional y lands on row floor(y) or on
// the row next to it depending on the direction of the line (Qt documentation, "Coordinate System", section
// "Aliased Painting": https://doc.qt.io/qt-6/coordsys.html#aliased-painting).
void tst_M4Decimator::drawnColumnExtent()
{
    enum class SignalType { UniformNoise, Sine3Periods, SquareWave, SparseSpikes, Chirp, SineDropouts };
    struct SignalConfig {
        SignalType type;
        const char *name;
    };
    const SignalConfig signalConfigs[] = {
        {SignalType::UniformNoise, "UniformNoise"}, {SignalType::Sine3Periods, "Sine3Periods"},
        {SignalType::SquareWave, "SquareWave"},     {SignalType::SparseSpikes, "SparseSpikes"},
        {SignalType::Chirp, "Chirp"},               {SignalType::SineDropouts, "SineDropouts"},
    };
    const int widths[] = {799, 800, 1920};
    const bool alignments[] = {true, false}; // aligned: the first sample starts a bucket
    const int height = 400;
    const double yMin = -110.0;
    const double yMax = 110.0;
    const int samplesPerBucket = 1000;
    constexpr int kRowTolerance = 1; // Qt's aliased pixel model, see above

    for (const auto &sig : signalConfigs) {
        for (int width : widths) {
            for (bool aligned : alignments) {
                const int sampleCount = width * samplesPerBucket;
                const qint64 dt_px = samplesPerBucket;
                const qint64 tOffset = aligned ? 0LL : (dt_px / 3);
                std::vector<qint64> timestamps(static_cast<size_t>(sampleCount));
                std::vector<double> values(static_cast<size_t>(sampleCount));
                std::mt19937_64 rng(42 + static_cast<int>(sig.type) * 100 + width);
                std::uniform_real_distribution<double> dist(-100.0, 100.0);
                for (int i = 0; i < sampleCount; ++i) {
                    const auto u = static_cast<size_t>(i);
                    timestamps[u] = tOffset + static_cast<qint64>(i);
                    const double frac = static_cast<double>(i) / static_cast<double>(sampleCount - 1);
                    switch (sig.type) {
                    case SignalType::UniformNoise:
                        values[u] = dist(rng);
                        break;
                    case SignalType::Sine3Periods:
                    case SignalType::SineDropouts:
                        values[u] = 100.0 * std::sin(2.0 * M_PI * 3.0 * frac);
                        break;
                    case SignalType::SquareWave: {
                        const double bucketIdx = static_cast<double>(timestamps[u] - tOffset) / static_cast<double>(dt_px);
                        values[u] = (std::fmod(bucketIdx, 16.5) < 8.25) ? 80.0 : -80.0;
                        break;
                    }
                    case SignalType::SparseSpikes:
                        values[u] = 0.0;
                        break;
                    case SignalType::Chirp:
                        values[u] = 100.0 * std::sin(2.0 * M_PI * (1.0 + 499.0 * frac) * frac);
                        break;
                    }
                }
                if (sig.type == SignalType::SparseSpikes) {
                    const int spikeInterval = sampleCount / 10;
                    for (int k = 1; k <= 9; ++k) {
                        values[static_cast<size_t>(k * spikeInterval + 42)] = (k % 2 == 0) ? 95.0 : -95.0;
                    }
                }
                if (sig.type == SignalType::SineDropouts) {
                    // Dropouts shorter than a bucket (one bucket holds finite and non-finite samples) and longer
                    // than several buckets, NaN and +-Inf
                    const double nonFinite[] = {std::numeric_limits<double>::quiet_NaN(),
                                                std::numeric_limits<double>::infinity(),
                                                -std::numeric_limits<double>::infinity()};
                    for (int d = 1; d <= 6; ++d) {
                        const int start = d * sampleCount / 7 + 123;
                        const int length = (d % 2 == 0) ? 40 : 3 * samplesPerBucket + 17;
                        for (int i = start; i < start + length && i < sampleCount; ++i) {
                            values[static_cast<size_t>(i)] = nonFinite[d % 3];
                        }
                    }
                }

                const BucketGrid grid(1, dt_px);
                const qint64 kStart = grid.bucketOf(timestamps.front());
                const PlotXMapping xmap = makePlotXMapping(grid, kStart, 0.0); // DPR 1 image: device = logical
                auto mapY = [&](double v) { return height - ((v - yMin) / (yMax - yMin)) * (height - 20.0) - 10.0; };
                auto identity = [](double x) { return x; };
                // StripChart's trace pen for decimated traces (and for its cosmetic raw test mode)
                QPen pen(Qt::white, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                pen.setCosmetic(true);

                // Raw samples, as StripChart draws them: continuous positions, broken at non-finite values
                QImage rawImage(width, height, QImage::Format_ARGB32_Premultiplied);
                rawImage.fill(Qt::black);
                {
                    QPainter p(&rawImage);
                    p.setRenderHint(QPainter::Antialiasing, false);
                    p.setPen(pen);
                    QPolygonF poly;
                    for (int i = 0; i < sampleCount; ++i) {
                        const auto u = static_cast<size_t>(i);
                        const qint64 k = grid.bucketOf(timestamps[u]) - kStart;
                        if (k < 0 || k >= width) continue;
                        if (!std::isfinite(values[u])) {
                            p.drawPolyline(poly);
                            poly.clear();
                            continue;
                        }
                        poly << QPointF(xmap.positionDev(timestamps[u]), mapY(values[u]));
                    }
                    p.drawPolyline(poly);
                }
                // M4, drawn with StripChart's own function
                QImage decImage(width, height, QImage::Format_ARGB32_Premultiplied);
                decImage.fill(Qt::black);
                {
                    QPainter p(&decImage);
                    p.setRenderHint(QPainter::Antialiasing, false);
                    p.setPen(pen);
                    QPolygonF poly;
                    for (const auto &seg : M4Decimator::decimateToSegments(timestamps.data(), values.data(),
                                                                          timestamps.size(), grid, kStart, width, 0)) {
                        if (!seg.empty()) drawDecimatedSegment(p, xmap, seg, poly, mapY, identity);
                    }
                }

                // Expected extent of each column, from the raw samples only
                struct Column {
                    bool any = false, nonFinite = false;
                    double first = 0.0, last = 0.0, lo = std::numeric_limits<double>::infinity(),
                           hi = -std::numeric_limits<double>::infinity();
                };
                std::vector<Column> cols(static_cast<size_t>(width));
                for (size_t u = 0; u < timestamps.size(); ++u) {
                    const qint64 k = grid.bucketOf(timestamps[u]) - kStart;
                    if (k < 0 || k >= width) continue;
                    Column &c = cols[static_cast<size_t>(k)];
                    if (!std::isfinite(values[u])) {
                        c.nonFinite = true;
                        continue;
                    }
                    if (!c.any) c.first = values[u];
                    c.any = true;
                    c.last = values[u];
                    c.lo = std::min(c.lo, values[u]);
                    c.hi = std::max(c.hi, values[u]);
                }
                auto connects = [&](int k) { // column k is part of the trace (raw finite samples, no dropout)
                    return k >= 0 && k < width && cols[static_cast<size_t>(k)].any && !cols[static_cast<size_t>(k)].nonFinite;
                };
                auto litRows = [&](const QImage &img, int x, int &top, int &bottom) {
                    top = bottom = -1;
                    for (int y = 0; y < height; ++y) {
                        if (img.pixel(x, y) & 0x00ffffffU) {
                            if (top < 0) top = y;
                            bottom = y;
                        }
                    }
                };
                auto litNear = [&](const QImage &img, int x, int row) { // a lit pixel within the tolerance of row
                    for (int y = std::max(0, row - kRowTolerance); y <= std::min(height - 1, row + kRowTolerance); ++y) {
                        if (img.pixel(x, y) & 0x00ffffffU) return y;
                    }
                    return -1;
                };

                int checked = 0;
                int blanks = 0;
                int rawCompared = 0;    // min/max rows compared with the raw drawing in their own column
                int rawInNeighbour = 0; // min/max rows the raw drawing shows in an adjacent column
                for (int x = 0; x < width; ++x) {
                    const Column &c = cols[static_cast<size_t>(x)];
                    int top = -1;
                    int bottom = -1;
                    litRows(decImage, x, top, bottom);
                    const QString where = QStringLiteral("%1 W=%2 %3 column %4").arg(QLatin1String(sig.name)).arg(width)
                                              .arg(aligned ? QStringLiteral("aligned") : QStringLiteral("non-aligned")).arg(x);
                    if (c.nonFinite) {
                        // #11: the column of a bucket holding a non-finite sample stays blank
                        QVERIFY2(top < 0, qPrintable(where + QStringLiteral(": dropout column lit at rows %1..%2").arg(top).arg(bottom)));
                        ++blanks;
                        continue;
                    }
                    if (!c.any) continue;
                    double yTop = mapY(c.hi);
                    double yBottom = mapY(c.lo);
                    if (connects(x - 1)) { // half connection from the previous column's last raw sample
                        const double mid = 0.5 * (mapY(cols[static_cast<size_t>(x - 1)].last) + mapY(c.first));
                        yTop = std::min(yTop, mid);
                        yBottom = std::max(yBottom, mid);
                    }
                    if (connects(x + 1)) { // half connection to the next column's first raw sample
                        const double mid = 0.5 * (mapY(c.last) + mapY(cols[static_cast<size_t>(x + 1)].first));
                        yTop = std::min(yTop, mid);
                        yBottom = std::max(yBottom, mid);
                    }
                    const int expTop = static_cast<int>(std::floor(yTop));
                    const int expBottom = static_cast<int>(std::floor(yBottom));
                    QVERIFY2(top >= 0 && std::abs(top - expTop) <= kRowTolerance && std::abs(bottom - expBottom) <= kRowTolerance,
                             qPrintable(where + QStringLiteral(": drawn rows %1..%2, expected %3..%4 (+-%5)")
                                                   .arg(top).arg(bottom).arg(expTop).arg(expBottom).arg(kRowTolerance)));

                    // Independent check against the raw drawing: the rows where the raw trace shows the column's
                    // maximum and minimum are lit by the decimated trace too. On a steep stretch the raw segment that
                    // ends on such a sample can light that row in the adjacent column instead (the sample lies at the
                    // column edge and the line's end pixel follows the direction of the line); the raw trace then shows
                    // no row for that value in this column, and it must show it in an adjacent one.
                    for (double value : {c.hi, c.lo}) {
                        const int row = static_cast<int>(std::floor(mapY(value)));
                        const int rawRow = litNear(rawImage, x, row);
                        if (rawRow < 0) {
                            QVERIFY2((x > 0 && litNear(rawImage, x - 1, row) >= 0) || (x + 1 < width && litNear(rawImage, x + 1, row) >= 0),
                                     qPrintable(where + QStringLiteral(": raw trace does not show %1 near row %2").arg(value).arg(row)));
                            ++rawInNeighbour;
                            continue;
                        }
                        QVERIFY2(litNear(decImage, x, rawRow) >= 0,
                                 qPrintable(where + QStringLiteral(": raw shows %1 at row %2, decimated trace does not")
                                                       .arg(value).arg(rawRow)));
                        ++rawCompared;
                    }
                    ++checked;
                }
                QVERIFY(checked > width / 2);
                // Most min/max rows are compared in their own column (in the data above at least 90 %)
                QVERIFY2(rawCompared >= 9 * (rawCompared + rawInNeighbour) / 10,
                         qPrintable(QStringLiteral("%1 W=%2: %3 rows compared, %4 shown in an adjacent column")
                                        .arg(QLatin1String(sig.name)).arg(width).arg(rawCompared).arg(rawInNeighbour)));
                if (sig.type == SignalType::SineDropouts) QVERIFY2(blanks >= 6, qPrintable(QString::number(blanks)));
            }
        }
    }
}

// #73: drawDecimatedSegment() draws each column as one vertical stroke (min to max) plus the connections between
// columns, instead of the polyline through the M4 vertices that was drawn before. Compared with that polyline, drawn
// here as the reference (appendDecimatedVertices(), drawPolyline(), drawSegmentEndPixels()), every device column lights:
// - the same columns (a column blank in the reference, e.g. a dropout column (#11), stays blank);
// - no pixel outside the reference's vertical extent of that column, except at most one row above its top or one row
//   below its bottom;
// - no isolated pixel (every lit pixel has a lit 8-neighbour), so no pixel detached from the trace.
// The one-row tolerance is the difference approved by the maintainer on #73. It comes from Qt's pixel model for
// aliased drawing (the rule drawnColumnExtent() uses): a fractional y lands on row floor(y) or on the next row
// depending on the direction of the line, and a separate line ends where a polyline turns (Qt documentation,
// "Coordinate System", section "Aliased Painting": https://doc.qt.io/qt-6/coordsys.html#aliased-painting).
// Data: two signals of bench_StripChart (where #73 found pixels detached from the trace) with 10k samples in its 10 s
// window, at its plot size, DPR 1 and 2: the cases that catch the detached pixels of the #73 prototype drawn with a
// square cap, at both DPRs (SmoothSine: steep joins; SpikesAndNaN: spikes and dropout columns). BroadbandNoise and
// 100k / 1M samples, measured on #73, add time without catching more.
void tst_M4Decimator::columnStrokeStaysWithinPolylineExtent()
{
    enum class SignalType { SmoothSine, SpikesAndNaN };
    const struct {
        SignalType type;
        const char *name;
    } signalConfigs[] = {{SignalType::SmoothSine, "SmoothSine"}, {SignalType::SpikesAndNaN, "SpikesAndNaN"}};
    constexpr qint64 kSpanNs = 10000000000LL; // 10 s window
    constexpr int kWidth = 1920;              // logical plot size of bench_StripChart
    constexpr int kHeight = 1024;
    constexpr int kRowTolerance = 1; // approved on #73, see above

    for (const qreal dpr : {1.0, 2.0}) {
        const int widthDev = static_cast<int>(kWidth * dpr);
        const int heightDev = static_cast<int>(kHeight * dpr);
        const BucketGrid grid(widthDev, kSpanNs);
        const PlotXMapping xmap = makePlotXMapping(grid, 0, 0.0);
        auto toLogicalX = [dpr](double xDev) { return xDev / dpr; };
        for (const int samples : {10000}) {
            for (const auto &sig : signalConfigs) {
                // bench_StripChart::generateSignal(), channel 0
                const qint64 dtNs = kSpanNs / samples;
                std::vector<qint64> timestamps(static_cast<size_t>(samples));
                std::vector<double> values(static_cast<size_t>(samples));
                for (int i = 0; i < samples; ++i) {
                    const qint64 tNs = static_cast<qint64>(i) * dtNs;
                    const double phase = 0.001 * (static_cast<double>(tNs) / 10000.0);
                    double v = std::sin(phase) * 50.0 + 50.0;
                    if (sig.type == SignalType::SpikesAndNaN) {
                        if (tNs % 100000000 < dtNs) v = 500.0;
                        else if (tNs % 250000000 < 50000) v = std::numeric_limits<double>::quiet_NaN();
                    }
                    timestamps[static_cast<size_t>(i)] = tNs;
                    values[static_cast<size_t>(i)] = v;
                }
                const double lo = 0.0;
                const double hi = sig.type == SignalType::SpikesAndNaN ? 500.0 : 100.0;
                auto mapY = [&](double v) { return kHeight - ((v - lo) / (hi - lo)) * (kHeight - 20.0) - 10.0; };
                const auto segments = M4Decimator::decimateToSegments(timestamps.data(), values.data(), timestamps.size(),
                                                                      grid, 0, widthDev, 0);
                QPen pen(Qt::white, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin); // StripChart's decimated trace pen
                pen.setCosmetic(true);
                auto render = [&](bool reference) {
                    QImage img(widthDev, heightDev, QImage::Format_RGB32);
                    img.setDevicePixelRatio(dpr);
                    img.fill(Qt::black);
                    QPainter p(&img);
                    p.setRenderHint(QPainter::Antialiasing, false);
                    p.setPen(pen);
                    QPolygonF poly;
                    for (const auto &seg : segments) {
                        if (seg.empty()) continue;
                        if (reference) { // the drawing before #73
                            poly.clear();
                            appendDecimatedVertices(poly, xmap, seg, mapY, toLogicalX);
                            p.drawPolyline(poly);
                            drawSegmentEndPixels(p, xmap, seg, poly, toLogicalX);
                        } else {
                            drawDecimatedSegment(p, xmap, seg, poly, mapY, toLogicalX);
                        }
                    }
                    return img;
                };
                const QImage ref = render(true);
                const QImage img = render(false);
                auto lit = [&](const QImage &im, int x, int y) {
                    return x >= 0 && y >= 0 && x < widthDev && y < heightDev && (im.pixel(x, y) & 0x00ffffffU);
                };
                auto isolated = [&](const QImage &im, int x, int y) {
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if ((dx || dy) && lit(im, x + dx, y + dy)) return false;
                    return true;
                };
                int litColumns = 0;
                int extendedRows = 0;
                for (int x = 0; x < widthDev; ++x) {
                    int top = -1;
                    int bottom = -1;
                    for (int y = 0; y < heightDev; ++y) {
                        if (lit(ref, x, y)) {
                            if (top < 0) top = y;
                            bottom = y;
                        }
                    }
                    const QString where = QStringLiteral("%1 %2 samples DPR %3 column %4")
                                              .arg(QLatin1String(sig.name)).arg(samples).arg(dpr).arg(x);
                    for (int y = 0; y < heightDev; ++y) {
                        if (!lit(img, x, y)) continue;
                        QVERIFY2(top >= 0, qPrintable(where + QStringLiteral(": lit at row %1, blank in the reference").arg(y)));
                        QVERIFY2(y >= top - kRowTolerance && y <= bottom + kRowTolerance,
                                 qPrintable(where + QStringLiteral(": lit at row %1, reference extent %2..%3 (+-%4)")
                                                       .arg(y).arg(top).arg(bottom).arg(kRowTolerance)));
                        QVERIFY2(!isolated(img, x, y) || isolated(ref, x, y),
                                 qPrintable(where + QStringLiteral(": isolated pixel at row %1").arg(y)));
                        if (y < top || y > bottom) ++extendedRows;
                    }
                    if (top >= 0) {
                        bool any = false;
                        for (int y = top - kRowTolerance; y <= bottom + kRowTolerance && !any; ++y) any = lit(img, x, y);
                        QVERIFY2(any, qPrintable(where + QStringLiteral(": blank, reference lit at rows %1..%2").arg(top).arg(bottom)));
                        ++litColumns;
                    }
                }
                QVERIFY(litColumns > widthDev * 9 / 10);
                qInfo("%s %d samples DPR %g: %d lit columns, %d pixels one row beyond the reference extent", sig.name,
                      samples, dpr, litColumns, extendedRows);
            }
        }
    }
}

void tst_M4Decimator::spikePreservation()
{
    constexpr int sampleCount = 1000000;
    constexpr int width = 800;
    const qint64 dt_px = sampleCount / width;

    std::vector<qint64> timestamps(sampleCount);
    std::vector<double> values(sampleCount, 10.0);

    // Single extreme spike in the middle of a bucket
    const int spikeIndex = 543210;
    const double spikeValue = 999.876;
    values[spikeIndex] = spikeValue;

    for (int i = 0; i < sampleCount; ++i) {
        timestamps[i] = static_cast<qint64>(i);
    }

    auto segments = M4Decimator::decimateToSegments(
        timestamps.data(), values.data(), sampleCount, dt_px, 0, width, 0);

    bool spikeFound = false;
    for (const auto &seg : segments) {
        for (const auto &pt : seg) {
            if (pt.t == spikeIndex && qFuzzyCompare(pt.value, spikeValue)) {
                spikeFound = true;
                break;
            }
        }
        if (spikeFound) break;
    }

    QVERIFY2(spikeFound, "Single-sample spike was lost in M4 decimation");
}

void tst_M4Decimator::vertexBound()
{
    constexpr int sampleCount = 1000000;
    constexpr int width = 800;
    const qint64 dt_px = sampleCount / width;

    std::vector<qint64> timestamps(sampleCount);
    std::vector<double> values(sampleCount);

    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(-50.0, 50.0);
    for (int i = 0; i < sampleCount; ++i) {
        timestamps[i] = static_cast<qint64>(i);
        values[i] = dist(rng);
    }

    auto segments = M4Decimator::decimateToSegments(
        timestamps.data(), values.data(), sampleCount, dt_px, 0, width, 0);

    size_t totalVertices = 0;
    for (const auto &seg : segments) {
        totalVertices += seg.size();
    }

    // Spec constraint: <= 4 * W_dev vertices per channel
    const size_t maxAllowed = static_cast<size_t>(4 * width);
    QVERIFY2(totalVertices <= maxAllowed,
             qPrintable(QStringLiteral("Decimated vertex count %1 exceeds 4 * W_dev (%2)")
                            .arg(totalVertices).arg(maxAllowed)));
}

void tst_M4Decimator::bucketAggregatesAreIndependentOfWindowStart()
{
    // Note: this test verifies that bucket aggregates are deterministic and independent of window start.
    // The actual widget-level scrolling shimmer test is windowIsQuantisedAndScrollIsShiftInvariant.
    constexpr int sampleCount = 100000;
    const qint64 dt_px = 1000; // 1 us per bucket

    std::vector<qint64> timestamps(sampleCount);
    std::vector<double> values(sampleCount);

    // Stationary sine wave
    for (int i = 0; i < sampleCount; ++i) {
        timestamps[i] = static_cast<qint64>(i) * 10; // 10 ns per sample
        values[i] = std::sin(i * 0.05);
    }

    // Frame 1: kStart = 10, numBuckets = 50
    auto buckets1 = M4Decimator::decimateToBuckets(
        timestamps.data(), values.data(), sampleCount, dt_px, 10, 50);

    // Frame 2: window advanced by sub-bucket steps, kStart = 12, numBuckets = 50
    auto buckets2 = M4Decimator::decimateToBuckets(
        timestamps.data(), values.data(), sampleCount, dt_px, 12, 50);

    // Buckets present in both frames (k = 12 to 59) must be bit-identical
    for (qint64 k = 12; k <= 59; ++k) {
        size_t idx1 = static_cast<size_t>(k - 10);
        size_t idx2 = static_cast<size_t>(k - 12);
        const auto &b1 = buckets1[idx1];
        const auto &b2 = buckets2[idx2];

        QCOMPARE(b1.tFirst, b2.tFirst);
        QCOMPARE(b1.yFirst, b2.yFirst);
        QCOMPARE(b1.tMin, b2.tMin);
        QCOMPARE(b1.yMin, b2.yMin);
        QCOMPARE(b1.tMax, b2.tMax);
        QCOMPARE(b1.yMax, b2.yMax);
        QCOMPARE(b1.tLast, b2.tLast);
        QCOMPARE(b1.yLast, b2.yLast);
        QCOMPARE(b1.count, b2.count);
        QCOMPARE(b1.hasFinite, b2.hasFinite);
        QCOMPARE(b1.hasNonFinite, b2.hasNonFinite);
    }
}

void tst_M4Decimator::incrementalEqualsBatch()
{
    constexpr int numBuckets = 100;
    constexpr int sampleCount = 10000;
    const qint64 dt_px = 500;

    std::vector<qint64> timestamps(sampleCount);
    std::vector<double> values(sampleCount);

    std::mt19937_64 rng(999);
    std::uniform_real_distribution<double> dist(-20.0, 80.0);

    // 10000 samples over dt_px = 500, with 5ns step -> total time 50000 ns -> 100 buckets
    for (int i = 0; i < sampleCount; ++i) {
        timestamps[i] = static_cast<qint64>(i) * 5;
        values[i] = dist(rng);
    }

    // 1. Batch decimation
    qint64 kStart = 0;
    auto batchBuckets = M4Decimator::decimateToBuckets(
        timestamps.data(), values.data(), sampleCount, dt_px, kStart, numBuckets);

    // 2. Incremental streaming
    M4Decimator::IncrementalStream stream(numBuckets);
    for (int i = 0; i < sampleCount; ++i) {
        stream.addSample(timestamps[i], values[i], dt_px);
    }

    // Verify all buckets match batch exactly
    for (int i = 0; i < numBuckets; ++i) {
        qint64 k = kStart + i;
        const M4Bucket *incBucket = stream.bucketAt(k);
        QVERIFY2(incBucket != nullptr, qPrintable(QStringLiteral("Missing bucket %1 in incremental stream").arg(k)));

        const auto &batchBucket = batchBuckets[static_cast<size_t>(i)];

        QCOMPARE(incBucket->tFirst, batchBucket.tFirst);
        QCOMPARE(incBucket->yFirst, batchBucket.yFirst);
        QCOMPARE(incBucket->tMin, batchBucket.tMin);
        QCOMPARE(incBucket->yMin, batchBucket.yMin);
        QCOMPARE(incBucket->tMax, batchBucket.tMax);
        QCOMPARE(incBucket->yMax, batchBucket.yMax);
        QCOMPARE(incBucket->tLast, batchBucket.tLast);
        QCOMPARE(incBucket->yLast, batchBucket.yLast);
        QCOMPARE(incBucket->count, batchBucket.count);
        QCOMPARE(incBucket->hasFinite, batchBucket.hasFinite);
        QCOMPARE(incBucket->hasNonFinite, batchBucket.hasNonFinite);
    }
}

void tst_M4Decimator::propertyTestIncrementalEqualsBatch()
{
    const uint64_t masterSeed = 42ULL;
    std::mt19937_64 rng(masterSeed);
    std::uniform_int_distribution<int> bucketCountDist(10, 150);
    std::uniform_int_distribution<int> sampleCountDist(50, 1500);
    std::uniform_int_distribution<qint64> dtPxDist(20, 200);
    std::uniform_int_distribution<qint64> gapDist(1, 40);
    std::uniform_real_distribution<double> valDist(-100.0, 100.0);
    std::bernoulli_distribution nonFiniteDist(0.04);
    std::bernoulli_distribution infDist(0.5);

    constexpr int totalScenarios = 250;
    int mixedBuckets = 0; // buckets with both finite and non-finite samples, across all scenarios

    for (int scenario = 0; scenario < totalScenarios; ++scenario) {
        uint64_t scenarioSeed = rng();
        std::mt19937_64 scenarioRng(scenarioSeed);

        int numBuckets = bucketCountDist(scenarioRng);
        int sampleCount = sampleCountDist(scenarioRng);
        qint64 dt_px = dtPxDist(scenarioRng);
        qint64 gapThreshold = (scenario % 3 == 0) ? (dt_px * 2) : 0;

        std::vector<qint64> timestamps(sampleCount);
        std::vector<double> values(sampleCount);

        // Include negative timestamps for some scenarios
        qint64 currentT = 0;
        if (scenario % 3 == 0) {
            currentT = -500000LL;
        } else if (scenario % 3 == 1) {
            currentT = 5000000LL;
        }

        for (int i = 0; i < sampleCount; ++i) {
            timestamps[i] = currentT;
            if (nonFiniteDist(scenarioRng)) {
                values[i] = infDist(scenarioRng) ? std::numeric_limits<double>::infinity()
                                                 : std::numeric_limits<double>::quiet_NaN();
            } else {
                values[i] = valDist(scenarioRng);
            }
            qint64 step = gapDist(scenarioRng);
            if (scenario % 5 == 0 && i % 25 == 0) {
                // Gap larger than several buckets
                step += dt_px * 5;
            }
            currentT += step;
        }

        qint64 kFirst = M4Decimator::floorDiv(timestamps.front(), dt_px);
        qint64 kLast = M4Decimator::floorDiv(timestamps.back(), dt_px);

        // Test rings smaller than number of buckets touched, or standard rings
        size_t ringSize = (scenario % 2 == 0)
            ? static_cast<size_t>(std::max(4, numBuckets / 2))
            : static_cast<size_t>(std::max(numBuckets + 4, 8));

        M4Decimator::IncrementalStream stream(numBuckets, ringSize);

        if (scenario % 10 == 0) {
            stream.setWidth(numBuckets / 2, (numBuckets / 2) + 4);
            stream.setWidth(numBuckets, ringSize);
        }

        std::map<size_t, qint64> slotToLastBucket;
        std::vector<qint64> overwrittenBuckets;
        qint64 prevK = std::numeric_limits<qint64>::min();

        for (int i = 0; i < sampleCount; ++i) {
            qint64 k = M4Decimator::floorDiv(timestamps[i], dt_px);
            if (prevK != std::numeric_limits<qint64>::min() && k > prevK) {
                qint64 s = static_cast<qint64>(ringSize);
                qint64 rem = prevK % s;
                if (rem < 0) rem += s;
                size_t slot = static_cast<size_t>(rem);
                auto it = slotToLastBucket.find(slot);
                if (it != slotToLastBucket.end() && it->second != prevK) {
                    overwrittenBuckets.push_back(it->second);
                }
                slotToLastBucket[slot] = prevK;
            }
            prevK = k;
            stream.addSample(timestamps[i], values[i], dt_px);
        }

        // 1. Verify scrolled-past / overwritten buckets and uncommitted slot aliases are never returned
        for (qint64 owK : overwrittenBuckets) {
            const M4Bucket *stale = stream.bucketAt(owK);
            QVERIFY2(stale == nullptr,
                     qPrintable(QString("Overwritten bucket %1 returned in scenario %2 (seed %3)")
                                    .arg(owK).arg(scenario).arg(scenarioSeed)));
        }

        for (const auto &pair : slotToLastBucket) {
            qint64 commK = pair.second;
            qint64 delta = (kLast - commK) + static_cast<qint64>(ringSize);
            qint64 mult = (delta / static_cast<qint64>(ringSize) + 1) * static_cast<qint64>(ringSize);
            qint64 phantomK = commK + mult;
            const M4Bucket *phantom = stream.bucketAt(phantomK);
            QVERIFY2(phantom == nullptr,
                     qPrintable(QString("Phantom bucket %1 mapping to slot %2 returned in scenario %3 (seed %4)")
                                    .arg(phantomK).arg(pair.first).arg(scenario).arg(scenarioSeed)));
        }

        // 2. Active query range
        qint64 kStart = kLast - std::min(numBuckets - 1, static_cast<int>(ringSize) - 2);
        kStart = std::max(kStart, kFirst);
        int activeBuckets = std::min(numBuckets, static_cast<int>(kLast - kStart + 1));

        // Bucket-by-bucket equality against decimateToBuckets
        auto batchBuckets = M4Decimator::decimateToBuckets(
            timestamps.data(), values.data(), sampleCount, dt_px, kStart, activeBuckets);

        for (int i = 0; i < activeBuckets; ++i) {
            qint64 k = kStart + i;
            const M4Bucket *incBucket = stream.bucketAt(k);
            const auto &batchBucket = batchBuckets[static_cast<size_t>(i)];

            if (!batchBucket.hasFinite && !batchBucket.hasNonFinite) {
                if (incBucket) {
                    QCOMPARE(incBucket->count, 0UL);
                }
                continue;
            }

            QVERIFY2(incBucket != nullptr,
                     qPrintable(QString("Missing bucket %1 in scenario %2 (seed %3)")
                                    .arg(k).arg(scenario).arg(scenarioSeed)));

            QCOMPARE(incBucket->count, batchBucket.count);
            QCOMPARE(incBucket->hasFinite, batchBucket.hasFinite);
            QCOMPARE(incBucket->hasNonFinite, batchBucket.hasNonFinite);

            if (batchBucket.hasFinite) {
                QCOMPARE(incBucket->tFirst, batchBucket.tFirst);
                QCOMPARE(incBucket->yFirst, batchBucket.yFirst);
                QCOMPARE(incBucket->tMin, batchBucket.tMin);
                QCOMPARE(incBucket->yMin, batchBucket.yMin);
                QCOMPARE(incBucket->tMax, batchBucket.tMax);
                QCOMPARE(incBucket->yMax, batchBucket.yMax);
                QCOMPARE(incBucket->tLast, batchBucket.tLast);
                QCOMPARE(incBucket->yLast, batchBucket.yLast);
            }
        }

        // 3. Segment extraction equality against decimateToSegments
        auto batchSegments = M4Decimator::decimateToSegments(
            timestamps.data(), values.data(), sampleCount, dt_px, kStart, activeBuckets, gapThreshold);
        auto streamSegments = stream.extractVisibleSegments(kStart, activeBuckets, gapThreshold);

        QCOMPARE(streamSegments.size(), batchSegments.size());
        for (size_t s = 0; s < streamSegments.size(); ++s) {
            const auto &streamSeg = streamSegments[s];
            const auto &batchSeg = batchSegments[s];
            QCOMPARE(streamSeg.size(), batchSeg.size());
            for (size_t v = 0; v < streamSeg.size(); ++v) {
                QCOMPARE(streamSeg[v].t, batchSeg[v].t);
                if (std::isnan(streamSeg[v].value) && std::isnan(batchSeg[v].value)) {
                    continue;
                }
                QCOMPARE(streamSeg[v].value, batchSeg[v].value);
            }
        }

        // 4. Blank buckets: a bucket with any non-finite sample is never drawn, identically in batch
        //    and incremental extraction; every finite-only bucket is drawn; no segment spans a blank bucket.
        std::set<qint64> blank;
        std::set<qint64> finiteOnly;
        for (int i = 0; i < activeBuckets; ++i) {
            const auto &bb = batchBuckets[static_cast<size_t>(i)];
            if (bb.hasNonFinite) {
                blank.insert(kStart + i);
                if (bb.hasFinite) {
                    ++mixedBuckets;
                }
            } else if (bb.hasFinite) {
                finiteOnly.insert(kStart + i);
            }
        }
        auto drawnBuckets = [&](const std::vector<std::vector<M4Point>> &segs) {
            std::set<qint64> drawn;
            for (const auto &seg : segs) {
                for (const auto &pt : seg) {
                    drawn.insert(M4Decimator::floorDiv(pt.t, dt_px));
                }
            }
            return drawn;
        };
        const std::set<qint64> drawnBatch = drawnBuckets(batchSegments);
        const std::set<qint64> drawnStream = drawnBuckets(streamSegments);
        const QString where = QString("scenario %1 (seed %2)").arg(scenario).arg(scenarioSeed);
        QVERIFY2(drawnStream == drawnBatch, qPrintable("incremental and batch draw different buckets in " + where));
        QVERIFY2(drawnBatch == finiteOnly, qPrintable("drawn buckets differ from finite-only buckets in " + where));
        for (const auto &seg : batchSegments) {
            const qint64 lo = M4Decimator::floorDiv(seg.front().t, dt_px);
            const qint64 hi = M4Decimator::floorDiv(seg.back().t, dt_px);
            auto it = blank.lower_bound(lo);
            QVERIFY2(it == blank.end() || *it > hi, qPrintable("segment spans a blank bucket in " + where));
        }
    }

    // The scenarios must actually exercise buckets that mix finite and non-finite samples
    QVERIFY2(mixedBuckets > 0, "no mixed finite/non-finite bucket generated");
}

void tst_M4Decimator::mulDivExact()
{
    // Reference values computed with arbitrary-precision integers
    struct Ref {
        qint64 a, b, c, floor, rem, ceil;
    };
    const Ref refs[] = {
        {1000000000000000000LL, 16384LL, 10000000007LL, 1638399998853LL, 1200008029LL, 1638399998854LL},
        {-1000000000000000000LL, 16384LL, 10000000007LL, -1638399998854LL, 8799991978LL, -1638399998853LL},
        {1000000000000000000LL, 16384LL, 999999937LL, 16384001032192LL, 65028096LL, 16384001032193LL},
        {999999999999999999LL, 16383LL, 4611686018427387909LL, 3552LL, 2291262545918130849LL, 3553LL},
        {-999999999999999997LL, 16384LL, 9223372036854775807LL, -1777LL, 5932109490936658191LL, -1776LL},
        {123456789012345678LL, 9999LL, 1000003LL, 1234440730012254LL, 397560LL, 1234440730012255LL},
        {-1LL, 16384LL, 1000000000000000000LL, -1LL, 999999999999983616LL, 0LL},
        {7, 3, 2, 10, 1, 11},
        {-7, 3, 2, -11, 1, -10},
        {0, 16384, 7, 0, 0, 0},
    };
    for (const Ref &r : refs) {
        qint64 rem = -1;
        QCOMPARE(mulDivFloor(r.a, r.b, r.c, &rem), r.floor);
        QCOMPARE(rem, r.rem);
        QCOMPARE(mulDivCeil(r.a, r.b, r.c), r.ceil);
    }

    // Saturation when the exact result does not fit in qint64
    QCOMPARE(mulDivFloor(std::numeric_limits<qint64>::max(), 2, 1), std::numeric_limits<qint64>::max());
    QCOMPARE(mulDivFloor(-999999999999999989LL, 8191, 3), std::numeric_limits<qint64>::min());

#if defined(__SIZEOF_INT128__)
    // Randomised comparison against the compiler's 128-bit integers where available
    std::mt19937_64 rng(20261011);
    std::uniform_int_distribution<qint64> aDist(-1000000000000000000LL, 1000000000000000000LL);
    std::uniform_int_distribution<qint64> bDist(1, 16384);
    std::uniform_int_distribution<qint64> cDist(1, std::numeric_limits<qint64>::max());
    std::uniform_int_distribution<qint64> cSmallDist(1, 100000);
    for (int i = 0; i < 200000; ++i) {
        const qint64 a = aDist(rng);
        const qint64 b = bDist(rng);
        const qint64 c = (i % 2 == 0) ? cDist(rng) : cSmallDist(rng);
        __extension__ typedef __int128 i128;
        const i128 p = static_cast<i128>(a) * b;
        i128 f = p / c;
        if ((p % c) != 0 && p < 0) --f;
        if (f > std::numeric_limits<qint64>::max() || f < std::numeric_limits<qint64>::min()) {
            continue; // saturation is checked above
        }
        qint64 rem = -1;
        const qint64 got = mulDivFloor(a, b, c, &rem);
        QVERIFY2(got == static_cast<qint64>(f), qPrintable(QString("floor(%1*%2/%3)").arg(a).arg(b).arg(c)));
        QCOMPARE(rem, static_cast<qint64>(p - f * c));
    }
#endif
}

void tst_M4Decimator::bucketGridBoundaries()
{
    // boundary(k) is the first position of bucket k, for non-uniform grids and large positions
    std::mt19937_64 rng(12);
    std::uniform_int_distribution<qint64> numDist(1, 16384);
    std::uniform_int_distribution<qint64> extraDist(0, 1000000000000LL);
    std::uniform_int_distribution<qint64> xDist(-1000000000000000000LL, 1000000000000000000LL);
    for (int i = 0; i < 20000; ++i) {
        const qint64 num = numDist(rng);
        const qint64 den = num + extraDist(rng); // den >= num: at most one bucket per axis unit
        const BucketGrid grid(num, den);
        const qint64 x = xDist(rng);
        const qint64 k = grid.bucketOf(x);
        QVERIFY2(grid.boundary(k) <= x && x < grid.boundary(k + 1),
                 qPrintable(QString("x %1 not in bucket %2 of grid %3/%4").arg(x).arg(k).arg(num).arg(den)));
        QCOMPARE(grid.bucketOf(grid.boundary(k)), k);
        QCOMPARE(grid.bucketOf(grid.boundary(k) - 1), k - 1);
    }
    // A uniform grid matches floorDiv
    const BucketGrid uniform(1, 1000);
    for (qint64 x : {-2001LL, -1000LL, -1LL, 0LL, 999LL, 1000LL, 123456789LL}) {
        QCOMPARE(uniform.bucketOf(x), M4Decimator::floorDiv(x, 1000));
    }
}

void tst_M4Decimator::bucketGridMoreBucketsThanUnits()
{
    // num > den: more buckets than axis units, as with a SampleIndex capacity (>= 10 samples)
    // below the plot width (<= 16384 device px) in DecimationMode::Always. Bucket assignment must
    // stay exact over the whole valid range |x| <= maxPosition().
    std::mt19937_64 rng(1515);
    std::uniform_int_distribution<qint64> numDist(11, 16384);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (int i = 0; i < 20000; ++i) {
        const qint64 num = numDist(rng);
        const qint64 den = 10 + static_cast<qint64>(unit(rng) * static_cast<double>(num - 10)); // 10 <= den < num
        const BucketGrid grid(num, den);
        const qint64 limit = grid.maxPosition();
        QVERIFY(limit > 0);
        // Positions across the range, including both ends of it
        qint64 x = 0;
        switch (i % 4) {
            case 0: x = limit; break;
            case 1: x = -limit; break;
            case 2: x = limit - static_cast<qint64>(unit(rng) * 1e6); break;
            default: x = static_cast<qint64>((unit(rng) * 2.0 - 1.0) * static_cast<double>(limit)); break;
        }
        const qint64 k = grid.bucketOf(x);
        // At the top of the range k can be INT64_MAX, whose next boundary does not exist
        const bool lastBucket = (k == std::numeric_limits<qint64>::max());
        QVERIFY2(grid.boundary(k) <= x && (lastBucket || x < grid.boundary(k + 1)),
                 qPrintable(QString("x %1 not in bucket %2 of grid %3/%4 (limit %5)").arg(x).arg(k).arg(num).arg(den).arg(limit)));
#if defined(__SIZEOF_INT128__)
        __extension__ typedef __int128 i128;
        const i128 p = static_cast<i128>(x) * num;
        i128 f = p / den;
        if ((p % den) != 0 && p < 0) --f;
        QVERIFY2(f == k, qPrintable(QString("floor(%1*%2/%3) != %4").arg(x).arg(num).arg(den).arg(k)));
#endif
    }
    // maxPosition() is exact: one more position would not fit
    const BucketGrid g(16384, 10);
    QCOMPARE(g.maxPosition(), 5629499534213119LL); // floor((2^63 - 1) * 10 / 16384)
    QCOMPARE(g.bucketOf(g.maxPosition()), 9223372036854774169LL); // floor(5629499534213119 * 16384 / 10)
    // and is the whole qint64 range when num <= den
    QCOMPARE(BucketGrid(1000, 1000000000).maxPosition(), std::numeric_limits<qint64>::max());
}

void tst_M4Decimator::mulDivSaturationEdges()
{
    // Results exactly at INT64_MIN / INT64_MAX and one beyond or inside them, on both sides.
    // The intermediate product q * b of mulDivFloor() can leave the qint64 range while the final
    // result still fits; only results outside the range saturate.
    struct Edge {
        qint64 a, b, c, expected; // expected = exact floor(a * b / c), saturated to the qint64 range
    };
    const Edge edges[] = {
        {-9223362813491962317LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::min()}, // exact INT64_MIN - 1: saturates
        {-9223362813491962316LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::min()}, // exact INT64_MIN
        {-9223362813491962315LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::min() + 1}, // exact INT64_MIN + 1
        {9223362813491962315LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::max() - 1}, // exact INT64_MAX - 1
        {9223362813491962316LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::max()},     // exact INT64_MAX
        {9223362813491962317LL, 1000001LL, 1000000LL, std::numeric_limits<qint64>::max()},     // exact INT64_MAX + 1: saturates
    };
    for (const Edge &e : edges) {
        QVERIFY2(mulDivFloor(e.a, e.b, e.c) == e.expected,
                 qPrintable(QString("floor(%1 * %2 / %3) = %4, expected %5")
                                .arg(e.a).arg(e.b).arg(e.c).arg(mulDivFloor(e.a, e.b, e.c)).arg(e.expected)));
    }
    // The first case found by bucketGridMoreBucketsThanUnits: x = -maxPosition() of grid 15079/7023
    QCOMPARE(mulDivFloor(-4295758459767298261LL, 15079, 7023), -9223372036854775805LL);
    QCOMPARE(mulDivFloor(4295758459767298261LL, 15079, 7023), 9223372036854775804LL);
}

QTEST_MAIN(tst_M4Decimator)
#include "tst_M4Decimator.moc"
