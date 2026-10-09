// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include <QtIndustrialWidgets/QRadialGauge.h>
#include <QtIndustrialWidgets/QLinearGauge.h>
#include <QtIndustrialWidgets/QSevenSegmentDisplay.h>
#include <QtIndustrialWidgets/QLedIndicator.h>
#include <QtIndustrialWidgets/QIndustrialKnob.h>
#include <QtIndustrialWidgets/QStripChart.h>
#include <QtIndustrialWidgets/QIndustrialSwitch.h>
#include <QtIndustrialWidgets/QLevelMeter.h>
#include <QtIndustrialWidgets/QAnnunciatorPanel.h>
#include <QtIndustrialWidgets/QCompass.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QEasingCurve>
#include <QtGui/QPalette>
#include <QtCore/QtMath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("QtIndustrialWidgets — Test Bench & SCADA Showcase"));
    resize(1200, 820);
    setMinimumSize(900, 650);

    setupUi();
    applyTheme(true);

    connect(&m_simTimer, &QTimer::timeout, this, &MainWindow::onSimulationTick);
    connect(&m_tourTimer, &QTimer::timeout, this, &MainWindow::onTourTick);
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    // ========================================================================
    // Top Control / Status Bar
    // ========================================================================
    auto *topBar = new QFrame(this);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFrameShape(QFrame::StyledPanel);

    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(14, 10, 14, 10);
    topLayout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("⚡ <b>QtIndustrialWidgets</b> &nbsp;|&nbsp; High-Performance Instrumentation"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleLabel->setFont(titleFont);

    m_statusLabel = new QLabel(QStringLiteral("● Standby (Manual Mode)"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-weight: bold;"));

    m_fpsLabel = new QLabel(QStringLiteral("FPS: -- (0 ms)"), this);
    m_fpsLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace;"));

    // Annunciator Status LEDs
    m_runLed = new QLedIndicator(QColor(46, 204, 113), topBar);
    m_runLed->setLabelText(QStringLiteral("RUN"));
    m_runLed->setOn(false);

    m_warnLed = new QLedIndicator(QColor(254, 211, 48), topBar);
    m_warnLed->setLabelText(QStringLiteral("WARN"));
    m_warnLed->setOn(false);

    m_alarmLed = new QLedIndicator(QColor(235, 59, 90), topBar);
    m_alarmLed->setLabelText(QStringLiteral("FAULT"));
    m_alarmLed->setOn(false);
    m_alarmLed->setBlinkRateMs(180);

    m_pumpLed = new QLedIndicator(QColor(0, 229, 255), topBar);
    m_pumpLed->setLabelText(QStringLiteral("AUX PUMP"));
    m_pumpLed->setShape(QLedIndicator::LedShape::Rectangular);
    m_pumpLed->setOn(false);

    m_simButton = new QPushButton(QStringLiteral("▶ Start Simulation (50 Hz)"), this);
    m_simButton->setCursor(Qt::PointingHandCursor);
    m_simButton->setMinimumHeight(32);
    connect(m_simButton, &QPushButton::clicked, this, &MainWindow::toggleSimulation);

    m_tourButton = new QPushButton(QStringLiteral("🔄 Auto-Tour (Off)"), this);
    m_tourButton->setCheckable(true);
    m_tourButton->setCursor(Qt::PointingHandCursor);
    m_tourButton->setMinimumHeight(32);
    m_tourButton->setToolTip(QStringLiteral("Cycle automatically between tabs every 3.5 seconds with a smooth transition"));
    connect(m_tourButton, &QPushButton::clicked, this, &MainWindow::toggleAutoTour);

    m_themeButton = new QPushButton(QStringLiteral("☀️ Switch to Light Theme"), this);
    m_themeButton->setCursor(Qt::PointingHandCursor);
    m_themeButton->setMinimumHeight(32);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    topLayout->addWidget(titleLabel);
    topLayout->addSpacing(12);
    topLayout->addWidget(m_runLed);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_warnLed);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_alarmLed);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_pumpLed);
    topLayout->addStretch();
    topLayout->addWidget(m_fpsLabel);
    topLayout->addWidget(m_simButton);
    topLayout->addWidget(m_tourButton);
    topLayout->addWidget(m_themeButton);

    rootLayout->addWidget(topBar);

    // ========================================================================
    // Tabs: Dashboard vs Manual Controls
    // ========================================================================
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName(QStringLiteral("mainTabs"));
    auto *tabWidget = m_tabWidget;

    connect(m_tabWidget, &QTabWidget::tabBarClicked, this, [this](int) {
        if (m_isAutoTourActive) {
            m_tourTimer.start(3500); // Reset timer on manual tab interaction
        }
    });

    // ------------------------------------------------------------------------
    // TAB 1: Live Telemetry Dashboard
    // ------------------------------------------------------------------------
    auto *dashTab = new QWidget(tabWidget);
    auto *dashLayout = new QVBoxLayout(dashTab);
    dashLayout->setContentsMargins(12, 12, 12, 12);
    dashLayout->setSpacing(14);

    // Row 1: Radial Gauges Group
    auto *radialGroup = new QGroupBox(QStringLiteral("Powertrain & Dynamics (Radial Gauges)"), dashTab);
    auto *radialLayout = new QHBoxLayout(radialGroup);
    radialLayout->setContentsMargins(12, 16, 12, 12);
    radialLayout->setSpacing(14);

    // RPM Gauge (0 - 8000 RPM)
    m_rpmGauge = new QRadialGauge(radialGroup);
    m_rpmGauge->setRange(0.0, 8000.0);
    m_rpmGauge->setValue(2400.0);
    m_rpmGauge->setUnit(QStringLiteral("RPM"));
    m_rpmGauge->setPrecision(0);
    m_rpmGauge->setMajorTicks(8);
    m_rpmGauge->setMinorTicks(4);
    m_rpmGauge->setWarningThreshold(6000.0);
    m_rpmGauge->setErrorThreshold(7200.0);
    m_rpmGauge->setSpanAngle(270.0);

    // Boost Gauge (0.0 - 3.0 bar)
    m_boostGauge = new QRadialGauge(radialGroup);
    m_boostGauge->setRange(0.0, 3.0);
    m_boostGauge->setValue(1.15);
    m_boostGauge->setUnit(QStringLiteral("bar"));
    m_boostGauge->setPrecision(2);
    m_boostGauge->setMajorTicks(6);
    m_boostGauge->setMinorTicks(4);
    m_boostGauge->setWarningThreshold(2.2);
    m_boostGauge->setErrorThreshold(2.6);
    m_boostGauge->setSpanAngle(240.0);

    // Oil Pressure Gauge (0.0 - 10.0 bar)
    m_oilGauge = new QRadialGauge(radialGroup);
    m_oilGauge->setRange(0.0, 10.0);
    m_oilGauge->setValue(4.8);
    m_oilGauge->setUnit(QStringLiteral("bar"));
    m_oilGauge->setPrecision(1);
    m_oilGauge->setMajorTicks(5);
    m_oilGauge->setMinorTicks(3);
    m_oilGauge->setWarningThreshold(7.5);
    m_oilGauge->setErrorThreshold(8.8);
    m_oilGauge->setSpanAngle(220.0);

    // Steering / Angle Semicircular Gauge (-90° to +90°)
    m_steeringGauge = new QRadialGauge(radialGroup);
    m_steeringGauge->setRange(-90.0, 90.0);
    m_steeringGauge->setValue(15.0);
    m_steeringGauge->setUnit(QStringLiteral("DEG"));
    m_steeringGauge->setPrecision(0);
    m_steeringGauge->setStartAngle(-90.0);
    m_steeringGauge->setSpanAngle(180.0);
    m_steeringGauge->setMajorTicks(6);
    m_steeringGauge->setMinorTicks(2);
    m_steeringGauge->setWarningThreshold(65.0);
    m_steeringGauge->setErrorThreshold(80.0);

    radialLayout->addWidget(m_rpmGauge, 1);
    radialLayout->addWidget(m_boostGauge, 1);
    radialLayout->addWidget(m_oilGauge, 1);
    radialLayout->addWidget(m_steeringGauge, 1);
    dashLayout->addWidget(radialGroup, 4);

    // Row 2: Fluid Levels & Column Gauges + Digital Displays
    auto *lowerRowLayout = new QHBoxLayout();
    lowerRowLayout->setSpacing(14);

    // Group for Linear Gauges / Thermometers
    auto *linearGroup = new QGroupBox(QStringLiteral("Thermal & Fluid Systems (Linear Gauges)"), dashTab);
    auto *linearLayout = new QHBoxLayout(linearGroup);
    linearLayout->setContentsMargins(12, 16, 12, 12);
    linearLayout->setSpacing(14);

    // Coolant Thermometer (with bulb mode)
    m_coolantGauge = new QLinearGauge(linearGroup);
    m_coolantGauge->setOrientation(Qt::Vertical);
    m_coolantGauge->setThermometerMode(true);
    m_coolantGauge->setRange(0.0, 120.0);
    m_coolantGauge->setValue(88.0);
    m_coolantGauge->setUnit(QStringLiteral("°C"));
    m_coolantGauge->setPrecision(1);
    m_coolantGauge->setWarningThreshold(95.0);
    m_coolantGauge->setErrorThreshold(108.0);

    // Fuel Tank Gauge (rectangular bar mode)
    m_fuelGauge = new QLinearGauge(linearGroup);
    m_fuelGauge->setOrientation(Qt::Vertical);
    m_fuelGauge->setThermometerMode(false);
    m_fuelGauge->setRange(0.0, 100.0);
    m_fuelGauge->setValue(65.0);
    m_fuelGauge->setUnit(QStringLiteral("%"));
    m_fuelGauge->setPrecision(0);
    m_fuelGauge->setWarningThreshold(75.0);
    m_fuelGauge->setErrorThreshold(90.0);

    // Hydraulic Line Pressure (Horizontal)
    m_hydraulicGauge = new QLinearGauge(linearGroup);
    m_hydraulicGauge->setOrientation(Qt::Horizontal);
    m_hydraulicGauge->setThermometerMode(false);
    m_hydraulicGauge->setRange(0.0, 250.0);
    m_hydraulicGauge->setValue(140.0);
    m_hydraulicGauge->setUnit(QStringLiteral("bar"));
    m_hydraulicGauge->setPrecision(0);
    m_hydraulicGauge->setWarningThreshold(190.0);
    m_hydraulicGauge->setErrorThreshold(225.0);

    auto *vertGaugesLayout = new QHBoxLayout();
    vertGaugesLayout->addWidget(m_coolantGauge);
    vertGaugesLayout->addWidget(m_fuelGauge);

    auto *combinedLinearLayout = new QVBoxLayout();
    combinedLinearLayout->addLayout(vertGaugesLayout, 3);
    combinedLinearLayout->addWidget(m_hydraulicGauge, 1);

    // Dynamic Vibration Dual-Channel Meter (QLevelMeter)
    m_vibrationMeter = new QLevelMeter(linearGroup);
    m_vibrationMeter->setChannelCount(2);
    m_vibrationMeter->setChannelLabels({QStringLiteral("X"), QStringLiteral("Y")});
    m_vibrationMeter->setTitle(QStringLiteral("VIB RMS"));
    m_vibrationMeter->setUnit(QStringLiteral("g"));
    m_vibrationMeter->setRange(0.0, 10.0);
    m_vibrationMeter->setWarningThreshold(6.5);
    m_vibrationMeter->setErrorThreshold(8.5);
    m_vibrationMeter->setSegmentCount(22);
    m_vibrationMeter->setValue(0, 2.4);
    m_vibrationMeter->setValue(1, 1.8);

    linearLayout->addLayout(combinedLinearLayout, 3);
    linearLayout->addWidget(m_vibrationMeter, 2);
    lowerRowLayout->addWidget(linearGroup, 4);

    // Group for 7-Segment Digital Readouts
    auto *digitalGroup = new QGroupBox(QStringLiteral("High-Speed Telemetry (7-Segment Displays)"), dashTab);
    auto *digitalLayout = new QVBoxLayout(digitalGroup);
    digitalLayout->setContentsMargins(14, 16, 14, 14);
    digitalLayout->setSpacing(12);

    // Speed display
    auto *speedBox = new QHBoxLayout();
    auto *speedLbl = new QLabel(QStringLiteral("SPEED [km/h]:"), digitalGroup);
    speedLbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    m_speedDisplay = new QSevenSegmentDisplay(digitalGroup);
    m_speedDisplay->setDigitCount(5);
    m_speedDisplay->setDecimalPlaces(1);
    m_speedDisplay->setValue(142.6);
    m_speedDisplay->setActiveSegmentColor(QColor(0, 229, 255)); // Neon Cyan
    m_speedDisplay->setInactiveSegmentColor(QColor(0, 229, 255, 30));
    speedBox->addWidget(speedLbl, 1);
    speedBox->addWidget(m_speedDisplay, 3);

    // Battery / Bus Voltage display
    auto *voltBox = new QHBoxLayout();
    auto *voltLbl = new QLabel(QStringLiteral("BUS VOLTAGE [V]:"), digitalGroup);
    voltLbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    m_voltageDisplay = new QSevenSegmentDisplay(digitalGroup);
    m_voltageDisplay->setDigitCount(4);
    m_voltageDisplay->setDecimalPlaces(1);
    m_voltageDisplay->setValue(13.8);
    m_voltageDisplay->setActiveSegmentColor(QColor(46, 204, 113)); // Neon Emerald
    m_voltageDisplay->setInactiveSegmentColor(QColor(46, 204, 113, 30));
    voltBox->addWidget(voltLbl, 1);
    voltBox->addWidget(m_voltageDisplay, 3);

    // Test Run Time display
    auto *timerBox = new QHBoxLayout();
    auto *timerLbl = new QLabel(QStringLiteral("TEST TIME [s]:"), digitalGroup);
    timerLbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    m_timerDisplay = new QSevenSegmentDisplay(digitalGroup);
    m_timerDisplay->setDigitCount(6);
    m_timerDisplay->setDecimalPlaces(2);
    m_timerDisplay->setValue(45.28);
    m_timerDisplay->setActiveSegmentColor(QColor(254, 211, 48)); // Amber Gold
    m_timerDisplay->setInactiveSegmentColor(QColor(254, 211, 48, 30));
    timerBox->addWidget(timerLbl, 1);
    timerBox->addWidget(m_timerDisplay, 3);

    digitalLayout->addLayout(speedBox);
    digitalLayout->addLayout(voltBox);
    digitalLayout->addLayout(timerBox);

    lowerRowLayout->addWidget(digitalGroup, 3);

    // Group for Real-Time Oscilloscope / Telemetry Strip Chart
    auto *chartGroup = new QGroupBox(QStringLiteral("Real-Time Telemetry (QStripChart)"), dashTab);
    auto *chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->setContentsMargins(10, 16, 10, 10);

    m_stripChart = new QStripChart(chartGroup);
    m_stripChart->setCapacity(300);
    m_stripChart->setYRange(0.0, 120.0);
    m_chRpm = m_stripChart->addChannel(QStringLiteral("RPM %"), QColor(0, 229, 255), 2.0);
    m_chBoost = m_stripChart->addChannel(QStringLiteral("Boost x35"), QColor(235, 59, 90), 2.0);
    m_chTemp = m_stripChart->addChannel(QStringLiteral("Temp °C"), QColor(46, 204, 113), 2.0);

    chartLayout->addWidget(m_stripChart);
    lowerRowLayout->addWidget(chartGroup, 4);

    dashLayout->addLayout(lowerRowLayout, 3);

    tabWidget->addTab(dashTab, QStringLiteral("📊 Live Instrumentation Dashboard"));

    // ------------------------------------------------------------------------
    // TAB 2: Manual Control Sliders & Calibration
    // ------------------------------------------------------------------------
    auto *controlsTab = new QWidget(tabWidget);
    auto *controlsLayout = new QGridLayout(controlsTab);
    controlsLayout->setContentsMargins(20, 20, 20, 20);
    controlsLayout->setSpacing(14);

    int row = 0;

    auto addSliderControl = [&](const QString &label, int minVal, int maxVal, int currVal,
                                double scaleFactor, auto setterSlot, QSlider *&outSlider) {
        auto *lbl = new QLabel(label, controlsTab);
        lbl->setStyleSheet(QStringLiteral("font-weight: bold;"));

        auto *slider = new QSlider(Qt::Horizontal, controlsTab);
        slider->setRange(minVal, maxVal);
        slider->setValue(currVal);
        outSlider = slider;

        auto *valLbl = new QLabel(QString::number(currVal * scaleFactor, 'f', (scaleFactor < 1.0) ? 2 : 0), controlsTab);
        valLbl->setFixedWidth(60);

        connect(slider, &QSlider::valueChanged, this, [=](int val) {
            double actualVal = val * scaleFactor;
            valLbl->setText(QString::number(actualVal, 'f', (scaleFactor < 1.0) ? 2 : 0));
            setterSlot(actualVal);
        });

        controlsLayout->addWidget(lbl, row, 0);
        controlsLayout->addWidget(slider, row, 1);
        controlsLayout->addWidget(valLbl, row, 2);
        row++;
    };

    addSliderControl(QStringLiteral("Engine RPM (0 - 8000):"), 0, 8000, 2400, 1.0,
                     [this](double v) {
                         m_rpmGauge->setValue(v);
                         if (m_throttleKnob && !m_isSimulating) m_throttleKnob->setValue(v);
                     }, m_rpmSlider);

    addSliderControl(QStringLiteral("Turbo Boost (0.0 - 3.0 bar):"), 0, 300, 115, 0.01,
                     [this](double v) {
                         m_boostGauge->setValue(v);
                         if (m_boostKnob && !m_isSimulating) m_boostKnob->setValue(v);
                     }, m_boostSlider);

    addSliderControl(QStringLiteral("Oil Pressure (0.0 - 10.0 bar):"), 0, 100, 48, 0.1,
                     [this](double v) { m_oilGauge->setValue(v); }, m_oilSlider);

    addSliderControl(QStringLiteral("Coolant Temp (0 - 120 °C):"), 0, 1200, 880, 0.1,
                     [this](double v) { m_coolantGauge->setValue(v); }, m_coolantSlider);

    addSliderControl(QStringLiteral("Fuel Level (0 - 100 %):"), 0, 100, 65, 1.0,
                     [this](double v) { m_fuelGauge->setValue(v); }, m_fuelSlider);

    addSliderControl(QStringLiteral("Hydraulic Line (0 - 250 bar):"), 0, 250, 140, 1.0,
                     [this](double v) { m_hydraulicGauge->setValue(v); }, m_hydraulicSlider);

    // ------------------------------------------------------------------------
    // Precision Rotary Knobs Showcase Group (QIndustrialKnob)
    // ------------------------------------------------------------------------
    auto *knobBox = new QGroupBox(QStringLiteral("Precision Rotary Controls (QIndustrialKnob)"), controlsTab);
    auto *knobLayout = new QHBoxLayout(knobBox);
    knobLayout->setSpacing(24);
    knobLayout->setContentsMargins(16, 18, 16, 14);

    // Throttle / Target RPM Knob (Continuous)
    m_throttleKnob = new QIndustrialKnob(knobBox);
    m_throttleKnob->setRange(0.0, 8000.0);
    m_throttleKnob->setValue(2400.0);
    m_throttleKnob->setUnit(QStringLiteral("RPM"));
    m_throttleKnob->setStep(50.0);
    m_throttleKnob->setPrecision(0);
    m_throttleKnob->setMajorTicks(8);
    m_throttleKnob->setMinorTicks(4);

    // Boost Regulator Knob (Continuous)
    m_boostKnob = new QIndustrialKnob(knobBox);
    m_boostKnob->setRange(0.0, 3.0);
    m_boostKnob->setValue(1.15);
    m_boostKnob->setUnit(QStringLiteral("bar"));
    m_boostKnob->setStep(0.05);
    m_boostKnob->setPrecision(2);
    m_boostKnob->setMajorTicks(6);
    m_boostKnob->setMinorTicks(3);
    m_boostKnob->setPointerColor(QColor(235, 59, 90));
    m_boostKnob->setTrackColor(QColor(235, 59, 90));

    // Drive Mode Selector Knob (Discrete 4 positions)
    m_modeSelectorKnob = new QIndustrialKnob(knobBox);
    m_modeSelectorKnob->setMode(QIndustrialKnob::KnobMode::Discrete);
    m_modeSelectorKnob->setDiscreteSteps(4);
    m_modeSelectorKnob->setRange(1.0, 4.0);
    m_modeSelectorKnob->setValue(2.0);
    m_modeSelectorKnob->setUnit(QStringLiteral("MODE"));
    m_modeSelectorKnob->setPointerColor(QColor(46, 204, 113));
    m_modeSelectorKnob->setTrackColor(QColor(46, 204, 113));

    // Synchronize throttle knob with slider and gauge
    connect(m_throttleKnob, &QIndustrialKnob::valueChanged, this, [this](double val) {
        if (!m_isSimulating) {
            m_rpmSlider->setValue(static_cast<int>(val));
            m_rpmGauge->setValue(val);
        }
    });

    // Synchronize boost knob with slider and gauge
    connect(m_boostKnob, &QIndustrialKnob::valueChanged, this, [this](double val) {
        if (!m_isSimulating) {
            m_boostSlider->setValue(static_cast<int>(val * 100.0));
            m_boostGauge->setValue(val);
        }
    });

    knobLayout->addWidget(m_throttleKnob, 1);
    knobLayout->addWidget(m_boostKnob, 1);
    knobLayout->addWidget(m_modeSelectorKnob, 1);

    controlsLayout->addWidget(knobBox, row, 0, 1, 3);
    row++;

    // ------------------------------------------------------------------------
    // Interactive LED testing group (QLedIndicator)
    // ------------------------------------------------------------------------
    auto *ledBox = new QGroupBox(QStringLiteral("Interactive QLedIndicator Showcase (Click on LEDs to Toggle)"), controlsTab);
    auto *ledLayout = new QHBoxLayout(ledBox);
    ledLayout->setSpacing(20);

    auto *testLed1 = new QLedIndicator(QColor(46, 204, 113), ledBox);
    testLed1->setLabelText(QStringLiteral("Green (Click Me)"));
    testLed1->setClickable(true);

    auto *testLed2 = new QLedIndicator(QColor(235, 59, 90), ledBox);
    testLed2->setLabelText(QStringLiteral("Red Blinking (2 Hz)"));
    testLed2->setBlinking(true);
    testLed2->setBlinkRateMs(250);
    testLed2->setClickable(true);

    auto *testLed3 = new QLedIndicator(QColor(254, 211, 48), ledBox);
    testLed3->setLabelText(QStringLiteral("Amber Rectangular"));
    testLed3->setShape(QLedIndicator::LedShape::Rectangular);
    testLed3->setClickable(true);

    auto *testLed4 = new QLedIndicator(QColor(0, 229, 255), ledBox);
    testLed4->setLabelText(QStringLiteral("Cyan Rectangular"));
    testLed4->setShape(QLedIndicator::LedShape::Rectangular);
    testLed4->setClickable(true);

    ledLayout->addWidget(testLed1);
    ledLayout->addWidget(testLed2);
    ledLayout->addWidget(testLed3);
    ledLayout->addWidget(testLed4);
    ledLayout->addStretch();

    controlsLayout->addWidget(ledBox, row, 0, 1, 3);
    row++;

    // ------------------------------------------------------------------------
    // Heavy-Duty Industrial Switches Showcase Group (QIndustrialSwitch)
    // ------------------------------------------------------------------------
    auto *switchBox = new QGroupBox(QStringLiteral("Heavy-Duty Industrial Switches (QIndustrialSwitch)"), controlsTab);
    auto *switchLayout = new QHBoxLayout(switchBox);
    switchLayout->setSpacing(28);
    switchLayout->setContentsMargins(18, 18, 18, 14);

    // 1. Classic Main Power Toggle Switch
    m_powerSwitch = new QIndustrialSwitch(switchBox);
    m_powerSwitch->setLabel(QStringLiteral("MAIN PWR"));
    m_powerSwitch->setLabelOn(QStringLiteral("ON"));
    m_powerSwitch->setLabelOff(QStringLiteral("OFF"));
    m_powerSwitch->setChecked(false);
    connect(m_powerSwitch, &QIndustrialSwitch::toggled, this, [this](bool on) {
        if (on != m_isSimulating) {
            toggleSimulation();
        }
    });

    // 2. High-Risk Safety Guard Switch (Red flip-up cover)
    m_safetySwitch = new QIndustrialSwitch(switchBox);
    m_safetySwitch->setLabel(QStringLiteral("EMERGENCY"));
    m_safetySwitch->setLabelOn(QStringLiteral("ARMED"));
    m_safetySwitch->setLabelOff(QStringLiteral("SAFE"));
    m_safetySwitch->setHasSafetyGuard(true);
    m_safetySwitch->setGuardColor(QColor(220, 53, 69));
    m_safetySwitch->setLedColor(QColor(235, 59, 90));
    connect(m_safetySwitch, &QIndustrialSwitch::toggled, this, [this](bool armed) {
        if (m_alarmLed) {
            m_alarmLed->setOn(armed);
            m_alarmLed->setBlinking(armed);
        }
    });

    // 3. 3-Position Mode Selector (MANUAL / OFF / AUTO)
    m_modeSwitch = new QIndustrialSwitch(switchBox);
    m_modeSwitch->setPositionCount(3);
    m_modeSwitch->setLabel(QStringLiteral("SYS MODE"));
    m_modeSwitch->setLabelOff(QStringLiteral("MAN"));
    m_modeSwitch->setLabelCenter(QStringLiteral("OFF"));
    m_modeSwitch->setLabelOn(QStringLiteral("AUTO"));
    m_modeSwitch->setPosition(1);
    m_modeSwitch->setLedColor(QColor(254, 211, 48));

    // 4. Industrial Rocker Switch (Cooling Fan)
    m_rockerSwitch = new QIndustrialSwitch(switchBox);
    m_rockerSwitch->setSwitchType(QIndustrialSwitch::SwitchType::Rocker);
    m_rockerSwitch->setLabel(QStringLiteral("COOLING"));
    m_rockerSwitch->setLabelOn(QStringLiteral("HIGH"));
    m_rockerSwitch->setLabelOff(QStringLiteral("LOW"));
    m_rockerSwitch->setLedColor(QColor(0, 229, 255));
    m_rockerSwitch->setChecked(false);
    connect(m_rockerSwitch, &QIndustrialSwitch::toggled, this, [this](bool on) {
        if (m_pumpLed) {
            m_pumpLed->setOn(on);
        }
    });

    // 5. Horizontal Toggle Switch (Bus Tie Feed)
    auto *horizSwitch = new QIndustrialSwitch(switchBox);
    horizSwitch->setOrientation(Qt::Horizontal);
    horizSwitch->setLabel(QStringLiteral("BUS TIE"));
    horizSwitch->setLabelOff(QStringLiteral("GEN A"));
    horizSwitch->setLabelOn(QStringLiteral("GEN B"));
    horizSwitch->setLedColor(QColor(155, 89, 182));

    switchLayout->addWidget(m_powerSwitch);
    switchLayout->addWidget(m_safetySwitch);
    switchLayout->addWidget(m_modeSwitch);
    switchLayout->addWidget(m_rockerSwitch);
    switchLayout->addWidget(horizSwitch);
    switchLayout->addStretch();

    controlsLayout->addWidget(switchBox, row, 0, 1, 3);
    row++;

    // ------------------------------------------------------------------------
    // Multi-Channel VU & Level Meter Showcase (QLevelMeter)
    // ------------------------------------------------------------------------
    auto *meterBox = new QGroupBox(QStringLiteral("Acoustic & Signal Level Meters (QLevelMeter)"), controlsTab);
    auto *meterLayout = new QHBoxLayout(meterBox);
    meterLayout->setSpacing(24);
    meterLayout->setContentsMargins(18, 18, 18, 14);

    // 1. Classic Studio Stereo VU Meter (-60 dB to +6 dB)
    m_audioVuMeter = new QLevelMeter(meterBox);
    m_audioVuMeter->setChannelCount(2);
    m_audioVuMeter->setTitle(QStringLiteral("MASTER BUS"));
    m_audioVuMeter->setChannelLabels({QStringLiteral("CH 1"), QStringLiteral("CH 2")});
    m_audioVuMeter->setRange(-60.0, 6.0);
    m_audioVuMeter->setWarningThreshold(-6.0);
    m_audioVuMeter->setErrorThreshold(0.0);
    m_audioVuMeter->setSegmentCount(28);
    m_audioVuMeter->setUnit(QStringLiteral("dB"));
    m_audioVuMeter->setValue(0, -12.0);
    m_audioVuMeter->setValue(1, -14.0);

    // 2. Continuous Mode Smooth Level Meter (0 - 100%)
    auto *contMeter = new QLevelMeter(meterBox);
    contMeter->setChannelCount(1);
    contMeter->setDisplayMode(QLevelMeter::DisplayMode::Continuous);
    contMeter->setTitle(QStringLiteral("LINE RMS"));
    contMeter->setChannelLabels({QStringLiteral("LINE")});
    contMeter->setRange(0.0, 100.0);
    contMeter->setWarningThreshold(75.0);
    contMeter->setErrorThreshold(90.0);
    contMeter->setUnit(QStringLiteral("%"));
    contMeter->setValue(62.0);

    // 3. Horizontal Level Meter
    auto *horizMeter = new QLevelMeter(meterBox);
    horizMeter->setOrientation(Qt::Horizontal);
    horizMeter->setChannelCount(2);
    horizMeter->setTitle(QStringLiteral("TELEMETRY LINK"));
    horizMeter->setChannelLabels({QStringLiteral("TX"), QStringLiteral("RX")});
    horizMeter->setRange(0.0, 100.0);
    horizMeter->setWarningThreshold(70.0);
    horizMeter->setErrorThreshold(90.0);
    horizMeter->setUnit(QStringLiteral("%"));
    horizMeter->setValue(0, 82.0);
    horizMeter->setValue(1, 74.0);

    meterLayout->addWidget(m_audioVuMeter);
    meterLayout->addWidget(contMeter);
    meterLayout->addWidget(horizMeter);
    meterLayout->addStretch();

    controlsLayout->addWidget(meterBox, row, 0, 1, 3);
    row++;

    controlsLayout->setRowStretch(row, 1);
    tabWidget->addTab(controlsTab, QStringLiteral("🎛️ Manual Controls & Diagnostics"));

    // ------------------------------------------------------------------------
    // TAB 3: Alarm Annunciator Matrix (ANSI/ISA-18.1)
    // ------------------------------------------------------------------------
    auto *annunciatorTab = new QWidget(tabWidget);
    auto *annLayout = new QVBoxLayout(annunciatorTab);
    annLayout->setContentsMargins(16, 16, 16, 16);
    annLayout->setSpacing(14);

    // Annunciator Header Bar: Status, Horn Indicator, Counts
    auto *annHeaderBox = new QFrame(annunciatorTab);
    annHeaderBox->setFrameShape(QFrame::StyledPanel);
    auto *annHeaderLayout = new QHBoxLayout(annHeaderBox);
    annHeaderLayout->setContentsMargins(14, 10, 14, 10);
    annHeaderLayout->setSpacing(16);

    auto *annTitleLbl = new QLabel(QStringLiteral("🚨 <b>ANSI/ISA-18.1 Alarm Annunciator Window Matrix</b>"), annHeaderBox);
    annTitleLbl->setStyleSheet(QStringLiteral("font-size: 13px;"));

    m_annunciatorHornLabel = new QLabel(QStringLiteral("🔇 HORN: SILENT"), annHeaderBox);
    m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #a4b3c6;"));

    m_annunciatorStatusLabel = new QLabel(QStringLiteral("Active: 0 | Unack: 0 | Normal: 12"), annHeaderBox);
    m_annunciatorStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #00e5ff;"));

    annHeaderLayout->addWidget(annTitleLbl);
    annHeaderLayout->addStretch();
    annHeaderLayout->addWidget(m_annunciatorHornLabel);
    annHeaderLayout->addWidget(m_annunciatorStatusLabel);
    annLayout->addWidget(annHeaderBox);

    // The Central Matrix Panel (3 rows x 4 columns = 12 windows)
    m_annunciatorPanel = new QAnnunciatorPanel(3, 4, annunciatorTab);
    m_annunciatorPanel->setMinimumHeight(320);

    const struct {
        int row;
        int col;
        const char *text;
        QAnnunciatorPanel::Severity severity;
    } tileConfigs[] = {
        {0, 0, "TURBINE 1\nOVERSPEED TRIP", QAnnunciatorPanel::Severity::Critical},
        {0, 1, "MAIN STEAM\nPRESS HIGH", QAnnunciatorPanel::Severity::Critical},
        {0, 2, "BEARING OIL\nPRESS LOW", QAnnunciatorPanel::Severity::Critical},
        {0, 3, "GENERATOR\nLOCKOUT TRIP", QAnnunciatorPanel::Severity::Critical},

        {1, 0, "FEEDWATER PUMP\nTRIP FAULT", QAnnunciatorPanel::Severity::Warning},
        {1, 1, "CONDENSER\nVACUUM LOW", QAnnunciatorPanel::Severity::Warning},
        {1, 2, "TRANSFORMER\nTEMP HIGH", QAnnunciatorPanel::Severity::Warning},
        {1, 3, "MAIN STEAM\nTEMP HIGH", QAnnunciatorPanel::Severity::Warning},

        {2, 0, "FIRE SUPPRESSION\nDISCHARGED", QAnnunciatorPanel::Severity::Critical},
        {2, 1, "AUX DIESEL GEN\nRUNNING", QAnnunciatorPanel::Severity::Advisory},
        {2, 2, "UPS BATTERY\nON INVERTER", QAnnunciatorPanel::Severity::Advisory},
        {2, 3, "SCADA TELEMETRY\nLINK OFFLINE", QAnnunciatorPanel::Severity::Warning}
    };

    for (const auto &cfg : tileConfigs) {
        m_annunciatorPanel->setTileText(cfg.row, cfg.col, QString::fromLatin1(cfg.text));
        m_annunciatorPanel->setTileSeverity(cfg.row, cfg.col, cfg.severity);
    }

    annLayout->addWidget(m_annunciatorPanel, 1);

    // Control and Simulator Controls Panel (2 columns of QGroupBox)
    auto *annControlsRow = new QHBoxLayout();
    annControlsRow->setSpacing(14);

    // Operator Pushbuttons (ANSI/ISA-18.1 standard)
    auto *operatorBox = new QGroupBox(QStringLiteral("Standard Operator Pushbuttons (ANSI/ISA-18.1)"), annunciatorTab);
    auto *operatorLayout = new QHBoxLayout(operatorBox);
    operatorLayout->setSpacing(12);
    operatorLayout->setContentsMargins(14, 16, 14, 14);

    auto *ackBtn = new QPushButton(QStringLiteral("🔔 ACKNOWLEDGE (ACK)"), operatorBox);
    ackBtn->setCursor(Qt::PointingHandCursor);
    ackBtn->setStyleSheet(QStringLiteral("background-color: #0984e3; color: white; font-weight: bold; padding: 8px 16px;"));
    connect(ackBtn, &QPushButton::clicked, m_annunciatorPanel, &QAnnunciatorPanel::acknowledgeAll);

    auto *silenceBtn = new QPushButton(QStringLiteral("🔇 SILENCE"), operatorBox);
    silenceBtn->setCursor(Qt::PointingHandCursor);
    silenceBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(silenceBtn, &QPushButton::clicked, m_annunciatorPanel, &QAnnunciatorPanel::silence);

    auto *resetBtn = new QPushButton(QStringLiteral("🔄 RESET"), operatorBox);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(resetBtn, &QPushButton::clicked, m_annunciatorPanel, &QAnnunciatorPanel::resetAll);

    auto *lampTestBtn = new QPushButton(QStringLiteral("💡 LAMP TEST"), operatorBox);
    lampTestBtn->setCheckable(true);
    lampTestBtn->setCursor(Qt::PointingHandCursor);
    lampTestBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(lampTestBtn, &QPushButton::toggled, m_annunciatorPanel, &QAnnunciatorPanel::setLampTest);

    operatorLayout->addWidget(ackBtn);
    operatorLayout->addWidget(silenceBtn);
    operatorLayout->addWidget(resetBtn);
    operatorLayout->addWidget(lampTestBtn);
    annControlsRow->addWidget(operatorBox, 1);

    // Alarm Trip Simulator & ISA Sequence selector
    auto *simBox = new QGroupBox(QStringLiteral("Alarm Event Simulator & Configuration"), annunciatorTab);
    auto *simLayout = new QHBoxLayout(simBox);
    simLayout->setSpacing(12);
    simLayout->setContentsMargins(14, 16, 14, 14);

    auto *seqCombo = new QComboBox(simBox);
    seqCombo->addItem(QStringLiteral("Sequence A (Automatic Reset)"), static_cast<int>(QAnnunciatorPanel::AnnunciatorSequence::SequenceA_AutomaticReset));
    seqCombo->addItem(QStringLiteral("Sequence M (Manual Reset)"), static_cast<int>(QAnnunciatorPanel::AnnunciatorSequence::SequenceM_ManualReset));
    connect(seqCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, seqCombo](int index) {
        auto seq = static_cast<QAnnunciatorPanel::AnnunciatorSequence>(seqCombo->itemData(index).toInt());
        m_annunciatorPanel->setSequence(seq);
    });

    auto *tripTurbineBtn = new QPushButton(QStringLiteral("Trip Turbine"), simBox);
    tripTurbineBtn->setCheckable(true);
    connect(tripTurbineBtn, &QPushButton::toggled, this, [this](bool on) {
        m_annunciatorPanel->setAlarmActive(0, 0, on);
    });

    auto *tripSteamBtn = new QPushButton(QStringLiteral("Trip Steam"), simBox);
    tripSteamBtn->setCheckable(true);
    connect(tripSteamBtn, &QPushButton::toggled, this, [this](bool on) {
        m_annunciatorPanel->setAlarmActive(0, 1, on);
    });

    auto *tripFireBtn = new QPushButton(QStringLiteral("Trip Fire Sys"), simBox);
    tripFireBtn->setCheckable(true);
    connect(tripFireBtn, &QPushButton::toggled, this, [this](bool on) {
        m_annunciatorPanel->setAlarmActive(2, 0, on);
    });

    auto *clearAllBtn = new QPushButton(QStringLiteral("Clear Trips"), simBox);
    connect(clearAllBtn, &QPushButton::clicked, this, [this, tripTurbineBtn, tripSteamBtn, tripFireBtn]() {
        tripTurbineBtn->setChecked(false);
        tripSteamBtn->setChecked(false);
        tripFireBtn->setChecked(false);
        for (int i = 0; i < m_annunciatorPanel->tileCount(); ++i) {
            m_annunciatorPanel->setAlarmActive(i, false);
        }
    });

    simLayout->addWidget(new QLabel(QStringLiteral("Sequence:"), simBox));
    simLayout->addWidget(seqCombo);
    simLayout->addWidget(tripTurbineBtn);
    simLayout->addWidget(tripSteamBtn);
    simLayout->addWidget(tripFireBtn);
    simLayout->addWidget(clearAllBtn);
    annControlsRow->addWidget(simBox, 1);

    annLayout->addLayout(annControlsRow);

    // Connect Annunciator signals to status bar
    auto updateStatus = [this]() {
        int act = m_annunciatorPanel->activeAlarmsCount();
        int unack = m_annunciatorPanel->unacknowledgedCount();
        int total = m_annunciatorPanel->tileCount();
        int norm = total - act;
        m_annunciatorStatusLabel->setText(
            QStringLiteral("Active: %1 | Unacknowledged: %2 | Normal: %3")
                .arg(act).arg(unack).arg(norm));
    };

    connect(m_annunciatorPanel, &QAnnunciatorPanel::activeAlarmsCountChanged, this, [updateStatus](int) {
        updateStatus();
    });
    connect(m_annunciatorPanel, &QAnnunciatorPanel::unacknowledgedCountChanged, this, [updateStatus](int) {
        updateStatus();
    });
    connect(m_annunciatorPanel, &QAnnunciatorPanel::audibleHornChanged, this, [this](bool horn) {
        if (horn) {
            m_annunciatorHornLabel->setText(QStringLiteral("🔊 HORN: SOUNDING (AUDIBLE ALARM)"));
            m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #eb3b5a; border-radius: 4px; color: #ffffff;"));
        } else {
            m_annunciatorHornLabel->setText(QStringLiteral("🔇 HORN: SILENT"));
            m_annunciatorHornLabel->setStyleSheet(m_isDarkTheme 
                ? QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #a4b3c6;")
                : QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #e8ecf1; border-radius: 4px; color: #718093;"));
        }
    });

    // When an operator clicks a tile directly:
    connect(m_annunciatorPanel, &QAnnunciatorPanel::tileClicked, this, [this](int idx) {
        if (m_annunciatorPanel->tileState(idx) == QAnnunciatorPanel::AlarmState::Unacknowledged) {
            m_annunciatorPanel->acknowledge(idx);
        } else if (m_annunciatorPanel->tileState(idx) == QAnnunciatorPanel::AlarmState::Ringback) {
            m_annunciatorPanel->reset(idx);
        } else if (m_annunciatorPanel->tileState(idx) == QAnnunciatorPanel::AlarmState::Normal) {
            m_annunciatorPanel->setAlarmActive(idx, !m_annunciatorPanel->isAlarmActive(idx));
        }
    });

    tabWidget->addTab(annunciatorTab, QStringLiteral("🚨 Alarm Annunciator Matrix (ISA-18.1)"));

    // ------------------------------------------------------------------------
    // TAB 4: Navigation & Directional Gyro (QCompass)
    // ------------------------------------------------------------------------
    auto *navTab = new QWidget(tabWidget);
    auto *navLayout = new QVBoxLayout(navTab);
    navLayout->setContentsMargins(16, 16, 16, 16);
    navLayout->setSpacing(14);

    // Navigation Header Bar: Mode & Course Deviation
    auto *navHeaderBox = new QFrame(navTab);
    navHeaderBox->setFrameShape(QFrame::StyledPanel);
    auto *navHeaderLayout = new QHBoxLayout(navHeaderBox);
    navHeaderLayout->setContentsMargins(14, 10, 14, 10);
    navHeaderLayout->setSpacing(16);

    auto *navTitleLbl = new QLabel(QStringLiteral("🧭 <b>Aeronautical & Marine Directional Navigation Instruments</b>"), navHeaderBox);
    navTitleLbl->setStyleSheet(QStringLiteral("font-size: 13px;"));

    m_deviationLabel = new QLabel(QStringLiteral("CDI: ON COURSE (000°)"), navHeaderBox);
    m_deviationLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #2ecc71;"));

    auto *autopilotStatusLabel = new QLabel(QStringLiteral("AUTOPILOT: STANDBY"), navHeaderBox);
    autopilotStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #00e5ff;"));

    navHeaderLayout->addWidget(navTitleLbl);
    navHeaderLayout->addStretch();
    navHeaderLayout->addWidget(m_deviationLabel);
    navHeaderLayout->addWidget(autopilotStatusLabel);
    navLayout->addWidget(navHeaderBox);

    // Center Instruments Row: Heading-Up (left) and North-Up (right)
    auto *instrumentsRow = new QHBoxLayout();
    instrumentsRow->setSpacing(16);

    // Group 1: Heading-Up Marine Gyrocompass
    auto *headingUpGroup = new QGroupBox(QStringLiteral("Heading-Up Gyrocompass (Aircraft / Marine Compass Card)"), navTab);
    auto *huLayout = new QVBoxLayout(headingUpGroup);
    huLayout->setContentsMargins(12, 16, 12, 12);
    m_compassHeadingUp = new QCompass(headingUpGroup);
    m_compassHeadingUp->setDisplayMode(QCompass::DisplayMode::HeadingUp);
    m_compassHeadingUp->setHeading(45.0);
    m_compassHeadingUp->setTargetHeading(90.0);
    m_compassHeadingUp->setMinimumHeight(280);
    huLayout->addWidget(m_compassHeadingUp);
    instrumentsRow->addWidget(headingUpGroup, 1);

    // Group 2: North-Up Directional Indicator
    auto *northUpGroup = new QGroupBox(QStringLiteral("North-Up Heading Indicator (360° Magnetic Pointer)"), navTab);
    auto *nuLayout = new QVBoxLayout(northUpGroup);
    nuLayout->setContentsMargins(12, 16, 12, 12);
    m_compassNorthUp = new QCompass(northUpGroup);
    m_compassNorthUp->setDisplayMode(QCompass::DisplayMode::NorthUp);
    m_compassNorthUp->setHeading(45.0);
    m_compassNorthUp->setTargetHeading(90.0);
    m_compassNorthUp->setMinimumHeight(280);
    nuLayout->addWidget(m_compassNorthUp);
    instrumentsRow->addWidget(northUpGroup, 1);

    navLayout->addLayout(instrumentsRow, 1);

    // Bottom Controls & Simulation Console
    auto *navControlsBox = new QGroupBox(QStringLiteral("Navigation Controls & Autopilot Simulation"), navTab);
    auto *ncLayout = new QGridLayout(navControlsBox);
    ncLayout->setContentsMargins(16, 16, 16, 14);
    ncLayout->setSpacing(12);

    // Heading Slider (0 - 359°)
    auto *hdgLbl = new QLabel(QStringLiteral("Vessel Heading (0° - 359°):"), navControlsBox);
    hdgLbl->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_headingSlider = new QSlider(Qt::Horizontal, navControlsBox);
    m_headingSlider->setRange(0, 359);
    m_headingSlider->setValue(45);

    auto *hdgValLbl = new QLabel(QStringLiteral("045°"), navControlsBox);
    hdgValLbl->setFixedWidth(50);
    hdgValLbl->setStyleSheet(QStringLiteral("font-family: monospace; font-weight: bold;"));

    // Target Bug Slider (0 - 359°)
    auto *bugLbl = new QLabel(QStringLiteral("Target Bug Heading (Course):"), navControlsBox);
    bugLbl->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_targetBugSlider = new QSlider(Qt::Horizontal, navControlsBox);
    m_targetBugSlider->setRange(0, 359);
    m_targetBugSlider->setValue(90);

    auto *bugValLbl = new QLabel(QStringLiteral("090°"), navControlsBox);
    bugValLbl->setFixedWidth(50);
    bugValLbl->setStyleSheet(QStringLiteral("font-family: monospace; font-weight: bold; color: #ff793f;"));

    ncLayout->addWidget(hdgLbl, 0, 0);
    ncLayout->addWidget(m_headingSlider, 0, 1);
    ncLayout->addWidget(hdgValLbl, 0, 2);

    ncLayout->addWidget(bugLbl, 1, 0);
    ncLayout->addWidget(m_targetBugSlider, 1, 1);
    ncLayout->addWidget(bugValLbl, 1, 2);

    // Quick cardinal buttons and Autopilot button
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    auto addCardBtn = [&](const QString &txt, double deg) {
        auto *btn = new QPushButton(txt, navControlsBox);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [=]() {
            m_compassHeadingUp->setTargetHeading(deg);
            m_compassNorthUp->setTargetHeading(deg);
            m_targetBugSlider->setValue(static_cast<int>(deg));
        });
        buttonRow->addWidget(btn);
    };

    addCardBtn(QStringLiteral("🧭 North (000°)"), 0.0);
    addCardBtn(QStringLiteral("🧭 East (090°)"), 90.0);
    addCardBtn(QStringLiteral("🧭 South (180°)"), 180.0);
    addCardBtn(QStringLiteral("🧭 West (270°)"), 270.0);

    // Align Button
    auto *alignBtn = new QPushButton(QStringLiteral("⚡ Align Bug to Heading"), navControlsBox);
    alignBtn->setCursor(Qt::PointingHandCursor);
    connect(alignBtn, &QPushButton::clicked, this, [=]() {
        double currentHdg = m_compassHeadingUp->heading();
        m_compassHeadingUp->setTargetHeading(currentHdg);
        m_compassNorthUp->setTargetHeading(currentHdg);
        m_targetBugSlider->setValue(static_cast<int>(currentHdg));
    });
    buttonRow->addWidget(alignBtn);

    // Autopilot Button
    m_autopilotButton = new QPushButton(QStringLiteral("🤖 Engage Autopilot Heading Hold"), navControlsBox);
    m_autopilotButton->setCheckable(true);
    m_autopilotButton->setCursor(Qt::PointingHandCursor);
    m_autopilotButton->setStyleSheet(QStringLiteral("background-color: #242c3d; color: #00e5ff; font-weight: bold; padding: 6px 14px;"));
    connect(m_autopilotButton, &QPushButton::toggled, this, [this, autopilotStatusLabel](bool checked) {
        m_isAutopilotActive = checked;
        if (checked) {
            m_autopilotButton->setText(QStringLiteral("⏹ Disengage Autopilot"));
            m_autopilotButton->setStyleSheet(QStringLiteral("background-color: #eb3b5a; color: white; font-weight: bold; padding: 6px 14px;"));
            autopilotStatusLabel->setText(QStringLiteral("AUTOPILOT: LOCKED (HEADING HOLD)"));
            autopilotStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #2ecc71;"));
        } else {
            m_autopilotButton->setText(QStringLiteral("🤖 Engage Autopilot Heading Hold"));
            m_autopilotButton->setStyleSheet(QStringLiteral("background-color: #242c3d; color: #00e5ff; font-weight: bold; padding: 6px 14px;"));
            autopilotStatusLabel->setText(QStringLiteral("AUTOPILOT: STANDBY"));
            autopilotStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #a4b3c6;"));
        }
    });
    buttonRow->addWidget(m_autopilotButton);

    ncLayout->addLayout(buttonRow, 2, 0, 1, 3);

    // Synchronize sliders with both compasses
    auto updateDeviation = [this]() {
        double dev = m_compassHeadingUp->courseDeviation();
        int idev = static_cast<int>(std::round(dev));
        if (std::abs(idev) == 0) {
            m_deviationLabel->setText(QStringLiteral("CDI: ON COURSE (000°)"));
            m_deviationLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #2ecc71;"));
        } else if (idev > 0) {
            m_deviationLabel->setText(QStringLiteral("CDI: +%1° STBD (RIGHT)").arg(idev, 3, 10, QLatin1Char('0')));
            m_deviationLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #fed330;"));
        } else {
            m_deviationLabel->setText(QStringLiteral("CDI: -%1° PORT (LEFT)").arg(-idev, 3, 10, QLatin1Char('0')));
            m_deviationLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #ff793f;"));
        }
    };

    connect(m_headingSlider, &QSlider::valueChanged, this, [=](int val) {
        hdgValLbl->setText(QStringLiteral("%1°").arg(val, 3, 10, QLatin1Char('0')));
        m_compassHeadingUp->setHeading(val);
        m_compassNorthUp->setHeading(val);
        updateDeviation();
    });

    connect(m_targetBugSlider, &QSlider::valueChanged, this, [=](int val) {
        bugValLbl->setText(QStringLiteral("%1°").arg(val, 3, 10, QLatin1Char('0')));
        m_compassHeadingUp->setTargetHeading(val);
        m_compassNorthUp->setTargetHeading(val);
        updateDeviation();
    });

    // When user drags bug directly on compass:
    connect(m_compassHeadingUp, &QCompass::targetHeadingChanged, this, [=](double val) {
        m_compassNorthUp->setTargetHeading(val);
        m_targetBugSlider->setValue(static_cast<int>(val));
        bugValLbl->setText(QStringLiteral("%1°").arg(static_cast<int>(val), 3, 10, QLatin1Char('0')));
        updateDeviation();
    });
    connect(m_compassNorthUp, &QCompass::targetHeadingChanged, this, [=](double val) {
        m_compassHeadingUp->setTargetHeading(val);
        m_targetBugSlider->setValue(static_cast<int>(val));
        bugValLbl->setText(QStringLiteral("%1°").arg(static_cast<int>(val), 3, 10, QLatin1Char('0')));
        updateDeviation();
    });

    navLayout->addWidget(navControlsBox);
    tabWidget->addTab(navTab, QStringLiteral("🧭 Directional Gyro & Marine Compass (QCompass)"));

    rootLayout->addWidget(tabWidget);
}

