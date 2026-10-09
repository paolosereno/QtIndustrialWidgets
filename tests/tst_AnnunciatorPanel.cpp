// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/AnnunciatorPanel.h>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

using namespace QtIndustrialWidgets;

class tst_AnnunciatorPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void gridSizing();
    void alarmTriggerAndSequenceA();
    void sequenceM_ManualReset();
    void acknowledgeAllAndSilence();
    void lampTest();
    void mouseInteraction();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_AnnunciatorPanel::defaultValues()
{
    AnnunciatorPanel panel;
    QCOMPARE(panel.rows(), 2);
    QCOMPARE(panel.columns(), 4);
    QCOMPARE(panel.tileCount(), 8);
    QCOMPARE(panel.sequence(), AnnunciatorPanel::AnnunciatorSequence::SequenceA_AutomaticReset);
    QCOMPARE(panel.isLampTestActive(), false);
    QCOMPARE(panel.isAudibleHornActive(), false);
    QCOMPARE(panel.activeAlarmsCount(), 0);
    QCOMPARE(panel.unacknowledgedCount(), 0);

    for (int i = 0; i < panel.tileCount(); ++i) {
        QCOMPARE(panel.tileState(i), AnnunciatorPanel::AlarmState::Normal);
        QCOMPARE(panel.isAlarmActive(i), false);
    }
}

void tst_AnnunciatorPanel::gridSizing()
{
    AnnunciatorPanel panel;
    panel.setGridSize(3, 5);
    QCOMPARE(panel.rows(), 3);
    QCOMPARE(panel.columns(), 5);
    QCOMPARE(panel.tileCount(), 15);

    panel.setTileText(0, QStringLiteral("BEARING 1\nTRIP"));
    QCOMPARE(panel.tileText(0), QStringLiteral("BEARING 1\nTRIP"));

    panel.setTileSeverity(0, AnnunciatorPanel::Severity::Critical);
    QCOMPARE(panel.tileSeverity(0), AnnunciatorPanel::Severity::Critical);

    panel.setTileText(1, 2, QStringLiteral("COOLANT\nWARN"));
    QCOMPARE(panel.tileText(1 * 5 + 2), QStringLiteral("COOLANT\nWARN"));
}

void tst_AnnunciatorPanel::alarmTriggerAndSequenceA()
{
    AnnunciatorPanel panel(2, 2);
    panel.setSequence(AnnunciatorPanel::AnnunciatorSequence::SequenceA_AutomaticReset);

    QSignalSpy stateSpy(&panel, &AnnunciatorPanel::tileStateChanged);
    QSignalSpy hornSpy(&panel, &AnnunciatorPanel::audibleHornChanged);
    QSignalSpy unackSpy(&panel, &AnnunciatorPanel::unacknowledgedCountChanged);

    // 1. Trigger alarm on tile 0
    panel.setAlarmActive(0, true);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Unacknowledged);
    QCOMPARE(panel.isAlarmActive(0), true);
    QCOMPARE(panel.isAudibleHornActive(), true);
    QCOMPARE(panel.unacknowledgedCount(), 1);

    QVERIFY(!stateSpy.isEmpty());
    QCOMPARE(stateSpy.last().at(0).toInt(), 0);
    QCOMPARE(stateSpy.last().at(1).value<AnnunciatorPanel::AlarmState>(), AnnunciatorPanel::AlarmState::Unacknowledged);
    QVERIFY(!hornSpy.isEmpty());
    QCOMPARE(hornSpy.last().at(0).toBool(), true);

    // 2. Operator Acknowledges tile 0
    panel.acknowledge(0);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Acknowledged);
    QCOMPARE(panel.isAudibleHornActive(), false);
    QCOMPARE(panel.unacknowledgedCount(), 0);
    QCOMPARE(panel.activeAlarmsCount(), 1);

    // 3. Sensor returns to normal -> in Sequence A, automatically resets to Normal
    panel.setAlarmActive(0, false);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Normal);
    QCOMPARE(panel.isAlarmActive(0), false);
    QCOMPARE(panel.activeAlarmsCount(), 0);
}

