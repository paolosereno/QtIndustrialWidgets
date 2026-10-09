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
class QSlider;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private Q_SLOTS:
    void toggleSimulation();
    void toggleTheme();
    void onSimulationTick();

private:
    void setupUi();
    void applyTheme(bool dark);

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

    // Sliders for manual control
    QSlider *m_rpmSlider{nullptr};
    QSlider *m_boostSlider{nullptr};
    QSlider *m_oilSlider{nullptr};
    QSlider *m_coolantSlider{nullptr};
    QSlider *m_fuelSlider{nullptr};
    QSlider *m_hydraulicSlider{nullptr};

    // Simulation & UI state
    QPushButton *m_simButton{nullptr};
    QPushButton *m_themeButton{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLabel *m_fpsLabel{nullptr};

    QTimer m_simTimer;
    QElapsedTimer m_elapsedTimer;
    double m_simTime{0.0};
    int m_frameCount{0};
    qint64 m_lastFpsCheck{0};
    bool m_isDarkTheme{true};
    bool m_isSimulating{false};
};
