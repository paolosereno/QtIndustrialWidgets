// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include "internal/LevelMeterLayout.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets::internal;

namespace {

// Centres of the ticks of [minimum, maximum] on a scale `length` px long
std::vector<double> centresOf(const std::vector<double> &ticks, double minimum, double maximum, double length)
{
    std::vector<double> out;
    for (double t : ticks) out.push_back((t - minimum) / (maximum - minimum) * length);
    return out;
}

bool allPairsKeepGap(const std::vector<int> &drawn, const std::vector<double> &centres, const std::vector<double> &extents, double gap)
{
    for (size_t a = 0; a < drawn.size(); ++a) {
        for (size_t b = a + 1; b < drawn.size(); ++b) {
            const size_t i = static_cast<size_t>(drawn[a]);
            const size_t j = static_cast<size_t>(drawn[b]);
            if (std::abs(centres[i] - centres[j]) - 0.5 * (extents[i] + extents[j]) < gap) return false;
        }
    }
    return true;
}

std::vector<double> valuesOf(const std::vector<double> &ticks, const std::vector<int> &drawn)
{
    std::vector<double> out;
    for (int i : drawn) out.push_back(ticks[static_cast<size_t>(i)]);
    return out;
}

} // namespace

class tst_LevelMeterLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dbLadderPriority_data();
    void dbLadderPriority();
    void dbLadderAnyRange();
    void endsWinOverZero();
    void priorityDecidesBetweenClashingLabels();
    void endsClashKeepsFirst();
    void linearStepIsSmallestDivisor_data();
    void linearStepIsSmallestDivisor();
    void degenerateInput();
    void horizontalMinimumKeepsEveryLinearLabel();
    void horizontalMinimumKeepsBothDbEnds();
    void verticalMinimumKeepsEveryLinearLabel();
    void verticalUnitBandOnlyWithUnit();
};

void tst_LevelMeterLayout::dbLadderPriority_data()
{
    QTest::addColumn<double>("length");
    QTest::addColumn<std::vector<double>>("expected");
    // -60..+6 dB, every label 10 px along the scale, gap 4 px
    QTest::addRow("400 px: all") << 400.0 << std::vector<double>{-60, -40, -30, -20, -12, -6, -3, 0, 3, 6};
    QTest::addRow("200 px") << 200.0 << std::vector<double>{-60, -40, -30, -20, -12, -6, 0, 6};
    QTest::addRow("100 px") << 100.0 << std::vector<double>{-60, -40, -30, -20, -6, 6};
    QTest::addRow("26 px: ends") << 26.0 << std::vector<double>{-60, 6};
}

// #57: dB labels are taken in priority order (ends, 0 dB, -20, -6, -40, -12, -30, -3, +3, others
// increasing), each only if it keeps the gap from every label already taken
void tst_LevelMeterLayout::dbLadderPriority()
{
    QFETCH(double, length);
    QFETCH(std::vector<double>, expected);
    const std::vector<double> ticks = levelMeterTicks(-60.0, 6.0, QStringLiteral("dB"));
    const std::vector<double> centres = centresOf(ticks, -60.0, 6.0, length);
    const std::vector<double> extents(ticks.size(), 10.0);
    const std::vector<int> drawn = levelMeterSelectLabels(ticks, centres, extents, false, 4.0);
    QCOMPARE(valuesOf(ticks, drawn), expected);
    QVERIFY(allPairsKeepGap(drawn, centres, extents, 4.0));
    // Greedy in priority order: every label left out clashes with one taken before it
    const std::vector<double> priority{-60, 6, 0, -20, -6, -40, -12, -30, -3, 3};
    for (size_t k = 0; k < priority.size(); ++k) {
        const auto it = std::find(ticks.begin(), ticks.end(), priority[k]);
        const int i = static_cast<int>(it - ticks.begin());
        if (std::find(drawn.begin(), drawn.end(), i) != drawn.end()) continue;
        bool clashes = false;
        for (size_t e = 0; e < k; ++e) {
            const int j = static_cast<int>(std::find(ticks.begin(), ticks.end(), priority[e]) - ticks.begin());
            if (std::find(drawn.begin(), drawn.end(), j) != drawn.end()
                && std::abs(centres[static_cast<size_t>(i)] - centres[static_cast<size_t>(j)]) - 10.0 < 4.0) clashes = true;
        }
        QVERIFY2(clashes, qPrintable(QStringLiteral("%1 dB left out without a clash").arg(priority[k])));
    }
}