void tst_AnnunciatorPanel::sequenceM_ManualReset()
{
    AnnunciatorPanel panel(2, 2);
    panel.setSequence(AnnunciatorPanel::AnnunciatorSequence::SequenceM_ManualReset);

    // 1. Trip alarm
    panel.setAlarmActive(0, true);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Unacknowledged);

    // 2. Acknowledge
    panel.acknowledge(0);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Acknowledged);

    // 3. Sensor condition clears -> in Sequence M, moves to Ringback (waiting for operator Reset)
    panel.setAlarmActive(0, false);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Ringback);

    // 4. Operator Reset -> transitions to Normal
    panel.reset(0);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Normal);
}

void tst_AnnunciatorPanel::acknowledgeAllAndSilence()
{
    AnnunciatorPanel panel(2, 4);

    panel.setAlarmActive(0, true);
    panel.setAlarmActive(1, true);
    panel.setAlarmActive(2, true);

    QCOMPARE(panel.unacknowledgedCount(), 3);
    QCOMPARE(panel.isAudibleHornActive(), true);

    // Silence horn without acknowledging flashing tiles
    panel.silence();
    QCOMPARE(panel.isAudibleHornActive(), false);
    QCOMPARE(panel.unacknowledgedCount(), 3);

    // Acknowledge all active alarms
    panel.acknowledgeAll();
    QCOMPARE(panel.unacknowledgedCount(), 0);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Acknowledged);
    QCOMPARE(panel.tileState(1), AnnunciatorPanel::AlarmState::Acknowledged);
    QCOMPARE(panel.tileState(2), AnnunciatorPanel::AlarmState::Acknowledged);
}

void tst_AnnunciatorPanel::lampTest()
{
    AnnunciatorPanel panel;
    QSignalSpy appSpy(&panel, &AnnunciatorPanel::appearanceChanged);

    panel.setLampTest(true);
    QCOMPARE(panel.isLampTestActive(), true);
    QVERIFY(!appSpy.isEmpty());

    panel.setLampTest(false);
    QCOMPARE(panel.isLampTestActive(), false);
}

void tst_AnnunciatorPanel::mouseInteraction()
{
    AnnunciatorPanel panel(2, 2);
    panel.resize(400, 300);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    QSignalSpy clickSpy(&panel, &AnnunciatorPanel::tileClicked);

    // Trip tile 0 (top-left)
    panel.setAlarmActive(0, true);
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Unacknowledged);

    // Click inside tile 0
    QTest::mouseClick(&panel, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));

    QVERIFY(!clickSpy.isEmpty());
    QCOMPARE(clickSpy.last().at(0).toInt(), 0);

    // Verify click acknowledged tile 0
    QCOMPARE(panel.tileState(0), AnnunciatorPanel::AlarmState::Acknowledged);
}

void tst_AnnunciatorPanel::renderOffscreen()
{
    AnnunciatorPanel panel(3, 4);
    panel.resize(600, 350);

    panel.setAlarmActive(0, true);
    panel.acknowledge(0);
    panel.setAlarmActive(1, true); // unacknowledged flashing

    QPixmap pixmap(panel.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    panel.render(&painter);
    painter.end();

    QVERIFY(!pixmap.isNull());
}

void tst_AnnunciatorPanel::extremeResizeNoCrash()
{
    AnnunciatorPanel panel(2, 4);
    const QSize sizes[] = {
        QSize(1, 1),
        QSize(10, 10),
        QSize(50, 30),
        QSize(800, 600),
        QSize(3840, 2160)
    };

    for (const auto &sz : sizes) {
        panel.resize(sz);
        panel.repaint();
    }
}

QTEST_MAIN(tst_AnnunciatorPanel)
#include "tst_AnnunciatorPanel.moc"


