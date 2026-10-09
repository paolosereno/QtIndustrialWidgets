/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

class QRadialGauge;
class QLinearGauge;
class QSevenSegmentDisplay;
class QLedIndicator;
class QIndustrialKnob;
class QStripChart;
class QIndustrialSwitch;
class QLevelMeter;
class QAnnunciatorPanel;
class QCompass;
class QSlider;
class QLabel;
class QPushButton;
class QTabWidget;
class QPropertyAnimation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private Q_SLOTS:
    void toggleSimulation();
    void toggleAutoTour();
    void toggleTheme();
    void onSimulationTick();
    void onTourTick();

private:
    void setupUi();
    void applyTheme(bool dark);
    void switchToTabWithTransition(int nextIndex);

    // Gauges
    QRadialGauge *m_rpmGauge{nullptr};
    QRadialGauge *m_boostGauge{nullptr};
    QRadialGauge *m_oilGauge{nullptr};
    QRadialGauge *m_steeringGauge{nullptr};

    QLinearGauge *m_coolantGauge{nullptr};
    QLinearGauge *m_fuelGauge{nullptr};
    QLinearGauge *m_hydraulicGauge{nullptr};

    QSevenSegmentDisplay *m_speedDisplay{nullptr};
    QSevenSegmentDisplay *m_voltageDisplay{nullptr};
    QSevenSegmentDisplay *m_timerDisplay{nullptr};

    // LED Indicators
    QLedIndicator *m_runLed{nullptr};
    QLedIndicator *m_alarmLed{nullptr};
    QLedIndicator *m_warnLed{nullptr};
    QLedIndicator *m_pumpLed{nullptr};

    // Rotary Knobs
    QIndustrialKnob *m_throttleKnob{nullptr};
    QIndustrialKnob *m_boostKnob{nullptr};
    QIndustrialKnob *m_modeSelectorKnob{nullptr};

    // Telemetry Strip Chart
    QStripChart *m_stripChart{nullptr};
    int m_chRpm{0};
    int m_chBoost{1};
    int m_chTemp{2};

    // Industrial Switches
    QIndustrialSwitch *m_powerSwitch{nullptr};
    QIndustrialSwitch *m_safetySwitch{nullptr};
    QIndustrialSwitch *m_modeSwitch{nullptr};
    QIndustrialSwitch *m_rockerSwitch{nullptr};

    // Level Meters / VU Meters
    QLevelMeter *m_vibrationMeter{nullptr};
    QLevelMeter *m_audioVuMeter{nullptr};

    // Sliders for manual control
    QSlider *m_rpmSlider{nullptr};
    QSlider *m_boostSlider{nullptr};
    QSlider *m_oilSlider{nullptr};
    QSlider *m_coolantSlider{nullptr};
    QSlider *m_fuelSlider{nullptr};
    QSlider *m_hydraulicSlider{nullptr};

    // Alarm Annunciator Matrix (ISA-18.1)
    QAnnunciatorPanel *m_annunciatorPanel{nullptr};
    QLabel *m_annunciatorHornLabel{nullptr};
    QLabel *m_annunciatorStatusLabel{nullptr};

    // Navigation / Directional Gyro (QCompass)
    QCompass *m_compassHeadingUp{nullptr};
    QCompass *m_compassNorthUp{nullptr};
    QSlider *m_headingSlider{nullptr};
    QSlider *m_targetBugSlider{nullptr};
    QLabel *m_deviationLabel{nullptr};
    QPushButton *m_autopilotButton{nullptr};
    bool m_isAutopilotActive{false};

    // Simulation & UI state
    QPushButton *m_simButton{nullptr};
    QPushButton *m_tourButton{nullptr};
    QPushButton *m_themeButton{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLabel *m_fpsLabel{nullptr};
    QTabWidget *m_tabWidget{nullptr};

    QTimer m_simTimer;
    QTimer m_tourTimer;
    QPropertyAnimation *m_tabAnimation{nullptr};
    QElapsedTimer m_elapsedTimer;
    double m_simTime{0.0};
    int m_frameCount{0};
    qint64 m_lastFpsCheck{0};
    bool m_isDarkTheme{true};
    bool m_isSimulating{false};
    bool m_isAutoTourActive{false};
};
