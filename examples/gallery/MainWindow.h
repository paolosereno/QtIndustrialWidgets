/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtWidgets/QMainWindow>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

namespace QtIndustrialWidgets {
class RadialGauge;
class LinearGauge;
class SevenSegmentDisplay;
class LedIndicator;
class IndustrialKnob;
class StripChart;
class IndustrialSwitch;
class LevelMeter;
class AnnunciatorPanel;
class Compass;
class CircularBargraph;
class AnalysisChart;
}
class QSlider;
class QLabel;
class QComboBox;
class QPushButton;
class QTabWidget;
class QPropertyAnimation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    // Documentation image export (--export-gallery, see GalleryExport.cpp)
    /// Stops everything time-dependent and fills the charts with fixed analytic data.
    void prepareForExport();
    /// Renders every tab at the window's current size; appends the written paths to \a written.
    bool exportTabImages(const QString &dir, qreal dpr, bool dark, QStringList *written, QString *error);

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
    QtIndustrialWidgets::RadialGauge *m_rpmGauge{nullptr};
    QtIndustrialWidgets::RadialGauge *m_boostGauge{nullptr};
    QtIndustrialWidgets::RadialGauge *m_oilGauge{nullptr};
    QtIndustrialWidgets::RadialGauge *m_steeringGauge{nullptr};

    QtIndustrialWidgets::LinearGauge *m_coolantGauge{nullptr};
    QtIndustrialWidgets::LinearGauge *m_fuelGauge{nullptr};
    QtIndustrialWidgets::LinearGauge *m_hydraulicGauge{nullptr};

    QtIndustrialWidgets::SevenSegmentDisplay *m_speedDisplay{nullptr};
    QtIndustrialWidgets::SevenSegmentDisplay *m_voltageDisplay{nullptr};
    QtIndustrialWidgets::SevenSegmentDisplay *m_timerDisplay{nullptr};

    // LED Indicators
    QtIndustrialWidgets::LedIndicator *m_runLed{nullptr};
    QtIndustrialWidgets::LedIndicator *m_alarmLed{nullptr};
    QtIndustrialWidgets::LedIndicator *m_warnLed{nullptr};
    QtIndustrialWidgets::LedIndicator *m_pumpLed{nullptr};

    // Rotary Knobs
    QtIndustrialWidgets::IndustrialKnob *m_throttleKnob{nullptr};
    QtIndustrialWidgets::IndustrialKnob *m_boostKnob{nullptr};
    QtIndustrialWidgets::IndustrialKnob *m_modeSelectorKnob{nullptr};

    // Telemetry Strip Chart
    QtIndustrialWidgets::StripChart *m_stripChart{nullptr};
    int m_chRpm{0};
    int m_chBoost{1};
    int m_chTemp{2};

    // Industrial Switches
    QtIndustrialWidgets::IndustrialSwitch *m_powerSwitch{nullptr};
    QtIndustrialWidgets::IndustrialSwitch *m_safetySwitch{nullptr};
    QtIndustrialWidgets::IndustrialSwitch *m_modeSwitch{nullptr};
    QtIndustrialWidgets::IndustrialSwitch *m_rockerSwitch{nullptr};

    // Level Meters / VU Meters
    QtIndustrialWidgets::LevelMeter *m_vibrationMeter{nullptr};
    QtIndustrialWidgets::LevelMeter *m_audioVuMeter{nullptr};

    // Circular Bargraphs
    QtIndustrialWidgets::CircularBargraph *m_circBargraphBar{nullptr};
    QtIndustrialWidgets::CircularBargraph *m_circBargraphDot{nullptr};
    QtIndustrialWidgets::CircularBargraph *m_circBargraphSemi{nullptr};
    QSlider *m_circBargraphSlider{nullptr};
    QComboBox *m_circInputCombo{nullptr};   // Live / Overrange / Underrange / Invalid demo input
    QLabel *m_circStateLabel{nullptr};

    // Sliders for manual control
    QSlider *m_rpmSlider{nullptr};
    QSlider *m_boostSlider{nullptr};
    QSlider *m_oilSlider{nullptr};
    QSlider *m_coolantSlider{nullptr};
    QSlider *m_fuelSlider{nullptr};
    QSlider *m_hydraulicSlider{nullptr};

    // Alarm Annunciator Matrix (ISA-18.1)
    QtIndustrialWidgets::AnnunciatorPanel *m_annunciatorPanel{nullptr};
    QLabel *m_annunciatorHornLabel{nullptr};
    QLabel *m_annunciatorStatusLabel{nullptr};

    // Navigation / Directional Gyro (Compass)
    QtIndustrialWidgets::Compass *m_compassHeadingUp{nullptr};
    QtIndustrialWidgets::Compass *m_compassNorthUp{nullptr};
    QSlider *m_headingSlider{nullptr};
    QSlider *m_targetBugSlider{nullptr};
    QLabel *m_deviationLabel{nullptr};
    QPushButton *m_autopilotButton{nullptr};
    bool m_isAutopilotActive{false};

    // Recorded data review (AnalysisChart)
    QtIndustrialWidgets::AnalysisChart *m_analysisChart{nullptr};

    // High-Rate DAQ Showcase (StripChart)
    QtIndustrialWidgets::StripChart *m_daqChart{nullptr};
    int m_daqChannelSine{0};
    int m_daqChannelSquare{1};
    QLabel *m_daqMetricsLabel{nullptr};
    std::vector<double> m_daqPaintTimes;
    std::chrono::nanoseconds m_daqCurrentTime{0};
    double m_daqPhase{0.0};
    quint64 m_daqSampleIndex{0};

    // Simulation & UI state
    QPushButton *m_simButton{nullptr};
    QPushButton *m_tourButton{nullptr};
    QPushButton *m_themeButton{nullptr};
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
    bool m_exportMode{false};

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