// #57: any dB range: ticks outside the range are skipped, the ends of the ticks in range always drawn,
// ticks not in the priority list come last in increasing order
void tst_LevelMeterLayout::dbLadderAnyRange()
{
    // -50..+10: ticks -40..+6; ends -40 and +6
    const std::vector<double> ticks = levelMeterTicks(-50.0, 10.0, QStringLiteral("dB"));
    QCOMPARE(ticks, (std::vector<double>{-40, -30, -20, -12, -6, -3, 0, 3, 6}));
    const std::vector<double> centres = centresOf(ticks, -50.0, 10.0, 60.0);
    const std::vector<int> drawn = levelMeterSelectLabels(ticks, centres, std::vector<double>(ticks.size(), 10.0), false, 4.0);
    QCOMPARE(drawn.front(), 0);
    QCOMPARE(drawn.back(), static_cast<int>(ticks.size()) - 1);

    // -70..+5: -60 is an end; +6 is out of range, +3 is the other end
    const std::vector<double> t2 = levelMeterTicks(-70.0, 5.0, QStringLiteral("dB"));
    QCOMPARE(t2.front(), -60.0);
    QCOMPARE(t2.back(), 3.0);

    // The ticks not in the priority list are -60 and +6: when in the range they are always its ends,
    // so the "others in increasing order" tail is not reachable with the current ladder
    for (double lo : {-70.0, -61.0, -50.0}) {
        for (double hi : {4.0, 7.0, 12.0}) {
            const std::vector<double> t = levelMeterTicks(lo, hi, QStringLiteral("dB"));
            for (size_t i = 1; i + 1 < t.size(); ++i) {
                QVERIFY(t[i] != -60.0 && t[i] != 6.0);
            }
        }
    }
}

// #57: when 0 dB clashes with an end, the end wins
void tst_LevelMeterLayout::endsWinOverZero()
{
    const std::vector<double> ticks = levelMeterTicks(-60.0, 6.0, QStringLiteral("dB"));
    const std::vector<double> centres = centresOf(ticks, -60.0, 6.0, 80.0); // 0 and +6 are 7.3 px apart
    const std::vector<int> drawn = levelMeterSelectLabels(ticks, centres, std::vector<double>(ticks.size(), 10.0), false, 4.0);
    const std::vector<double> values = valuesOf(ticks, drawn);
    QVERIFY(std::find(values.begin(), values.end(), 6.0) != values.end());
    QVERIFY(std::find(values.begin(), values.end(), 0.0) == values.end());
}

// #57: when two labels clash, the one earlier in the priority list is drawn: 0 dB over -3 dB and
// -20 dB over -40 dB (both later in increasing order), every other label far from the rest
void tst_LevelMeterLayout::priorityDecidesBetweenClashingLabels()
{
    const std::vector<double> ticks = levelMeterTicks(-60.0, 6.0, QStringLiteral("dB"));
    QCOMPARE(ticks, (std::vector<double>{-60, -40, -30, -20, -12, -6, -3, 0, 3, 6}));
    //                              -60  -40    -30  -20    -12  -6   -3   0    +3   +6
    const std::vector<double> centres{0, 105, 200, 110, 300, 400, 500, 505, 600, 700};
    const std::vector<int> drawn = levelMeterSelectLabels(ticks, centres, std::vector<double>(ticks.size(), 10.0), false, 4.0);
    QCOMPARE(valuesOf(ticks, drawn), (std::vector<double>{-60, -30, -20, -12, -6, 0, 3, 6}));
}

// #57: if the two ends clash, only the first is drawn (linear and dB)
void tst_LevelMeterLayout::endsClashKeepsFirst()
{
    const std::vector<double> db = levelMeterTicks(-60.0, 6.0, QStringLiteral("dB"));
    QCOMPARE(levelMeterSelectLabels(db, centresOf(db, -60.0, 6.0, 12.0), std::vector<double>(db.size(), 10.0), false, 4.0),
             (std::vector<int>{0}));
    const std::vector<double> lin = levelMeterTicks(0.0, 100.0, QString());
    QCOMPARE(levelMeterSelectLabels(lin, centresOf(lin, 0.0, 100.0, 12.0), std::vector<double>(lin.size(), 10.0), true, 4.0),
             (std::vector<int>{0}));
}

