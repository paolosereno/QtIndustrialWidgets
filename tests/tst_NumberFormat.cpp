// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "ScaleLabelCases.h"
#include <QtTest/QtTest>

#include "internal/NumberFormat.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

class tst_NumberFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void formatFixedCases_data();
    void formatFixedCases();
    void formatFixedMatchesQStringNumberAwayFromZero();
    void scaleLabelTextsCases_data();
    void scaleLabelTextsCases();
    void scaleLabelTextsEdgeCases();
};

void tst_NumberFormat::formatFixedCases_data()
{
    QTest::addColumn<double>("value");
    QTest::addColumn<int>("decimals");
    QTest::addColumn<QString>("expected");

    // Cases of #46
    QTest::addRow("-0.0157, 1") << -0.0157 << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.0, 1") << -0.0 << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.04, 1") << -0.04 << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.4, 0") << -0.4 << 0 << QStringLiteral("0");
    QTest::addRow("-0.05, 1 (rounding boundary)") << -0.05 << 1 << QStringLiteral("-0.1");
    QTest::addRow("0.04, 1") << 0.04 << 1 << QStringLiteral("0.0");
    QTest::addRow("-1.24, 1") << -1.24 << 1 << QStringLiteral("-1.2");
    QTest::addRow("123.456, 2") << 123.456 << 2 << QStringLiteral("123.46");
    // Further negative zeros
    QTest::addRow("-0.0333, 1 (Y-axis tick)") << (-0.1 + 2.0 / 6.0 * 0.2) << 1 << QStringLiteral("0.0");
    QTest::addRow("-1.4e-17, 1 (rounding error at zero)") << -1.3877787807814457e-17 << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.004, 2") << -0.004 << 2 << QStringLiteral("0.00");
    QTest::addRow("-0.3, 0") << -0.3 << 0 << QStringLiteral("0");
    QTest::addRow("-0.0, 0") << -0.0 << 0 << QStringLiteral("0");
    // Not zero: unchanged
    QTest::addRow("-0.6, 0") << -0.6 << 0 << QStringLiteral("-1");
    QTest::addRow("-1000000, 0") << -1000000.0 << 0 << QStringLiteral("-1000000");
    QTest::addRow("0, 3") << 0.0 << 3 << QStringLiteral("0.000");
}

void tst_NumberFormat::formatFixedCases()
{
    QFETCH(double, value);
    QFETCH(int, decimals);
    QFETCH(QString, expected);
    QCOMPARE(formatFixed(value, decimals), expected);
}

// Everything that does not show zero, and non-finite values, give the text of QString::number()
void tst_NumberFormat::formatFixedMatchesQStringNumberAwayFromZero()
{
    const double values[] = {-1234.5678, -10.0, -1.25, -0.06, -0.05, 0.05, 0.5, 1.0, 99.95, 8000.0, 1e9,
                             std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()};
    for (double v : values) {
        for (int d = 0; d <= 3; ++d) {
            const QString reference = QString::number(v, 'f', d);
            if (std::isfinite(v) && std::abs(v) < 0.5 * std::pow(10.0, -d)) {
                continue; // shows zero
            }
            QCOMPARE(formatFixed(v, d), reference);
        }
    }
}

void tst_NumberFormat::scaleLabelTextsCases_data()
{
    // The values of the scales of the widgets: minimum + i / ticks * (maximum - minimum)
    ScaleLabelCases::addRows();
    const auto row = [](const char *name, double minimum, double maximum, int ticks, int precision, const char *labels) {
        QTest::newRow(name) << minimum << maximum << ticks << precision
                            << QString::fromLatin1(labels).split(QLatin1Char(' '));
    };
    // Rule 4: no decimals up to 6 are exact for the step 0.1000001 (7 are): the first that separates the labels
    row("step 0.1000001, precision 0: distinct at 1, not exact at 7", 0.0, 0.4000004, 4, 0, "0.0 0.1 0.2 0.3 0.4");
    // Rule 5: 12 decimals do not separate the labels of a range of 1e-13: they stay repeated
    row("range 1e-13: stop at 12", 0.0, 1e-13, 4, 0,
        "0.000000000000 0.000000000000 0.000000000000 0.000000000000 0.000000000000");
    // Rule 3 at 6 decimals: the first exact above the precision
    row("step 0.000025, precision 2: exact at 6", 0.0, 0.0001, 4, 2,
        "0.000000 0.000025 0.000050 0.000075 0.000100");
    // No negative zero: the middle tick of -0.1..0.1 is -1.4e-17, not 0
    row("-0.1-0.1, 4, 2: no negative zero", -0.1, 0.1, 4, 2, "-0.10 -0.05 0.00 0.05 0.10");
    row("-0.1-0.1, 5, 1: repeated at 1, exact at 2", -0.1, 0.1, 5, 1, "-0.10 -0.06 -0.02 0.02 0.06 0.10");
    // Whole values: no decimals, whatever the precision
    row("-1000-1000, 4, 3: whole", -1000.0, 1000.0, 4, 3, "-1000 -500 0 500 1000");
    row("0-8000, 8, 0", 0.0, 8000.0, 8, 0, "0 1000 2000 3000 4000 5000 6000 7000 8000");
    // Rule 1 with its tolerance: at 10 decimals the thirds are within 1e-9 of the step (precision 20 counts as 12)
    row("thirds, precision 20: exact within the tolerance at 10", 0.0, 1.0, 3, 20,
        "0.0000000000 0.3333333333 0.6666666667 1.0000000000");
    // Ties round away from zero, as QString::number()
    row("0-0.5, 4, 2: 0.125 is 0.13", 0.0, 0.5, 4, 2, "0.00 0.13 0.25 0.38 0.50");
}

void tst_NumberFormat::scaleLabelTextsCases()
{
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, majorTicks);
    QFETCH(int, precision);
    QFETCH(QStringList, expected);
    std::vector<double> values;
    for (int i = 0; i <= majorTicks; ++i) {
        values.push_back(minimum + static_cast<double>(i) / majorTicks * (maximum - minimum));
    }
    QCOMPARE(scaleLabelTexts(values, precision), expected);
}

void tst_NumberFormat::scaleLabelTextsEdgeCases()
{
    QCOMPARE(scaleLabelTexts({}, 2), QStringList());
    QCOMPARE(scaleLabelTexts({0.125}, 2), QStringList{QStringLiteral("0.13")}); // No step: precision
    QCOMPARE(scaleLabelTexts({5.0}, 2), QStringList{QStringLiteral("5")});
}

QTEST_MAIN(tst_NumberFormat)
#include "tst_NumberFormat.moc"
