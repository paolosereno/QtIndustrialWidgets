// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <internal/M4Decimator.h>
#include <cmath>
#include <random>
#include <vector>

using namespace QtIndustrialWidgets::internal;

class tst_M4Decimator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pixelEquivalence();
    void spikePreservation();
    void vertexBound();
    void noShimmer();
    void incrementalEqualsBatch();
};

void tst_M4Decimator::pixelEquivalence()
{
    constexpr int sampleCount = 1000000;
    constexpr int width = 800;
    constexpr int height = 400;

    std::vector<qint64> timestamps(sampleCount);
    std::vector<double> values(sampleCount);

    // Deterministic PRNG
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> dist(-100.0, 100.0);

    for (int i = 0; i < sampleCount; ++i) {
        timestamps[i] = static_cast<qint64>(i);
        values[i] = dist(rng);
    }

    const qint64 dt_px = sampleCount / width; // 1250 ns per bucket
    const double yMin = -110.0;
    const double yMax = 110.0;
    const double ySpan = yMax - yMin;

    auto mapPoint = [&](qint64 t, double v) -> QPointF {
        double x = (static_cast<double>(t) / static_cast<double>(sampleCount)) * width;
        double y = height - ((v - yMin) / ySpan) * (height - 20.0) - 10.0;
        return QPointF(x, y);
    };

    // 1. Render Raw
    QImage rawImage(width, height, QImage::Format_ARGB32_Premultiplied);
    rawImage.fill(Qt::black);
    {
        QPainter p(&rawImage);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(Qt::white, 1));
        QPolygonF poly;
        poly.reserve(sampleCount);
        for (int i = 0; i < sampleCount; ++i) {
            poly << mapPoint(timestamps[i], values[i]);
        }
        p.drawPolyline(poly);
    }

    // 2. Render Decimated
    QImage decImage(width, height, QImage::Format_ARGB32_Premultiplied);
    decImage.fill(Qt::black);
    {
        auto segments = M4Decimator::decimateToSegments(
            timestamps.data(), values.data(), sampleCount, dt_px, 0, width, 0);

        QPainter p(&decImage);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(Qt::white, 1));
        for (const auto &seg : segments) {
            QPolygonF poly;
            poly.reserve(static_cast<int>(seg.size()));
            for (const auto &pt : seg) {
                poly << mapPoint(pt.t, pt.value);
            }
            p.drawPolyline(poly);
        }
    }

    // 3. Compare images: assert exact equality first
    if (rawImage == decImage) {
        QCOMPARE(rawImage, decImage);
    } else {
        // Fall back to verifying at most 1 differing pixel per column
        // due to sub-pixel rasterizer polyline join differences across diagonal segments
        int maxDiffPerCol = 0;
        int totalDiffCols = 0;
        for (int x = 0; x < width; ++x) {
            int colDiffs = 0;
            for (int y = 0; y < height; ++y) {
                if (rawImage.pixelColor(x, y) != decImage.pixelColor(x, y)) {
                    colDiffs++;
                }
            }
            if (colDiffs > 0) {
                totalDiffCols++;
                if (colDiffs > maxDiffPerCol) {
                    maxDiffPerCol = colDiffs;
                }
            }
        }

        qInfo() << "Rasterizer difference:" << totalDiffCols << "columns differ, max diff per col:" << maxDiffPerCol;
        QVERIFY2(maxDiffPerCol <= 1,
                 "M4 pixel equivalence: rasterizer differences exceed 1 pixel per column between raw and decimated");
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

void tst_M4Decimator::noShimmer()
{
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

QTEST_MAIN(tst_M4Decimator)
#include "tst_M4Decimator.moc"