void tst_LevelMeterLayout::linearStepIsSmallestDivisor_data()
{
    QTest::addColumn<double>("length");
    QTest::addColumn<std::vector<int>>("expected");
    // 7 labels 10 px along the scale, gap 4 px: k labels fit when k x length / 6 - 10 >= 4
    QTest::addRow("85 px: every label") << 85.0 << std::vector<int>{0, 1, 2, 3, 4, 5, 6};
    QTest::addRow("83 px: k = 2") << 83.0 << std::vector<int>{0, 2, 4, 6};
    QTest::addRow("41 px: k = 3") << 41.0 << std::vector<int>{0, 3, 6};
    QTest::addRow("20 px: k = 6") << 20.0 << std::vector<int>{0, 6};
}

// #57: linear scale: every k-th label, k the smallest divisor of 6 that keeps the gap
void tst_LevelMeterLayout::linearStepIsSmallestDivisor()
{
    QFETCH(double, length);
    QFETCH(std::vector<int>, expected);
    const std::vector<double> ticks = levelMeterTicks(0.0, 100.0, QString());
    QCOMPARE(levelMeterSelectLabels(ticks, centresOf(ticks, 0.0, 100.0, length), std::vector<double>(ticks.size(), 10.0), true, 4.0),
             expected);
}

void tst_LevelMeterLayout::degenerateInput()
{
    QVERIFY(levelMeterSelectLabels({}, {}, {}, true, 4.0).empty());
    QCOMPARE(levelMeterSelectLabels({1.0}, {0.0}, {10.0}, false, 4.0), (std::vector<int>{0}));
    // Non-finite gap: treated as 0 (touching labels all drawn)
    QCOMPARE(levelMeterSelectLabels({0, 1, 2}, {0, 10, 20}, {10, 10, 10}, true, std::numeric_limits<double>::quiet_NaN()),
             (std::vector<int>{0, 1, 2}));
    // A non-finite centre clashes with every label: only the ends remain
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(levelMeterSelectLabels({-60, -20, 6}, {0, nan, 100}, {10, 10, 10}, false, 4.0), (std::vector<int>{0, 2}));
}

// #57 (a): the horizontal minimum keeps scaleLabelMinGap() between linear labels, so the selection
// draws all of them at the minimum width
// (100000-700000: every label as wide as the widest, so the gap at the minimum is exactly minGap)
void tst_LevelMeterLayout::horizontalMinimumKeepsEveryLinearLabel()
{
    for (const auto &range : {std::pair<double, double>{0.0, 100000.0}, std::pair<double, double>{100000.0, 700000.0}}) {
        for (double pt : {9.0, 11.0, 13.0}) {
            QFont font;
            font.setPointSizeF(pt);
            const LevelMeterScaleMetrics m = levelMeterScaleMetrics(range.first, range.second, QString(), font);
            const QSize min = levelMeterMinimumSize(Qt::Horizontal, 1, true, false, false, m, QSize(100, 45));
            const LevelMeterInsets insets = levelMeterHorizontalInsets(m, 0.0, 6.0);
            const double length = min.width() - 22.0 - insets.left - insets.right;
            const std::vector<double> ticks = levelMeterTicks(range.first, range.second, QString());
            const std::vector<double> extents = levelMeterLabelExtents(Qt::Horizontal, range.first, range.second, QString(), font);
            QCOMPARE(levelMeterSelectLabels(ticks, centresOf(ticks, range.first, range.second, length), extents, true, m.minGap).size(),
                     ticks.size());
        }
    }
}