void MainWindow::toggleSimulation()
{
    if (m_isSimulating) {
        m_simTimer.stop();
        m_isSimulating = false;
        m_simButton->setText(QStringLiteral("▶ Start Simulation (50 Hz)"));
        m_fpsLabel->setText(QStringLiteral("FPS: --"));

        m_runLed->setOn(false);
        m_warnLed->setOn(false);
        m_alarmLed->setOn(false);
        m_alarmLed->setBlinking(false);
        m_pumpLed->setOn(false);
        if (m_powerSwitch) m_powerSwitch->setChecked(false);
    } else {
        m_simTime = 0.0;
        m_frameCount = 0;
        m_elapsedTimer.start();
        m_lastFpsCheck = m_elapsedTimer.elapsed();
        m_simTimer.start(20); // 50 Hz (20 ms interval)
        m_isSimulating = true;
        m_simButton->setText(QStringLiteral("⏹ Stop Simulation"));

        m_runLed->setOn(true);
        m_pumpLed->setOn(true);
        if (m_powerSwitch) m_powerSwitch->setChecked(true);
    }
}

void MainWindow::toggleAutoTour()
{
    m_isAutoTourActive = !m_isAutoTourActive;
    if (m_isAutoTourActive) {
        m_tourButton->setChecked(true);
        m_tourButton->setText(QStringLiteral("⏸ Auto-Tour (3.5s)"));
        m_tourButton->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #0984e3; border: 1px solid #74b9ff; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #0870c2; }"
        ));
        m_tourTimer.start(3500);
    } else {
        m_tourButton->setChecked(false);
        m_tourButton->setText(QStringLiteral("🔄 Auto-Tour (Off)"));
        m_tourButton->setStyleSheet(QString());
        m_tourTimer.stop();
        if (m_tabAnimation) {
            m_tabAnimation->stop();
            delete m_tabAnimation;
            m_tabAnimation = nullptr;
        }
    }
}

