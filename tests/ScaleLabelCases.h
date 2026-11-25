// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

// #208: the scale labels of the widgets with a precision (RadialGauge, LinearGauge, IndustrialKnob, TankDisplay,
// IndustrialSlider), for the configurations of the plan: the decimals of the precision, as few as write every tick
// exactly, more only to keep adjacent labels distinct. One table for the five tests

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QtTest>

namespace ScaleLabelCases {

struct Case
{
    const char *name;
    double minimum;
    double maximum;
    int majorTicks;
    int precision;
    const char *labels; // Expected texts, separated by spaces
};

inline const Case kCases[] = {
    {"0-100, 10, 1 (unchanged)", 0.0, 100.0, 10, 1, "0 10 20 30 40 50 60 70 80 90 100"},
    {"0-1, 4, 2", 0.0, 1.0, 4, 2, "0.00 0.25 0.50 0.75 1.00"},
    {"0-0.5, 4, 2: no exact decimals up to 2", 0.0, 0.5, 4, 2, "0.00 0.13 0.25 0.38 0.50"},
    {"0-0.1, 5, 3: 2 decimals are exact", 0.0, 0.1, 5, 3, "0.00 0.02 0.04 0.06 0.08 0.10"},
    {"-0.05-0.05, 4, 3", -0.05, 0.05, 4, 3, "-0.050 -0.025 0.000 0.025 0.050"},
    {"0-2.4, 4, 2: 1 decimal is exact", 0.0, 2.4, 4, 2, "0.0 0.6 1.2 1.8 2.4"},
    {"0-1, 4, 0: repeated at 0, exact at 2", 0.0, 1.0, 4, 0, "0.00 0.25 0.50 0.75 1.00"},
    {"0-1, 3, 0: thirds, distinct at 1", 0.0, 1.0, 3, 0, "0.0 0.3 0.7 1.0"},
};

/// Columns minimum, maximum, majorTicks, precision, expected, one row per case
inline void addRows()
{
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("majorTicks");
    QTest::addColumn<int>("precision");
    QTest::addColumn<QStringList>("expected");
    for (const Case &c : kCases) {
        QTest::newRow(c.name) << c.minimum << c.maximum << c.majorTicks << c.precision
                              << QString::fromLatin1(c.labels).split(QLatin1Char(' '));
    }
}

} // namespace ScaleLabelCases
