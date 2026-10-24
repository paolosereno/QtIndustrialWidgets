// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

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
    void scaleLabelTextCases_data();
    void scaleLabelTextCases();
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

void tst_NumberFormat::scaleLabelTextCases_data()
{
    QTest::addColumn<double>("value");
    QTest::addColumn<int>("precision");
    QTest::addColumn<QString>("expected");

    // Range -0.1..0.1 with 5 major ticks: tick 2 is -0.02
    QTest::addRow("-0.02, precision 1") << (-0.1 + 2.0 / 5.0 * 0.2) << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.025, precision 1") << -0.024999999999999994 << 1 << QStringLiteral("0.0");
    QTest::addRow("-1.4e-17, precision 1") << -1.3877787807814457e-17 << 1 << QStringLiteral("0.0");
    QTest::addRow("-0.3, precision 0") << -0.3 << 0 << QStringLiteral("0");
    QTest::addRow("-0.05, precision 1") << -0.05 << 1 << QStringLiteral("-0.1");
    QTest::addRow("0, precision 1 (whole value)") << 0.0 << 1 << QStringLiteral("0");
    QTest::addRow("-2, precision 2 (whole value)") << -2.0 << 2 << QStringLiteral("-2");
    QTest::addRow("2.5, precision 1") << 2.5 << 1 << QStringLiteral("2.5");
    QTest::addRow("8000, precision 0") << 8000.0 << 0 << QStringLiteral("8000");
    QTest::addRow("-1000, precision 0") << -1000.0 << 0 << QStringLiteral("-1000");
}

void tst_NumberFormat::scaleLabelTextCases()
{
    QFETCH(double, value);
    QFETCH(int, precision);
    QFETCH(QString, expected);
    QCOMPARE(scaleLabelText(value, precision), expected);
}

QTEST_MAIN(tst_NumberFormat)
#include "tst_NumberFormat.moc"