void MainWindow::onTourTick()
{
    if (!m_tabWidget || m_tabWidget->count() == 0) {
        return;
    }
    int nextIndex = (m_tabWidget->currentIndex() + 1) % m_tabWidget->count();
    switchToTabWithTransition(nextIndex);
}

void MainWindow::switchToTabWithTransition(int nextIndex)
{
    if (!m_tabWidget || nextIndex < 0 || nextIndex >= m_tabWidget->count()) {
        return;
    }
    if (nextIndex == m_tabWidget->currentIndex()) {
        return;
    }

    if (m_tabAnimation) {
        m_tabAnimation->stop();
        delete m_tabAnimation;
        m_tabAnimation = nullptr;
    }

    QWidget *nextWidget = m_tabWidget->widget(nextIndex);
    if (!nextWidget) {
        m_tabWidget->setCurrentIndex(nextIndex);
        return;
    }

    auto *opacityEffect = new QGraphicsOpacityEffect(nextWidget);
    opacityEffect->setOpacity(0.0);
    nextWidget->setGraphicsEffect(opacityEffect);

    m_tabWidget->setCurrentIndex(nextIndex);

    m_tabAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    m_tabAnimation->setDuration(350); // 350 ms smooth fade-in
    m_tabAnimation->setStartValue(0.0);
    m_tabAnimation->setEndValue(1.0);
    m_tabAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_tabAnimation, &QPropertyAnimation::finished, this, [this, nextWidget]() {
        nextWidget->setGraphicsEffect(nullptr);
        m_tabAnimation = nullptr;
    });

    m_tabAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onSimulationTick()
{
    m_simTime += 0.02; // 20 ms
    m_frameCount++;

    // Calculate real-time FPS every 500 ms
    qint64 now = m_elapsedTimer.elapsed();
    if (now - m_lastFpsCheck >= 500) {
        double fps = (m_frameCount * 1000.0) / (now - m_lastFpsCheck);
        m_fpsLabel->setText(QStringLiteral("FPS: %1 (20 ms tick)").arg(QString::number(fps, 'f', 1)));
        m_frameCount = 0;
        m_lastFpsCheck = now;
    }

    // 1. Realistic Engine Cycle Simulation (RPM & Turbo boost)
    double cycle = std::fmod(m_simTime * 0.4, 4.0); // 4-second cycle
    double rpmBase = 2000.0 + 5200.0 * std::sin(cycle * (M_PI / 4.0));
    double rpmNoise = 60.0 * std::sin(m_simTime * 15.0);
    double currentRpm = std::clamp(rpmBase + rpmNoise, 800.0, 7800.0);
    m_rpmGauge->setValue(currentRpm);

    // Boost tracks RPM with a slight lag/spool
    double boostBase = 0.3 + 2.2 * (currentRpm / 7800.0);
    double boostNoise = 0.05 * std::cos(m_simTime * 20.0);
    m_boostGauge->setValue(std::clamp(boostBase + boostNoise, 0.0, 2.95));

    // Oil pressure varies smoothly with RPM and temperature
    double oilPressure = 2.5 + 5.5 * (currentRpm / 8000.0) + 0.15 * std::sin(m_simTime * 5.0);
    m_oilGauge->setValue(oilPressure);

    // Steering oscillating sin wave (-45° to +45°)
    double steeringAngle = 45.0 * std::sin(m_simTime * 1.2);
    m_steeringGauge->setValue(steeringAngle);

    // Coolant temp gentle oscillation around 92°C
    double coolantTemp = 91.5 + 4.0 * std::sin(m_simTime * 0.15);
    m_coolantGauge->setValue(coolantTemp);

    // Slow fuel consumption
    double fuel = std::max(5.0, 80.0 - std::fmod(m_simTime * 0.5, 75.0));
    m_fuelGauge->setValue(fuel);

    // Hydraulic fluctuation
    double hydraulic = 150.0 + 40.0 * std::sin(m_simTime * 2.5) + 10.0 * std::cos(m_simTime * 7.0);
    m_hydraulicGauge->setValue(hydraulic);

    // Dynamic Annunciator LEDs
    bool isWarning = (currentRpm >= 6000.0 || coolantTemp >= 94.0 || boostBase >= 2.2);
    m_warnLed->setOn(isWarning);

    bool isAlarm = (currentRpm >= 7200.0 || coolantTemp >= 105.0 || boostBase >= 2.6);
    m_alarmLed->setOn(isAlarm);
    m_alarmLed->setBlinking(isAlarm);

    // Speed display based on gear and RPM
    double simulatedSpeed = (currentRpm / 8000.0) * 260.0;
    m_speedDisplay->setValue(simulatedSpeed);

    // Bus voltage 13.8V with small alternator ripple
    double voltage = 13.8 + 0.35 * std::sin(m_simTime * 8.0);
    m_voltageDisplay->setValue(voltage);

    // Elapsed test time
    m_timerDisplay->setValue(m_simTime);

    // Stream real-time waveforms into QStripChart
    m_stripChart->addDataPoint(m_chRpm, (currentRpm / 8000.0) * 100.0);
    m_stripChart->addDataPoint(m_chBoost, boostBase * 35.0);
    m_stripChart->addDataPoint(m_chTemp, coolantTemp);

    // Dynamic multi-channel vibration levels (QLevelMeter)
    if (m_vibrationMeter) {
        double vibX = 1.5 + (currentRpm / 8000.0) * 5.2 + 1.2 * std::sin(m_simTime * 14.0);
        double vibY = 1.2 + (boostBase / 3.0) * 4.8 + 1.0 * std::cos(m_simTime * 18.0);
        if (std::fmod(m_simTime, 4.0) < 0.1) {
            vibX += 2.2;
        }
        m_vibrationMeter->setValue(0, vibX);
        m_vibrationMeter->setValue(1, vibY);
    }

    // Dynamic acoustic / bus VU levels (QLevelMeter)
    if (m_audioVuMeter) {
        double db1 = -26.0 + (currentRpm / 8000.0) * 24.0 + 3.0 * std::sin(m_simTime * 9.0);
        double db2 = -28.0 + (boostBase / 3.0) * 26.0 + 2.5 * std::cos(m_simTime * 11.0);
        m_audioVuMeter->setValue(0, db1);
        m_audioVuMeter->setValue(1, db2);
    }

    // Dynamic annunciator alarms based on telemetry trips
    if (m_annunciatorPanel) {
        if (currentRpm >= 7200.0 && !m_annunciatorPanel->isAlarmActive(0)) {
            m_annunciatorPanel->setAlarmActive(0, true);
        } else if (currentRpm < 6800.0 && m_annunciatorPanel->isAlarmActive(0)) {
            m_annunciatorPanel->setAlarmActive(0, false);
        }

        if (boostBase >= 2.4 && !m_annunciatorPanel->isAlarmActive(1)) {
            m_annunciatorPanel->setAlarmActive(1, true);
        } else if (boostBase < 2.0 && m_annunciatorPanel->isAlarmActive(1)) {
            m_annunciatorPanel->setAlarmActive(1, false);
        }
    }

    // Autopilot Course Correction for QCompass
    if (m_isAutopilotActive && m_compassHeadingUp && m_compassNorthUp) {
        double dev = m_compassHeadingUp->courseDeviation();
        if (std::abs(dev) > 0.4) {
            double turnRate = std::clamp(dev * 0.08, -1.8, 1.8);
            double newHdg = QCompass::normalizeDegrees(m_compassHeadingUp->heading() - turnRate);
            m_compassHeadingUp->setHeading(newHdg);
            m_compassNorthUp->setHeading(newHdg);
            if (m_headingSlider) {
                m_headingSlider->setValue(static_cast<int>(newHdg));
            }
        }
    }
}