// #57: at the minimum width of a horizontal dB meter both end labels are drawn, also with wide labels
// (letter spacing 200 % and 300 %, 16-24 pt) and with a unit taking room at the right end of the bars
void tst_LevelMeterLayout::horizontalMinimumKeepsBothDbEnds()
{
    for (double spacing : {100.0, 200.0, 300.0}) {
        for (double pt : {9.0, 11.0, 13.0, 16.0, 20.0, 24.0}) {
            QFont font;
            font.setPointSizeF(pt);
            font.setLetterSpacing(QFont::PercentageSpacing, spacing);
            const QString unit = QStringLiteral("dB");
            const LevelMeterScaleMetrics m = levelMeterScaleMetrics(-60.0, 6.0, unit, font);
            const QSize min = levelMeterMinimumSize(Qt::Horizontal, 1, true, false, false, m, QSize(100, 45));
            const LevelMeterInsets insets = levelMeterHorizontalInsets(m, 0.0, 6.0);
            const double length = min.width() - 22.0 - insets.left - insets.right;
            const std::vector<double> ticks = levelMeterTicks(-60.0, 6.0, unit);
            const std::vector<int> drawn = levelMeterSelectLabels(ticks, centresOf(ticks, -60.0, 6.0, length),
                                                                  levelMeterLabelExtents(Qt::Horizontal, -60.0, 6.0, unit, font), false, m.minGap);
            QVERIFY2(!drawn.empty() && drawn.front() == 0 && drawn.back() == static_cast<int>(ticks.size()) - 1,
                     qPrintable(QStringLiteral("%1 pt, spacing %2 %: minimum width %3 (bars %4 px) does not hold both end labels")
                                    .arg(pt).arg(spacing).arg(min.width()).arg(length)));
        }
    }
}

// #51 (from #57): at the minimum height of a vertical meter every linear label is drawn, and both dB
// ends; with and without a unit (the unit band takes height above the bars)
void tst_LevelMeterLayout::verticalMinimumKeepsEveryLinearLabel()
{
    struct Range { double min; double max; QString unit; bool linear; };
    const Range ranges[] = {{0.0, 100.0, QString(), true}, {0.0, 100000.0, QStringLiteral("RPM"), true},
                            {-60.0, 6.0, QStringLiteral("dB"), false}};
    for (const Range &r : ranges) {
        for (double pt : {9.0, 11.0, 13.0, 16.0, 20.0}) {
            QFont font;
            font.setPointSizeF(pt);
            const LevelMeterScaleMetrics m = levelMeterScaleMetrics(r.min, r.max, r.unit, font);
            const QSize min = levelMeterMinimumSize(Qt::Vertical, 1, true, false, false, m, QSize(45, 100));
            const double length = min.height() - 22.0 - levelMeterVerticalUnitBand(m);
            const std::vector<double> ticks = levelMeterTicks(r.min, r.max, r.unit);
            const std::vector<int> drawn = levelMeterSelectLabels(ticks, centresOf(ticks, r.min, r.max, length),
                                                                  levelMeterLabelExtents(Qt::Vertical, r.min, r.max, r.unit, font), r.linear, m.minGap);
            const bool ok = r.linear ? drawn.size() == ticks.size()
                                     : (!drawn.empty() && drawn.front() == 0 && drawn.back() == static_cast<int>(ticks.size()) - 1);
            QVERIFY2(ok, qPrintable(QStringLiteral("%1..%2 '%3' %4 pt: minimum height %5 (bars %6 px) draws %7 of %8 labels")
                                        .arg(r.min).arg(r.max).arg(r.unit).arg(pt).arg(min.height()).arg(length)
                                        .arg(drawn.size()).arg(ticks.size())));
        }
    }
}

// #57: the unit band of a vertical meter exists only with a unit
void tst_LevelMeterLayout::verticalUnitBandOnlyWithUnit()
{
    const QFont font;
    QCOMPARE(levelMeterVerticalUnitBand(levelMeterScaleMetrics(-60.0, 6.0, QString(), font)), 0.0);
    const LevelMeterScaleMetrics m = levelMeterScaleMetrics(-60.0, 6.0, QStringLiteral("dB"), font);
    QVERIFY(levelMeterVerticalUnitBand(m) >= 1.5 * m.lineHeight + m.minGap);
    QCOMPARE(levelMeterVerticalScaleMargin(levelMeterScaleMetrics(-60.0, 6.0, QString(), font)), 36.0);
}

QTEST_MAIN(tst_LevelMeterLayout)
#include "tst_LevelMeterLayout.moc"
