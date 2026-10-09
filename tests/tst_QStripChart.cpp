// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QStripChart.h>
#include <QtGui/QPixmap>

class tst_QStripChart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void channelManagement();
    void circularBufferInsertion();
    void autoScaling();
    void clearData();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_QStripChart::defaultValues()
{
    QStripChart chart;
    QCOMPARE(chart.channelCount(), 0);
    QCOMPARE(chart.capacity(), 300);
    QCOMPARE(chart.yMinimum(), 0.0);
    QCOMPARE(chart.yMaximum(), 100.0);
    QCOMPARE(chart.isAutoScaleY(), false);
}

void tst_QStripChart::channelManagement()
{
    QStripChart chart;
    int ch1 = chart.addChannel(QStringLiteral("Voltage"), Qt::red, 2.0);
    int ch2 = chart.addChannel(QStringLiteral("Current"), Qt::blue, 1.5);

    QCOMPARE(ch1, 0);
    QCOMPARE(ch2, 1);
    QCOMPARE(chart.channelCount(), 2);

    const auto *info = chart.channel(0);
    QVERIFY(info != nullptr);
    QCOMPARE(info->name, QStringLiteral("Voltage"));
    QCOMPARE(info->color, QColor(Qt::red));
}

void tst_QStripChart::circularBufferInsertion()
{
    QStripChart chart;
    chart.setCapacity(10);
    int ch = chart.addChannel(QStringLiteral("Signal"), Qt::green);

    // Insert more points than capacity to verify circular buffer wraparound
    for (int i = 1; i <= 20; ++i) {
        chart.addDataPoint(ch, static_cast<double>(i * 10));
    }

    const auto *info = chart.channel(ch);
    QVERIFY(info != nullptr);
    QCOMPARE(info->latestValue, 200.0);
    QCOMPARE(static_cast<int>(info->count), 10);
}

void tst_QStripChart::autoScaling()
{
    QStripChart chart;
    chart.setAutoScaleY(true);
    int ch = chart.addChannel(QStringLiteral("Wave"), Qt::cyan);

    chart.addDataPoint(ch, -150.0);
    chart.addDataPoint(ch, 350.0);

    QVERIFY(chart.yMinimum() <= -150.0);
    QVERIFY(chart.yMaximum() >= 350.0);
}

void tst_QStripChart::clearData()
{
    QStripChart chart;
    int ch = chart.addChannel(QStringLiteral("Data"), Qt::yellow);
    chart.addDataPoint(ch, 42.0);

    chart.clear();
    const auto *info = chart.channel(ch);
    QVERIFY(info != nullptr);
    QCOMPARE(info->count, static_cast<size_t>(0));
    QCOMPARE(info->latestValue, 0.0);
}

void tst_QStripChart::signalEmission()
{
    QStripChart chart;
    int ch = chart.addChannel(QStringLiteral("Test"), Qt::magenta);

    QSignalSpy spy(&chart, &QStripChart::dataAdded);
    chart.addDataPoint(ch, 12.3);

    QCOMPARE(spy.count(), 1);
}

void tst_QStripChart::renderOffscreen()
{
    QStripChart chart;
    int ch = chart.addChannel(QStringLiteral("Wave"), Qt::green);
    for (int i = 0; i < 50; ++i) {
        chart.addDataPoint(ch, std::sin(i * 0.1) * 50.0 + 50.0);
    }
    chart.resize(400, 250);

    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_QStripChart::extremeResizeNoCrash()
{
    QStripChart chart;
    chart.resize(2, 2);
    QPixmap pixSmall(chart.size());
    chart.render(&pixSmall);

    chart.resize(3840, 2160);
    QPixmap pixLarge(chart.size());
    chart.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QStripChart)
#include "tst_QStripChart.moc"