void MainWindow::toggleTheme()
{
    applyTheme(!m_isDarkTheme);
}

void MainWindow::applyTheme(bool dark)
{
    m_isDarkTheme = dark;
    m_themeButton->setText(dark ? QStringLiteral("☀️ Switch to Light Theme")
                                : QStringLiteral("🌙 Switch to Dark Theme"));

    if (m_tourButton) {
        if (m_isAutoTourActive) {
            m_tourButton->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #0984e3; border: 1px solid #74b9ff; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
                "QPushButton:hover { background-color: #0870c2; }"
            ));
        } else {
            m_tourButton->setStyleSheet(QString());
        }
    }

    if (dark) {
        // Modern Industrial Dark SCADA Theme
        QString qss = QStringLiteral(
            "QMainWindow { background-color: #12161f; }"
            "QWidget { color: #e1e7ec; font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif; }"
            "QFrame#topBar { background-color: #1a1f2c; border-radius: 6px; border: 1px solid #283042; }"
            "QTabWidget::pane { border: 1px solid #283042; background: #171c26; border-radius: 6px; }"
            "QTabBar::tab { background: #1c2230; color: #8e9bb0; padding: 8px 18px; margin-right: 4px; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; }"
            "QTabBar::tab:selected { background: #22293a; color: #00e5ff; border-bottom: 2px solid #00e5ff; }"
            "QGroupBox { border: 1px solid #283042; border-radius: 6px; margin-top: 1.1em; font-weight: bold; color: #a4b3c6; background-color: #181d27; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }"
            "QPushButton { background-color: #242c3d; border: 1px solid #364259; border-radius: 4px; color: #e1e7ec; padding: 6px 14px; font-weight: bold; }"
            "QPushButton:hover { background-color: #2e384e; border-color: #00e5ff; }"
            "QPushButton:pressed { background-color: #19202d; }"
            "QSlider::groove:horizontal { border: 1px solid #2e384e; height: 6px; background: #1a202c; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #00e5ff; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #00e5ff; width: 16px; margin-top: -5px; margin-bottom: -5px; border-radius: 8px; }"
        );
        setStyleSheet(qss);

        // Update widget colors for dark mode
        m_rpmGauge->setDialColor(QColor(20, 24, 32));
        m_rpmGauge->setTextColor(QColor(245, 246, 250));
        m_rpmGauge->setBezelColor(QColor(48, 56, 70));

        m_boostGauge->setDialColor(QColor(20, 24, 32));
        m_boostGauge->setTextColor(QColor(245, 246, 250));
        m_boostGauge->setBezelColor(QColor(48, 56, 70));

        m_oilGauge->setDialColor(QColor(20, 24, 32));
        m_oilGauge->setTextColor(QColor(245, 246, 250));
        m_oilGauge->setBezelColor(QColor(48, 56, 70));

        m_steeringGauge->setDialColor(QColor(20, 24, 32));
        m_steeringGauge->setTextColor(QColor(245, 246, 250));
        m_steeringGauge->setBezelColor(QColor(48, 56, 70));

        m_coolantGauge->setBezelColor(QColor(36, 43, 56));
        m_coolantGauge->setTroughColor(QColor(20, 24, 32));
        m_coolantGauge->setTextColor(QColor(245, 246, 250));

        m_fuelGauge->setBezelColor(QColor(36, 43, 56));
        m_fuelGauge->setTroughColor(QColor(20, 24, 32));
        m_fuelGauge->setTextColor(QColor(245, 246, 250));

        m_hydraulicGauge->setBezelColor(QColor(36, 43, 56));
        m_hydraulicGauge->setTroughColor(QColor(20, 24, 32));
        m_hydraulicGauge->setTextColor(QColor(245, 246, 250));

        m_speedDisplay->setBackgroundColor(QColor(16, 20, 28));
        m_speedDisplay->setBezelColor(QColor(40, 48, 62));
        m_voltageDisplay->setBackgroundColor(QColor(16, 20, 28));
        m_voltageDisplay->setBezelColor(QColor(40, 48, 62));
        m_timerDisplay->setBackgroundColor(QColor(16, 20, 28));
        m_timerDisplay->setBezelColor(QColor(40, 48, 62));

        if (m_throttleKnob) {
            m_throttleKnob->setKnobColor(QColor(42, 48, 60));
            m_throttleKnob->setScaleColor(QColor(190, 200, 215));
            m_throttleKnob->setTextColor(QColor(240, 244, 250));
        }
        if (m_boostKnob) {
            m_boostKnob->setKnobColor(QColor(42, 48, 60));
            m_boostKnob->setScaleColor(QColor(190, 200, 215));
            m_boostKnob->setTextColor(QColor(240, 244, 250));
        }
        if (m_modeSelectorKnob) {
            m_modeSelectorKnob->setKnobColor(QColor(42, 48, 60));
            m_modeSelectorKnob->setScaleColor(QColor(190, 200, 215));
            m_modeSelectorKnob->setTextColor(QColor(240, 244, 250));
        }

        if (m_stripChart) {
            m_stripChart->setBackgroundColor(QColor(14, 18, 25));
            m_stripChart->setGridColor(QColor(42, 54, 70));
            m_stripChart->setBezelColor(QColor(38, 46, 60));
        }

        if (m_annunciatorPanel) {
            m_annunciatorPanel->setFrameColor(QColor(30, 36, 46));
            m_annunciatorPanel->setGridColor(QColor(55, 65, 80));
            m_annunciatorPanel->setCriticalColor(QColor(235, 59, 90));
            m_annunciatorPanel->setWarningColor(QColor(254, 211, 48));
            m_annunciatorPanel->setAdvisoryColor(QColor(0, 229, 255));
            m_annunciatorPanel->setTextColor(QColor(240, 244, 250));
        }
        if (m_annunciatorHornLabel && m_annunciatorPanel && !m_annunciatorPanel->isAudibleHornActive()) {
            m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #a4b3c6;"));
        }

        if (m_compassHeadingUp) {
            m_compassHeadingUp->setDialColor(QColor(20, 24, 32));
            m_compassHeadingUp->setBezelColor(QColor(48, 56, 70));
            m_compassHeadingUp->setTextColor(QColor(225, 231, 236));
        }
        if (m_compassNorthUp) {
            m_compassNorthUp->setDialColor(QColor(20, 24, 32));
            m_compassNorthUp->setBezelColor(QColor(48, 56, 70));
            m_compassNorthUp->setTextColor(QColor(225, 231, 236));
        }
    } else {
        // Modern Clean Light SCADA Theme
        QString qss = QStringLiteral(
            "QMainWindow { background-color: #f0f2f5; }"
            "QWidget { color: #2c3e50; font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif; }"
            "QFrame#topBar { background-color: #ffffff; border-radius: 6px; border: 1px solid #dcdde1; }"
            "QTabWidget::pane { border: 1px solid #dcdde1; background: #ffffff; border-radius: 6px; }"
            "QTabBar::tab { background: #e8ecf1; color: #718093; padding: 8px 18px; margin-right: 4px; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; }"
            "QTabBar::tab:selected { background: #ffffff; color: #0984e3; border-bottom: 2px solid #0984e3; }"
            "QGroupBox { border: 1px solid #dcdde1; border-radius: 6px; margin-top: 1.1em; font-weight: bold; color: #2f3640; background-color: #ffffff; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }"
            "QPushButton { background-color: #f5f6fa; border: 1px solid #dcdde1; border-radius: 4px; color: #2f3640; padding: 6px 14px; font-weight: bold; }"
            "QPushButton:hover { background-color: #e8ecf1; border-color: #0984e3; }"
            "QPushButton:pressed { background-color: #dcdde1; }"
            "QSlider::groove:horizontal { border: 1px solid #dcdde1; height: 6px; background: #e1e7ec; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #0984e3; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #0984e3; width: 16px; margin-top: -5px; margin-bottom: -5px; border-radius: 8px; }"
        );
        setStyleSheet(qss);

        // Update widget colors for light mode
        m_rpmGauge->setDialColor(QColor(242, 244, 248));
        m_rpmGauge->setTextColor(QColor(30, 39, 46));
        m_rpmGauge->setScaleColor(QColor(80, 90, 105));
        m_rpmGauge->setBezelColor(QColor(200, 208, 218));

        m_boostGauge->setDialColor(QColor(242, 244, 248));
        m_boostGauge->setTextColor(QColor(30, 39, 46));
        m_boostGauge->setScaleColor(QColor(80, 90, 105));
        m_boostGauge->setBezelColor(QColor(200, 208, 218));

        m_oilGauge->setDialColor(QColor(242, 244, 248));
        m_oilGauge->setTextColor(QColor(30, 39, 46));
        m_oilGauge->setScaleColor(QColor(80, 90, 105));
        m_oilGauge->setBezelColor(QColor(200, 208, 218));

        m_steeringGauge->setDialColor(QColor(242, 244, 248));
        m_steeringGauge->setTextColor(QColor(30, 39, 46));
        m_steeringGauge->setScaleColor(QColor(80, 90, 105));
        m_steeringGauge->setBezelColor(QColor(200, 208, 218));

        m_coolantGauge->setBezelColor(QColor(215, 222, 230));
        m_coolantGauge->setTroughColor(QColor(235, 240, 245));
        m_coolantGauge->setTextColor(QColor(30, 39, 46));
        m_coolantGauge->setScaleColor(QColor(80, 90, 105));

        m_fuelGauge->setBezelColor(QColor(215, 222, 230));
        m_fuelGauge->setTroughColor(QColor(235, 240, 245));
        m_fuelGauge->setTextColor(QColor(30, 39, 46));
        m_fuelGauge->setScaleColor(QColor(80, 90, 105));

        m_hydraulicGauge->setBezelColor(QColor(215, 222, 230));
        m_hydraulicGauge->setTroughColor(QColor(235, 240, 245));
        m_hydraulicGauge->setTextColor(QColor(30, 39, 46));
        m_hydraulicGauge->setScaleColor(QColor(80, 90, 105));

        m_speedDisplay->setBackgroundColor(QColor(230, 236, 242));
        m_speedDisplay->setBezelColor(QColor(190, 200, 210));
        m_speedDisplay->setActiveSegmentColor(QColor(0, 130, 200));
        m_speedDisplay->setInactiveSegmentColor(QColor(0, 130, 200, 25));

        m_voltageDisplay->setBackgroundColor(QColor(230, 236, 242));
        m_voltageDisplay->setBezelColor(QColor(190, 200, 210));
        m_voltageDisplay->setActiveSegmentColor(QColor(24, 150, 75));
        m_voltageDisplay->setInactiveSegmentColor(QColor(24, 150, 75, 25));

        m_timerDisplay->setBackgroundColor(QColor(230, 236, 242));
        m_timerDisplay->setBezelColor(QColor(190, 200, 210));
        m_timerDisplay->setActiveSegmentColor(QColor(210, 140, 0));
        m_timerDisplay->setInactiveSegmentColor(QColor(210, 140, 0, 25));

        if (m_throttleKnob) {
            m_throttleKnob->setKnobColor(QColor(220, 226, 235));
            m_throttleKnob->setScaleColor(QColor(70, 80, 95));
            m_throttleKnob->setTextColor(QColor(30, 39, 46));
        }
        if (m_boostKnob) {
            m_boostKnob->setKnobColor(QColor(220, 226, 235));
            m_boostKnob->setScaleColor(QColor(70, 80, 95));
            m_boostKnob->setTextColor(QColor(30, 39, 46));
        }
        if (m_modeSelectorKnob) {
            m_modeSelectorKnob->setKnobColor(QColor(220, 226, 235));
            m_modeSelectorKnob->setScaleColor(QColor(70, 80, 95));
            m_modeSelectorKnob->setTextColor(QColor(30, 39, 46));
        }

        if (m_stripChart) {
            m_stripChart->setBackgroundColor(QColor(242, 246, 252));
            m_stripChart->setGridColor(QColor(208, 218, 230));
            m_stripChart->setBezelColor(QColor(215, 222, 230));
        }

        if (m_annunciatorPanel) {
            m_annunciatorPanel->setFrameColor(QColor(215, 222, 230));
            m_annunciatorPanel->setGridColor(QColor(180, 190, 205));
            m_annunciatorPanel->setCriticalColor(QColor(235, 59, 90));
            m_annunciatorPanel->setWarningColor(QColor(243, 156, 18));
            m_annunciatorPanel->setAdvisoryColor(QColor(9, 132, 227));
            m_annunciatorPanel->setTextColor(QColor(30, 39, 46));
        }
        if (m_annunciatorHornLabel && m_annunciatorPanel && !m_annunciatorPanel->isAudibleHornActive()) {
            m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #e8ecf1; border-radius: 4px; color: #718093;"));
        }

        if (m_compassHeadingUp) {
            m_compassHeadingUp->setDialColor(QColor(242, 244, 248));
            m_compassHeadingUp->setBezelColor(QColor(200, 208, 218));
            m_compassHeadingUp->setTextColor(QColor(30, 39, 46));
        }
        if (m_compassNorthUp) {
            m_compassNorthUp->setDialColor(QColor(242, 244, 248));
            m_compassNorthUp->setBezelColor(QColor(200, 208, 218));
            m_compassNorthUp->setTextColor(QColor(30, 39, 46));
        }
    }
}
