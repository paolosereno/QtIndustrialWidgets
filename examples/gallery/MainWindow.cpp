// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/SevenSegmentDisplay.h>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/StripChart.h>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtIndustrialWidgets/AnnunciatorPanel.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/AnalysisChart.h>

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
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QMetaEnum>
#include <QtCore/QSignalBlocker>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtGui/QImageWriter>
#include <QtCore/QtMath>

#include <array>
#include <limits>

using namespace QtIndustrialWidgets;


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
    topLayout->setSpacing(8); // at 1200x820 the LED labels need the room (#26)

    auto *titleLabel = new QLabel(QStringLiteral("<b>QtIndustrialWidgets</b> &nbsp;|&nbsp; Instrumentation Gallery"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleLabel->setFont(titleFont);

    m_fpsLabel = new QLabel(QStringLiteral("FPS: --"), this);
    m_fpsLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace;"));

    // Annunciator Status LEDs
    m_runLed = new LedIndicator(QColor(46, 204, 113), topBar);
    m_runLed->setLabelText(QStringLiteral("RUN"));
    m_runLed->setOn(false);

    m_warnLed = new LedIndicator(QColor(254, 211, 48), topBar);
    m_warnLed->setLabelText(QStringLiteral("WARN"));
    m_warnLed->setOn(false);

    m_alarmLed = new LedIndicator(QColor(235, 59, 90), topBar);
    m_alarmLed->setLabelText(QStringLiteral("FAULT"));
    m_alarmLed->setOn(false);
    m_alarmLed->setBlinkRateMs(180);

    m_pumpLed = new LedIndicator(QColor(0, 229, 255), topBar);
    m_pumpLed->setLabelText(QStringLiteral("AUX PUMP"));
    m_pumpLed->setShape(LedIndicator::LedShape::Rectangular);
    m_pumpLed->setOn(false);

    m_simButton = new QPushButton(QStringLiteral("Start Simulation"), this);
    m_simButton->setCursor(Qt::PointingHandCursor);
    m_simButton->setMinimumHeight(32);
    connect(m_simButton, &QPushButton::clicked, this, &MainWindow::toggleSimulation);

    m_tourButton = new QPushButton(QStringLiteral("Auto-Tour"), this);
    m_tourButton->setCheckable(true);
    m_tourButton->setCursor(Qt::PointingHandCursor);
    m_tourButton->setMinimumHeight(32);
    m_tourButton->setToolTip(QStringLiteral("Cycle automatically between tabs every 3.5 seconds with a smooth transition"));
    connect(m_tourButton, &QPushButton::clicked, this, &MainWindow::toggleAutoTour);

    m_themeButton = new QPushButton(QStringLiteral("Light Theme"), this);
    m_themeButton->setCursor(Qt::PointingHandCursor);
    m_themeButton->setMinimumHeight(32);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    topLayout->addWidget(titleLabel);
    topLayout->addWidget(m_runLed);
    topLayout->addWidget(m_warnLed);
    topLayout->addWidget(m_alarmLed);
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
    auto *radialGroup = new QGroupBox(QStringLiteral("Powertrain && Dynamics (Radial Gauges)"), dashTab);
    auto *radialLayout = new QHBoxLayout(radialGroup);
    radialLayout->setContentsMargins(12, 16, 12, 12);
    radialLayout->setSpacing(14);

    // RPM Gauge (0 - 8000 RPM)
    m_rpmGauge = new RadialGauge(radialGroup);
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
    m_boostGauge = new RadialGauge(radialGroup);
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
    m_oilGauge = new RadialGauge(radialGroup);
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
    m_steeringGauge = new RadialGauge(radialGroup);
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
    auto *linearGroup = new QGroupBox(QStringLiteral("Thermal && Fluid Systems (Linear Gauges)"), dashTab);
    auto *linearLayout = new QHBoxLayout(linearGroup);
    linearLayout->setContentsMargins(12, 16, 12, 12);
    linearLayout->setSpacing(14);

    // Coolant Thermometer (with bulb mode)
    m_coolantGauge = new LinearGauge(linearGroup);
    m_coolantGauge->setOrientation(Qt::Vertical);
    m_coolantGauge->setThermometerMode(true);
    m_coolantGauge->setRange(0.0, 120.0);
    m_coolantGauge->setValue(88.0);
    m_coolantGauge->setUnit(QStringLiteral("°C"));
    m_coolantGauge->setPrecision(1);
    m_coolantGauge->setWarningThreshold(95.0);
    m_coolantGauge->setErrorThreshold(108.0);

    // Fuel Tank Gauge (rectangular bar mode)
    m_fuelGauge = new LinearGauge(linearGroup);
    m_fuelGauge->setOrientation(Qt::Vertical);
    m_fuelGauge->setThermometerMode(false);
    m_fuelGauge->setRange(0.0, 100.0);
    m_fuelGauge->setValue(65.0);
    m_fuelGauge->setUnit(QStringLiteral("%"));
    m_fuelGauge->setPrecision(0);
    m_fuelGauge->setWarningThreshold(75.0);
    m_fuelGauge->setErrorThreshold(90.0);

    // Hydraulic Line Pressure (Horizontal)
    m_hydraulicGauge = new LinearGauge(linearGroup);
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

    // Dynamic Vibration Dual-Channel Meter (LevelMeter)
    m_vibrationMeter = new LevelMeter(linearGroup);
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
    m_speedDisplay = new SevenSegmentDisplay(digitalGroup);
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
    m_voltageDisplay = new SevenSegmentDisplay(digitalGroup);
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
    m_timerDisplay = new SevenSegmentDisplay(digitalGroup);
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
    auto *chartGroup = new QGroupBox(QStringLiteral("Real-Time Telemetry (StripChart)"), dashTab);
    auto *chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->setContentsMargins(10, 16, 10, 10);

    m_stripChart = new StripChart(chartGroup);
    m_stripChart->setCapacity(300);
    m_stripChart->setYRange(0.0, 120.0);
    m_chRpm = m_stripChart->addChannel(QStringLiteral("RPM %"), QColor(0, 229, 255), 2.0);
    m_chBoost = m_stripChart->addChannel(QStringLiteral("Boost x35"), QColor(235, 59, 90), 2.0);
    m_chTemp = m_stripChart->addChannel(QStringLiteral("Temp °C"), QColor(46, 204, 113), 2.0);

    chartLayout->addWidget(m_stripChart);
    lowerRowLayout->addWidget(chartGroup, 4);

    dashLayout->addLayout(lowerRowLayout, 3);

    tabWidget->addTab(dashTab, QStringLiteral("Dashboard"));

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
    // Precision Rotary Knobs Showcase Group (IndustrialKnob)
    // ------------------------------------------------------------------------
    auto *knobBox = new QGroupBox(QStringLiteral("Precision Rotary Controls (IndustrialKnob)"), controlsTab);
    auto *knobLayout = new QHBoxLayout(knobBox);
    knobLayout->setSpacing(24);
    knobLayout->setContentsMargins(16, 18, 16, 14);

    // Throttle / Target RPM Knob (Continuous)
    m_throttleKnob = new IndustrialKnob(knobBox);
    m_throttleKnob->setRange(0.0, 8000.0);
    m_throttleKnob->setValue(2400.0);
    m_throttleKnob->setUnit(QStringLiteral("RPM"));
    m_throttleKnob->setStep(50.0);
    m_throttleKnob->setPrecision(0);
    m_throttleKnob->setMajorTicks(8);
    m_throttleKnob->setMinorTicks(4);

    // Boost Regulator Knob (Continuous)
    m_boostKnob = new IndustrialKnob(knobBox);
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
    m_modeSelectorKnob = new IndustrialKnob(knobBox);
    m_modeSelectorKnob->setMode(IndustrialKnob::KnobMode::Discrete);
    m_modeSelectorKnob->setDiscreteSteps(4);
    m_modeSelectorKnob->setRange(1.0, 4.0);
    m_modeSelectorKnob->setValue(2.0);
    m_modeSelectorKnob->setUnit(QStringLiteral("MODE"));
    m_modeSelectorKnob->setPointerColor(QColor(46, 204, 113));
    m_modeSelectorKnob->setTrackColor(QColor(46, 204, 113));

    // Synchronize throttle knob with slider and gauge
    connect(m_throttleKnob, &IndustrialKnob::valueChanged, this, [this](double val) {
        if (!m_isSimulating) {
            m_rpmSlider->setValue(static_cast<int>(val));
            m_rpmGauge->setValue(val);
        }
    });

    // Synchronize boost knob with slider and gauge
    connect(m_boostKnob, &IndustrialKnob::valueChanged, this, [this](double val) {
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
    // Interactive LED testing group (LedIndicator)
    // ------------------------------------------------------------------------
    auto *ledBox = new QGroupBox(QStringLiteral("Interactive LedIndicator Showcase (Click on LEDs to Toggle)"), controlsTab);
    auto *ledLayout = new QHBoxLayout(ledBox);
    ledLayout->setSpacing(20);

    auto *testLed1 = new LedIndicator(QColor(46, 204, 113), ledBox);
    testLed1->setLabelText(QStringLiteral("Green (Click Me)"));
    testLed1->setClickable(true);

    auto *testLed2 = new LedIndicator(QColor(235, 59, 90), ledBox);
    testLed2->setLabelText(QStringLiteral("Red Blinking (2 Hz)"));
    testLed2->setBlinking(true);
    testLed2->setBlinkRateMs(250);
    testLed2->setClickable(true);

    auto *testLed3 = new LedIndicator(QColor(254, 211, 48), ledBox);
    testLed3->setLabelText(QStringLiteral("Amber Rectangular"));
    testLed3->setShape(LedIndicator::LedShape::Rectangular);
    testLed3->setClickable(true);

    auto *testLed4 = new LedIndicator(QColor(0, 229, 255), ledBox);
    testLed4->setLabelText(QStringLiteral("Cyan Rectangular"));
    testLed4->setShape(LedIndicator::LedShape::Rectangular);
    testLed4->setClickable(true);

    ledLayout->addWidget(testLed1);
    ledLayout->addWidget(testLed2);
    ledLayout->addWidget(testLed3);
    ledLayout->addWidget(testLed4);
    ledLayout->addStretch();

    controlsLayout->addWidget(ledBox, row, 0, 1, 3);
    row++;

    // ------------------------------------------------------------------------
    // Heavy-Duty Industrial Switches Showcase Group (IndustrialSwitch)
    // ------------------------------------------------------------------------
    auto *switchBox = new QGroupBox(QStringLiteral("Heavy-Duty Industrial Switches (IndustrialSwitch)"), controlsTab);
    auto *switchLayout = new QHBoxLayout(switchBox);
    switchLayout->setSpacing(28);
    switchLayout->setContentsMargins(18, 18, 18, 14);

    // 1. Classic Main Power Toggle Switch
    m_powerSwitch = new IndustrialSwitch(switchBox);
    m_powerSwitch->setLabel(QStringLiteral("MAIN PWR"));
    m_powerSwitch->setLabelOn(QStringLiteral("ON"));
    m_powerSwitch->setLabelOff(QStringLiteral("OFF"));
    m_powerSwitch->setChecked(false);
    connect(m_powerSwitch, &IndustrialSwitch::toggled, this, [this](bool on) {
        if (on != m_isSimulating) {
            toggleSimulation();
        }
    });

    // 2. High-Risk Safety Guard Switch (Red flip-up cover)
    m_safetySwitch = new IndustrialSwitch(switchBox);
    m_safetySwitch->setLabel(QStringLiteral("EMERGENCY"));
    m_safetySwitch->setLabelOn(QStringLiteral("ARMED"));
    m_safetySwitch->setLabelOff(QStringLiteral("SAFE"));
    m_safetySwitch->setHasSafetyGuard(true);
    m_safetySwitch->setGuardColor(QColor(220, 53, 69));
    m_safetySwitch->setLedColor(QColor(235, 59, 90));
    connect(m_safetySwitch, &IndustrialSwitch::toggled, this, [this](bool armed) {
        if (m_alarmLed) {
            m_alarmLed->setOn(armed);
            m_alarmLed->setBlinking(armed);
        }
    });

    // 3. 3-Position Mode Selector (MANUAL / OFF / AUTO)
    m_modeSwitch = new IndustrialSwitch(switchBox);
    m_modeSwitch->setPositionCount(3);
    m_modeSwitch->setLabel(QStringLiteral("SYS MODE"));
    m_modeSwitch->setLabelOff(QStringLiteral("MAN"));
    m_modeSwitch->setLabelCenter(QStringLiteral("OFF"));
    m_modeSwitch->setLabelOn(QStringLiteral("AUTO"));
    m_modeSwitch->setPosition(1);
    m_modeSwitch->setLedColor(QColor(254, 211, 48));

    // 4. Industrial Rocker Switch (Cooling Fan)
    m_rockerSwitch = new IndustrialSwitch(switchBox);
    m_rockerSwitch->setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    m_rockerSwitch->setLabel(QStringLiteral("COOLING"));
    m_rockerSwitch->setLabelOn(QStringLiteral("HIGH"));
    m_rockerSwitch->setLabelOff(QStringLiteral("LOW"));
    m_rockerSwitch->setLedColor(QColor(0, 229, 255));
    m_rockerSwitch->setChecked(false);
    connect(m_rockerSwitch, &IndustrialSwitch::toggled, this, [this](bool on) {
        if (m_pumpLed) {
            m_pumpLed->setOn(on);
        }
    });

    // 5. Horizontal Toggle Switch (Bus Tie Feed)
    auto *horizSwitch = new IndustrialSwitch(switchBox);
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
    // Multi-Channel VU & Level Meter Showcase (LevelMeter)
    // ------------------------------------------------------------------------
    auto *meterBox = new QGroupBox(QStringLiteral("Acoustic && Signal Level Meters (LevelMeter)"), controlsTab);
    auto *meterLayout = new QHBoxLayout(meterBox);
    meterLayout->setSpacing(24);
    meterLayout->setContentsMargins(18, 18, 18, 14);

    // 1. Classic Studio Stereo VU Meter (-60 dB to +6 dB)
    m_audioVuMeter = new LevelMeter(meterBox);
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
    auto *contMeter = new LevelMeter(meterBox);
    contMeter->setChannelCount(1);
    contMeter->setDisplayMode(LevelMeter::DisplayMode::Continuous);
    contMeter->setTitle(QStringLiteral("LINE RMS"));
    contMeter->setChannelLabels({QStringLiteral("LINE")});
    contMeter->setRange(0.0, 100.0);
    contMeter->setWarningThreshold(75.0);
    contMeter->setErrorThreshold(90.0);
    contMeter->setUnit(QStringLiteral("%"));
    contMeter->setValue(62.0);

    // 3. Horizontal Level Meter
    auto *horizMeter = new LevelMeter(meterBox);
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
    // The tab is taller than the default window: scroll instead of squashing the widgets.
    // A Minimum vertical policy keeps the content at least at its size hint.
    controlsTab->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *controlsScroll = new QScrollArea(tabWidget);
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setWidget(controlsTab);
    // setWidget() turns on autoFillBackground; keep the themed tab pane visible behind the content
    controlsTab->setAutoFillBackground(false);
    controlsScroll->viewport()->setAutoFillBackground(false);
    tabWidget->addTab(controlsScroll, QStringLiteral("Manual Controls"));

    // ------------------------------------------------------------------------
    // TAB 3: Circular Arc LED Bargraph Showcase (CircularBargraph)
    // ------------------------------------------------------------------------
    auto *bargraphTab = new QWidget(tabWidget);
    auto *bargraphLayout = new QVBoxLayout(bargraphTab);
    bargraphLayout->setContentsMargins(20, 20, 20, 20);
    bargraphLayout->setSpacing(14);

    auto *circBox = new QGroupBox(QStringLiteral("Circular Arc LED Indicators (CircularBargraph)"), bargraphTab);
    auto *circMainLayout = new QVBoxLayout(circBox);
    circMainLayout->setContentsMargins(16, 16, 16, 14);
    circMainLayout->setSpacing(12);

    auto *circWidgetsLayout = new QHBoxLayout();
    circWidgetsLayout->setSpacing(24);

    // 1. 270° Bar Mode with Digital Readout
    m_circBargraphBar = new CircularBargraph(circBox);
    m_circBargraphBar->setRange(0.0, 100.0);
    m_circBargraphBar->setValue(65.0);
    m_circBargraphBar->setSegmentCount(24);
    m_circBargraphBar->setStartAngle(-135.0);
    m_circBargraphBar->setSpanAngle(270.0);
    m_circBargraphBar->setDisplayMode(CircularBargraph::DisplayMode::Bar);
    m_circBargraphBar->setWarningThreshold(70.0);
    m_circBargraphBar->setErrorThreshold(85.0);
    m_circBargraphBar->setPeakHoldEnabled(true);
    m_circBargraphBar->setUnit(QStringLiteral("%"));

    // 2. 270° Dot Mode (Single Cursor Pointer)
    m_circBargraphDot = new CircularBargraph(circBox);
    m_circBargraphDot->setRange(0.0, 100.0);
    m_circBargraphDot->setValue(65.0);
    m_circBargraphDot->setSegmentCount(32);
    m_circBargraphDot->setStartAngle(-135.0);
    m_circBargraphDot->setSpanAngle(270.0);
    m_circBargraphDot->setDisplayMode(CircularBargraph::DisplayMode::Dot);
    m_circBargraphDot->setWarningThreshold(70.0);
    m_circBargraphDot->setErrorThreshold(85.0);
    m_circBargraphDot->setPeakHoldEnabled(true);
    m_circBargraphDot->setUnit(QStringLiteral("DOT"));

    // 3. 180° Semicircular Sweep (16 Segments)
    m_circBargraphSemi = new CircularBargraph(circBox);
    m_circBargraphSemi->setRange(0.0, 100.0);
    m_circBargraphSemi->setValue(65.0);
    m_circBargraphSemi->setSegmentCount(16);
    m_circBargraphSemi->setStartAngle(-90.0);
    m_circBargraphSemi->setSpanAngle(180.0);
    m_circBargraphSemi->setDisplayMode(CircularBargraph::DisplayMode::Bar);
    m_circBargraphSemi->setWarningThreshold(70.0);
    m_circBargraphSemi->setErrorThreshold(85.0);
    m_circBargraphSemi->setPeakHoldEnabled(true);
    m_circBargraphSemi->setUnit(QStringLiteral("SEMI"));

    // The dial is painted in min(width, height) - 8 px, so 200x200 keeps it >= 180 px across
    for (CircularBargraph *bg : {m_circBargraphBar, m_circBargraphDot, m_circBargraphSemi}) {
        bg->setMinimumSize(200, 200);
        bg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        circWidgetsLayout->addWidget(bg, 1);
    }
    circMainLayout->addLayout(circWidgetsLayout, 1);

    // Controls row: slider + reset peak + mode toggle
    auto *circCtrlLayout = new QHBoxLayout();
    circCtrlLayout->setSpacing(12);

    // The level slider is in absolute scale units (0 - 100): narrowing the range below shows
    // Under/Overrange
    auto *sliderLbl = new QLabel(QStringLiteral("Level (absolute):"), circBox);
    m_circBargraphSlider = new QSlider(Qt::Horizontal, circBox);
    m_circBargraphSlider->setRange(0, 100);
    m_circBargraphSlider->setValue(65);
    connect(m_circBargraphSlider, &QSlider::valueChanged, this, [this](int val) {
        if (m_circInputCombo) m_circInputCombo->setCurrentIndex(0); // manual level -> Live input
        if (m_circBargraphBar) m_circBargraphBar->setValue(val);
        if (m_circBargraphDot) m_circBargraphDot->setValue(val);
        if (m_circBargraphSemi) m_circBargraphSemi->setValue(val);
    });

    // Value state demo: force an out-of-range or invalid input (the simulation leaves the
    // bargraphs alone while a forced input is selected)
    auto *inputLbl = new QLabel(QStringLiteral("Input:"), circBox);
    m_circInputCombo = new QComboBox(circBox);
    m_circInputCombo->addItem(QStringLiteral("Live"));
    m_circInputCombo->addItem(QStringLiteral("Overrange (max + 10)"));
    m_circInputCombo->addItem(QStringLiteral("Underrange (min - 10)"));
    m_circInputCombo->addItem(QStringLiteral("Invalid (NaN)"));
    connect(m_circInputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        for (CircularBargraph *bg : {m_circBargraphBar, m_circBargraphDot, m_circBargraphSemi}) {
            if (!bg) continue;
            switch (index) {
            case 1: bg->setValue(bg->maximum() + 10.0); break;
            case 2: bg->setValue(bg->minimum() - 10.0); break;
            case 3: bg->setValue(std::numeric_limits<double>::quiet_NaN()); break;
            default:
                if (m_circBargraphSlider) bg->setValue(m_circBargraphSlider->value());
                break;
            }
        }
    });

    m_circStateLabel = new QLabel(circBox);
    m_circStateLabel->setMinimumWidth(260);
    const auto updateCircStateLabel = [this]() {
        if (!m_circBargraphBar || !m_circStateLabel) return;
        const char *stateName = QMetaEnum::fromType<CircularBargraph::ValueState>()
                                    .valueToKey(static_cast<int>(m_circBargraphBar->valueState()));
        m_circStateLabel->setText(QStringLiteral("State: %1 | raw %2 | value %3 | warning %4 / error %5 | range %6 - %7")
                                      .arg(QString::fromLatin1(stateName))
                                      .arg(m_circBargraphBar->rawValue(), 0, 'f', 1)
                                      .arg(m_circBargraphBar->value(), 0, 'f', 1)
                                      .arg(m_circBargraphBar->warningThreshold(), 0, 'f', 0)
                                      .arg(m_circBargraphBar->errorThreshold(), 0, 'f', 0)
                                      .arg(m_circBargraphBar->minimum(), 0, 'f', 0)
                                      .arg(m_circBargraphBar->maximum(), 0, 'f', 0));
    };
    // All three bargraphs get the same settings, so the Bar one alone drives the label
    connect(m_circBargraphBar, &CircularBargraph::valueStateChanged, this, updateCircStateLabel);
    connect(m_circBargraphBar, &CircularBargraph::valueChanged, this, updateCircStateLabel);
    connect(m_circBargraphBar, &CircularBargraph::thresholdChanged, this, updateCircStateLabel);
    connect(m_circBargraphBar, &CircularBargraph::rangeChanged, this, updateCircStateLabel);
    updateCircStateLabel();

    auto *resetPeakBtn = new QPushButton(QStringLiteral("Reset Peak"), circBox);
    connect(resetPeakBtn, &QPushButton::clicked, this, [this]() {
        if (m_circBargraphBar) m_circBargraphBar->resetPeak();
        if (m_circBargraphDot) m_circBargraphDot->resetPeak();
        if (m_circBargraphSemi) m_circBargraphSemi->resetPeak();
    });

    auto *peakCheck = new QCheckBox(QStringLiteral("Peak Hold"), circBox);
    peakCheck->setChecked(true);
    connect(peakCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_circBargraphBar) m_circBargraphBar->setPeakHoldEnabled(on);
        if (m_circBargraphDot) m_circBargraphDot->setPeakHoldEnabled(on);
        if (m_circBargraphSemi) m_circBargraphSemi->setPeakHoldEnabled(on);
    });

    circCtrlLayout->addWidget(sliderLbl);
    circCtrlLayout->addWidget(m_circBargraphSlider, 1);
    circCtrlLayout->addWidget(peakCheck);
    circCtrlLayout->addWidget(resetPeakBtn);
    circMainLayout->addLayout(circCtrlLayout);

    auto *circStateLayout = new QHBoxLayout();
    circStateLayout->setSpacing(12);
    circStateLayout->addWidget(inputLbl);
    circStateLayout->addWidget(m_circInputCombo);
    circStateLayout->addWidget(m_circStateLabel, 1);
    circMainLayout->addLayout(circStateLayout);

    bargraphLayout->addWidget(circBox, 1);

    // Feature settings: every control acts on all three bargraphs
    auto *circSettingsBox = new QGroupBox(QStringLiteral("Segments, Zones, Peak and Range"), bargraphTab);
    auto *circSettingsLayout = new QGridLayout(circSettingsBox);
    circSettingsLayout->setContentsMargins(16, 16, 16, 14);
    circSettingsLayout->setHorizontalSpacing(12);
    circSettingsLayout->setVerticalSpacing(10);
    const auto allBargraphs = [this]() {
        return std::array<CircularBargraph *, 3>{m_circBargraphBar, m_circBargraphDot, m_circBargraphSemi};
    };

    // Only widgets the gallery theme already styles (sliders, radio buttons) are used here,
    // so the controls stay readable in both themes

    // Segment count ("Default" restores the per-bargraph counts set above)
    auto *segmentsLayout = new QHBoxLayout();
    segmentsLayout->setSpacing(16);
    auto *segmentsGroup = new QButtonGroup(circSettingsBox);
    for (int count : {0, 12, 24, 48}) {
        auto *radio = new QRadioButton(count > 0 ? QString::number(count) : QStringLiteral("Default (24 / 32 / 16)"),
                                       circSettingsBox);
        radio->setObjectName(QStringLiteral("circSegments%1").arg(count));
        radio->setChecked(count == 0);
        segmentsGroup->addButton(radio, count);
        segmentsLayout->addWidget(radio);
    }
    segmentsLayout->addStretch();
    connect(segmentsGroup, &QButtonGroup::idClicked, this, [this](int count) {
        m_circBargraphBar->setSegmentCount(count > 0 ? count : 24);
        m_circBargraphDot->setSegmentCount(count > 0 ? count : 32);
        m_circBargraphSemi->setSegmentCount(count > 0 ? count : 16);
    });

    const auto addSettingSlider = [circSettingsBox](const QString &objectName, int minVal, int maxVal, int value) {
        auto *slider = new QSlider(Qt::Horizontal, circSettingsBox);
        slider->setObjectName(objectName);
        slider->setRange(minVal, maxVal);
        slider->setValue(value);
        return slider;
    };
    const auto addValueLabel = [circSettingsBox](const QString &text) {
        auto *label = new QLabel(text, circSettingsBox);
        label->setMinimumWidth(80);
        return label;
    };

    // Thresholds: setting one may push the other (warning <= error); the sliders follow
    // thresholdChanged() of the Bar bargraph
    auto *warnSlider = addSettingSlider(QStringLiteral("circWarningSlider"), 0, 100,
                                        qRound(m_circBargraphBar->warningThreshold()));
    auto *errorSlider = addSettingSlider(QStringLiteral("circErrorSlider"), 0, 100,
                                         qRound(m_circBargraphBar->errorThreshold()));
    auto *warnValLbl = addValueLabel(QString::number(warnSlider->value()));
    auto *errorValLbl = addValueLabel(QString::number(errorSlider->value()));
    connect(warnSlider, &QSlider::valueChanged, this, [allBargraphs](int val) {
        for (CircularBargraph *bg : allBargraphs()) bg->setWarningThreshold(val);
    });
    connect(errorSlider, &QSlider::valueChanged, this, [allBargraphs](int val) {
        for (CircularBargraph *bg : allBargraphs()) bg->setErrorThreshold(val);
    });
    connect(m_circBargraphBar, &CircularBargraph::thresholdChanged, this,
            [warnSlider, errorSlider, warnValLbl, errorValLbl](double warning, double error) {
        const QSignalBlocker blockWarn(warnSlider);
        const QSignalBlocker blockError(errorSlider);
        warnSlider->setValue(qRound(warning));
        errorSlider->setValue(qRound(error));
        warnValLbl->setText(QString::number(warning, 'f', 0));
        errorValLbl->setText(QString::number(error, 'f', 0));
    });

    // Range: setRange() rejects min >= max, so both sliders are re-read from the Bar
    // bargraph after every change
    auto *minSlider = addSettingSlider(QStringLiteral("circMinimumSlider"), 0, 99, qRound(m_circBargraphBar->minimum()));
    auto *maxSlider = addSettingSlider(QStringLiteral("circMaximumSlider"), 1, 100, qRound(m_circBargraphBar->maximum()));
    auto *minValLbl = addValueLabel(QString::number(minSlider->value()));
    auto *maxValLbl = addValueLabel(QString::number(maxSlider->value()));
    const auto applyRange = [this, allBargraphs, minSlider, maxSlider, minValLbl, maxValLbl]() {
        for (CircularBargraph *bg : allBargraphs()) bg->setRange(minSlider->value(), maxSlider->value());
        const QSignalBlocker blockMin(minSlider);
        const QSignalBlocker blockMax(maxSlider);
        minSlider->setValue(qRound(m_circBargraphBar->minimum()));
        maxSlider->setValue(qRound(m_circBargraphBar->maximum()));
        minValLbl->setText(QString::number(minSlider->value()));
        maxValLbl->setText(QString::number(maxSlider->value()));
    };
    connect(minSlider, &QSlider::valueChanged, this, applyRange);
    connect(maxSlider, &QSlider::valueChanged, this, applyRange);

    // Peak hold time and decay rate
    auto *holdSlider = addSettingSlider(QStringLiteral("circPeakHoldSlider"), 0, 5000, m_circBargraphBar->peakHoldTimeMs());
    holdSlider->setSingleStep(100);
    holdSlider->setPageStep(500);
    auto *holdValLbl = addValueLabel(QStringLiteral("%1 ms").arg(holdSlider->value()));
    connect(holdSlider, &QSlider::valueChanged, this, [allBargraphs, holdValLbl](int ms) {
        for (CircularBargraph *bg : allBargraphs()) bg->setPeakHoldTimeMs(ms);
        holdValLbl->setText(QStringLiteral("%1 ms").arg(ms));
    });

    auto *decaySlider = addSettingSlider(QStringLiteral("circPeakDecaySlider"), 1, 200,
                                         qRound(m_circBargraphBar->peakDecayRate()));
    auto *decayValLbl = addValueLabel(QStringLiteral("%1 units/s").arg(decaySlider->value()));
    connect(decaySlider, &QSlider::valueChanged, this, [allBargraphs, decayValLbl](int rate) {
        for (CircularBargraph *bg : allBargraphs()) bg->setPeakDecayRate(rate);
        decayValLbl->setText(QStringLiteral("%1 units/s").arg(rate));
    });

    const auto addSettingRow = [circSettingsBox, circSettingsLayout](int gridRow, int column, const QString &label,
                                                                      QSlider *slider, QLabel *valueLabel) {
        circSettingsLayout->addWidget(new QLabel(label, circSettingsBox), gridRow, column);
        circSettingsLayout->addWidget(slider, gridRow, column + 1);
        circSettingsLayout->addWidget(valueLabel, gridRow, column + 2);
    };
    circSettingsLayout->addWidget(new QLabel(QStringLiteral("Segments:"), circSettingsBox), 0, 0);
    circSettingsLayout->addLayout(segmentsLayout, 0, 1, 1, 5);
    addSettingRow(1, 0, QStringLiteral("Warning threshold:"), warnSlider, warnValLbl);
    addSettingRow(2, 0, QStringLiteral("Error threshold:"), errorSlider, errorValLbl);
    addSettingRow(3, 0, QStringLiteral("Peak hold time:"), holdSlider, holdValLbl);
    addSettingRow(1, 3, QStringLiteral("Range min:"), minSlider, minValLbl);
    addSettingRow(2, 3, QStringLiteral("Range max:"), maxSlider, maxValLbl);
    addSettingRow(3, 3, QStringLiteral("Peak decay rate:"), decaySlider, decayValLbl);
    circSettingsLayout->setColumnStretch(1, 1);
    circSettingsLayout->setColumnStretch(4, 1);

    bargraphLayout->addWidget(circSettingsBox);
    tabWidget->addTab(bargraphTab, QStringLiteral("Circular Bargraph"));

    // ------------------------------------------------------------------------
    // TAB 4: Alarm Annunciator Matrix (ANSI/ISA-18.1)
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

    auto *annTitleLbl = new QLabel(QStringLiteral("<b>Alarm Annunciator Window Matrix (ISA-18.1 Sequences A & M)</b>"), annHeaderBox);
    annTitleLbl->setStyleSheet(QStringLiteral("font-size: 13px;"));

    m_annunciatorHornLabel = new QLabel(QStringLiteral("HORN: SILENT"), annHeaderBox);
    m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #a4b3c6;"));

    m_annunciatorStatusLabel = new QLabel(QStringLiteral("Active: 0 | Unack: 0 | Normal: 12"), annHeaderBox);
    m_annunciatorStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #00e5ff;"));

    annHeaderLayout->addWidget(annTitleLbl);
    annHeaderLayout->addStretch();
    annHeaderLayout->addWidget(m_annunciatorHornLabel);
    annHeaderLayout->addWidget(m_annunciatorStatusLabel);
    annLayout->addWidget(annHeaderBox);

    // The Central Matrix Panel (3 rows x 4 columns = 12 windows)
    m_annunciatorPanel = new AnnunciatorPanel(3, 4, annunciatorTab);
    m_annunciatorPanel->setMinimumHeight(320);

    const struct {
        int row;
        int col;
        const char *text;
        AnnunciatorPanel::Severity severity;
    } tileConfigs[] = {
        {0, 0, "TURBINE 1\nOVERSPEED TRIP", AnnunciatorPanel::Severity::Critical},
        {0, 1, "MAIN STEAM\nPRESS HIGH", AnnunciatorPanel::Severity::Critical},
        {0, 2, "BEARING OIL\nPRESS LOW", AnnunciatorPanel::Severity::Critical},
        {0, 3, "GENERATOR\nLOCKOUT TRIP", AnnunciatorPanel::Severity::Critical},

        {1, 0, "FEEDWATER PUMP\nTRIP FAULT", AnnunciatorPanel::Severity::Warning},
        {1, 1, "CONDENSER\nVACUUM LOW", AnnunciatorPanel::Severity::Warning},
        {1, 2, "TRANSFORMER\nTEMP HIGH", AnnunciatorPanel::Severity::Warning},
        {1, 3, "MAIN STEAM\nTEMP HIGH", AnnunciatorPanel::Severity::Warning},

        {2, 0, "FIRE SUPPRESSION\nDISCHARGED", AnnunciatorPanel::Severity::Critical},
        {2, 1, "AUX DIESEL GEN\nRUNNING", AnnunciatorPanel::Severity::Advisory},
        {2, 2, "UPS BATTERY\nON INVERTER", AnnunciatorPanel::Severity::Advisory},
        {2, 3, "SCADA TELEMETRY\nLINK OFFLINE", AnnunciatorPanel::Severity::Warning}
    };

    for (const auto &cfg : tileConfigs) {
        m_annunciatorPanel->setTileText(cfg.row, cfg.col, QString::fromLatin1(cfg.text));
        m_annunciatorPanel->setTileSeverity(cfg.row, cfg.col, cfg.severity);
    }

    annLayout->addWidget(m_annunciatorPanel, 1);

    // Control and Simulator Controls Panel (2 columns of QGroupBox)
    auto *annControlsRow = new QHBoxLayout();
    annControlsRow->setSpacing(14);

    // Operator Pushbuttons (ISA-18.1 Sequences A & M)
    auto *operatorBox = new QGroupBox(QStringLiteral("Operator Pushbuttons (ISA-18.1 Sequences A && M)"), annunciatorTab);
    auto *operatorLayout = new QHBoxLayout(operatorBox);
    operatorLayout->setSpacing(12);
    operatorLayout->setContentsMargins(14, 16, 14, 14);

    auto *ackBtn = new QPushButton(QStringLiteral("ACKNOWLEDGE (ACK)"), operatorBox);
    ackBtn->setCursor(Qt::PointingHandCursor);
    ackBtn->setStyleSheet(QStringLiteral("background-color: #0984e3; color: white; font-weight: bold; padding: 8px 16px;"));
    connect(ackBtn, &QPushButton::clicked, m_annunciatorPanel, &AnnunciatorPanel::acknowledgeAll);

    auto *silenceBtn = new QPushButton(QStringLiteral("SILENCE"), operatorBox);
    silenceBtn->setCursor(Qt::PointingHandCursor);
    silenceBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(silenceBtn, &QPushButton::clicked, m_annunciatorPanel, &AnnunciatorPanel::silence);

    auto *resetBtn = new QPushButton(QStringLiteral("RESET"), operatorBox);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(resetBtn, &QPushButton::clicked, m_annunciatorPanel, &AnnunciatorPanel::resetAll);

    auto *lampTestBtn = new QPushButton(QStringLiteral("LAMP TEST"), operatorBox);
    lampTestBtn->setCheckable(true);
    lampTestBtn->setCursor(Qt::PointingHandCursor);
    lampTestBtn->setStyleSheet(QStringLiteral("padding: 8px 14px; font-weight: bold;"));
    connect(lampTestBtn, &QPushButton::toggled, m_annunciatorPanel, &AnnunciatorPanel::setLampTest);

    operatorLayout->addWidget(ackBtn);
    operatorLayout->addWidget(silenceBtn);
    operatorLayout->addWidget(resetBtn);
    operatorLayout->addWidget(lampTestBtn);
    annControlsRow->addWidget(operatorBox, 1);

    // Alarm Trip Simulator & ISA Sequence selector
    auto *simBox = new QGroupBox(QStringLiteral("Alarm Event Simulator && Configuration"), annunciatorTab);
    // Two rows: on one row the box gets less width than its controls' size hints (#28)
    auto *simLayout = new QVBoxLayout(simBox);
    simLayout->setSpacing(10);
    simLayout->setContentsMargins(14, 16, 14, 14);

    auto *seqCombo = new QComboBox(simBox);
    seqCombo->addItem(QStringLiteral("Sequence A (Automatic Reset)"), static_cast<int>(AnnunciatorPanel::AnnunciatorSequence::SequenceA_AutomaticReset));
    seqCombo->addItem(QStringLiteral("Sequence M (Manual Reset)"), static_cast<int>(AnnunciatorPanel::AnnunciatorSequence::SequenceM_ManualReset));
    connect(seqCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, seqCombo](int index) {
        auto seq = static_cast<AnnunciatorPanel::AnnunciatorSequence>(seqCombo->itemData(index).toInt());
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

    auto *seqRow = new QHBoxLayout();
    seqRow->setSpacing(12);
    seqRow->addWidget(new QLabel(QStringLiteral("Sequence:"), simBox));
    seqRow->addWidget(seqCombo);
    seqRow->addStretch();
    simLayout->addLayout(seqRow);

    auto *tripRow = new QHBoxLayout();
    tripRow->setSpacing(12);
    tripRow->addWidget(tripTurbineBtn);
    tripRow->addWidget(tripSteamBtn);
    tripRow->addWidget(tripFireBtn);
    tripRow->addWidget(clearAllBtn);
    simLayout->addLayout(tripRow);
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

    connect(m_annunciatorPanel, &AnnunciatorPanel::activeAlarmsCountChanged, this, [updateStatus](int) {
        updateStatus();
    });
    connect(m_annunciatorPanel, &AnnunciatorPanel::unacknowledgedCountChanged, this, [updateStatus](int) {
        updateStatus();
    });
    connect(m_annunciatorPanel, &AnnunciatorPanel::audibleHornChanged, this, [this](bool horn) {
        if (horn) {
            m_annunciatorHornLabel->setText(QStringLiteral("HORN: SOUNDING (AUDIBLE ALARM)"));
            m_annunciatorHornLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #eb3b5a; border-radius: 4px; color: #ffffff;"));
        } else {
            m_annunciatorHornLabel->setText(QStringLiteral("HORN: SILENT"));
            m_annunciatorHornLabel->setStyleSheet(m_isDarkTheme 
                ? QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #283042; border-radius: 4px; color: #a4b3c6;")
                : QStringLiteral("font-weight: bold; font-family: monospace; padding: 4px 10px; background: #e8ecf1; border-radius: 4px; color: #718093;"));
        }
    });

    // When an operator clicks a tile directly:
    connect(m_annunciatorPanel, &AnnunciatorPanel::tileClicked, this, [this](int idx) {
        if (m_annunciatorPanel->tileState(idx) == AnnunciatorPanel::AlarmState::Unacknowledged) {
            m_annunciatorPanel->acknowledge(idx);
        } else if (m_annunciatorPanel->tileState(idx) == AnnunciatorPanel::AlarmState::Ringback) {
            m_annunciatorPanel->reset(idx);
        } else if (m_annunciatorPanel->tileState(idx) == AnnunciatorPanel::AlarmState::Normal) {
            m_annunciatorPanel->setAlarmActive(idx, !m_annunciatorPanel->isAlarmActive(idx));
        }
    });

    tabWidget->addTab(annunciatorTab, QStringLiteral("Annunciator"));

    // ------------------------------------------------------------------------
    // TAB 5: Navigation & Directional Gyro (Compass)
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

    auto *navTitleLbl = new QLabel(QStringLiteral("<b>Aeronautical & Marine Directional Navigation Instruments</b>"), navHeaderBox);
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
    m_compassHeadingUp = new Compass(headingUpGroup);
    m_compassHeadingUp->setDisplayMode(Compass::DisplayMode::HeadingUp);
    m_compassHeadingUp->setHeading(45.0);
    m_compassHeadingUp->setTargetHeading(90.0);
    m_compassHeadingUp->setMinimumHeight(280);
    huLayout->addWidget(m_compassHeadingUp);
    instrumentsRow->addWidget(headingUpGroup, 1);

    // Group 2: North-Up Directional Indicator
    auto *northUpGroup = new QGroupBox(QStringLiteral("North-Up Heading Indicator (360° Magnetic Pointer)"), navTab);
    auto *nuLayout = new QVBoxLayout(northUpGroup);
    nuLayout->setContentsMargins(12, 16, 12, 12);
    m_compassNorthUp = new Compass(northUpGroup);
    m_compassNorthUp->setDisplayMode(Compass::DisplayMode::NorthUp);
    m_compassNorthUp->setHeading(45.0);
    m_compassNorthUp->setTargetHeading(90.0);
    m_compassNorthUp->setMinimumHeight(280);
    nuLayout->addWidget(m_compassNorthUp);
    instrumentsRow->addWidget(northUpGroup, 1);

    navLayout->addLayout(instrumentsRow, 1);

    // Bottom Controls & Simulation Console
    auto *navControlsBox = new QGroupBox(QStringLiteral("Navigation Controls && Autopilot Simulation"), navTab);
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

    addCardBtn(QStringLiteral("North (000°)"), 0.0);
    addCardBtn(QStringLiteral("East (090°)"), 90.0);
    addCardBtn(QStringLiteral("South (180°)"), 180.0);
    addCardBtn(QStringLiteral("West (270°)"), 270.0);

    // Align Button
    auto *alignBtn = new QPushButton(QStringLiteral("Align Bug to Heading"), navControlsBox);
    alignBtn->setCursor(Qt::PointingHandCursor);
    connect(alignBtn, &QPushButton::clicked, this, [=]() {
        double currentHdg = m_compassHeadingUp->heading();
        m_compassHeadingUp->setTargetHeading(currentHdg);
        m_compassNorthUp->setTargetHeading(currentHdg);
        m_targetBugSlider->setValue(static_cast<int>(currentHdg));
    });
    buttonRow->addWidget(alignBtn);

    // Autopilot Button
    m_autopilotButton = new QPushButton(QStringLiteral("Engage Autopilot Heading Hold"), navControlsBox);
    m_autopilotButton->setCheckable(true);
    m_autopilotButton->setCursor(Qt::PointingHandCursor);
    m_autopilotButton->setStyleSheet(QStringLiteral("background-color: #242c3d; color: #00e5ff; font-weight: bold; padding: 6px 14px;"));
    connect(m_autopilotButton, &QPushButton::toggled, this, [this, autopilotStatusLabel](bool checked) {
        m_isAutopilotActive = checked;
        if (checked) {
            m_autopilotButton->setText(QStringLiteral("Disengage Autopilot"));
            m_autopilotButton->setStyleSheet(QStringLiteral("background-color: #eb3b5a; color: white; font-weight: bold; padding: 6px 14px;"));
            autopilotStatusLabel->setText(QStringLiteral("AUTOPILOT: LOCKED (HEADING HOLD)"));
            autopilotStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-family: monospace; color: #2ecc71;"));
        } else {
            m_autopilotButton->setText(QStringLiteral("Engage Autopilot Heading Hold"));
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
    connect(m_compassHeadingUp, &Compass::targetHeadingChanged, this, [=](double val) {
        m_compassNorthUp->setTargetHeading(val);
        m_targetBugSlider->setValue(static_cast<int>(val));
        bugValLbl->setText(QStringLiteral("%1°").arg(static_cast<int>(val), 3, 10, QLatin1Char('0')));
        updateDeviation();
    });
    connect(m_compassNorthUp, &Compass::targetHeadingChanged, this, [=](double val) {
        m_compassHeadingUp->setTargetHeading(val);
        m_targetBugSlider->setValue(static_cast<int>(val));
        bugValLbl->setText(QStringLiteral("%1°").arg(static_cast<int>(val), 3, 10, QLatin1Char('0')));
        updateDeviation();
    });

    navLayout->addWidget(navControlsBox);
    tabWidget->addTab(navTab, QStringLiteral("Compass"));

    // ------------------------------------------------------------------------
    // TAB 6: High-Rate DAQ Telemetry (StripChart)
    // ------------------------------------------------------------------------
    auto *daqTab = new QWidget(tabWidget);
    auto *daqLayout = new QVBoxLayout(daqTab);
    daqLayout->setContentsMargins(14, 14, 14, 14);
    daqLayout->setSpacing(12);

    m_daqChart = new StripChart(daqTab);
    m_daqChart->setCapacity(1000000);
    m_daqChart->setXAxisMode(StripChart::XAxisMode::Time);
    m_daqChart->setTimeSpan(std::chrono::seconds(10));
    m_daqChart->setDecimationMode(StripChart::DecimationMode::Auto);
    m_daqChart->setInterpolation(StripChart::Interpolation::Linear);
    m_daqChart->setTimeLabelFormat(StripChart::TimeLabelFormat::Relative);
    m_daqChart->setTimeOrigin(QDateTime::currentDateTime());
    m_daqChart->setYRange(-30.0, 160.0);
    m_daqChannelSine = m_daqChart->addChannel(QStringLiteral("Ch1: 100 kHz Sine + Glitches"), QColor(0, 229, 255), 1.8);
    m_daqChannelSquare = m_daqChart->addChannel(QStringLiteral("Ch2: Telemetry Pulse"), QColor(255, 170, 0), 1.8);
    m_daqChart->installEventFilter(this);

    daqLayout->addWidget(m_daqChart, 1);

    // Controls panel
    auto *daqControlsBox = new QGroupBox(QStringLiteral("High-Rate Acquisition Controls && Real-Time Performance"), daqTab);
    auto *daqControlsLayout = new QGridLayout(daqControlsBox);
    daqControlsLayout->setContentsMargins(12, 16, 12, 12);
    daqControlsLayout->setHorizontalSpacing(16);
    daqControlsLayout->setVerticalSpacing(10);

    // Decimation Mode
    auto *decimLbl = new QLabel(QStringLiteral("Decimation Mode:"), daqControlsBox);
    auto *decimCombo = new QComboBox(daqControlsBox);
    decimCombo->addItem(QStringLiteral("Auto (M4 Min/Max)"), static_cast<int>(StripChart::DecimationMode::Auto));
    decimCombo->addItem(QStringLiteral("Off (Raw Samples)"), static_cast<int>(StripChart::DecimationMode::Off));
    decimCombo->addItem(QStringLiteral("Always (Force M4)"), static_cast<int>(StripChart::DecimationMode::Always));
    connect(decimCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_daqChart->setDecimationMode(static_cast<StripChart::DecimationMode>(decimCombo->itemData(idx).toInt()));
    });
    daqControlsLayout->addWidget(decimLbl, 0, 0);
    daqControlsLayout->addWidget(decimCombo, 0, 1);

    // X-Axis Mode
    auto *xAxisLbl = new QLabel(QStringLiteral("X-Axis Mode:"), daqControlsBox);
    auto *xAxisCombo = new QComboBox(daqControlsBox);
    xAxisCombo->addItem(QStringLiteral("Time Axis (Real-Time)"), static_cast<int>(StripChart::XAxisMode::Time));
    xAxisCombo->addItem(QStringLiteral("Sample Index (Legacy)"), static_cast<int>(StripChart::XAxisMode::SampleIndex));
    connect(xAxisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_daqChart->setXAxisMode(static_cast<StripChart::XAxisMode>(xAxisCombo->itemData(idx).toInt()));
    });
    daqControlsLayout->addWidget(xAxisLbl, 0, 2);
    daqControlsLayout->addWidget(xAxisCombo, 0, 3);

    // Time Span
    auto *spanLbl = new QLabel(QStringLiteral("Time Span:"), daqControlsBox);
    auto *spanCombo = new QComboBox(daqControlsBox);
    spanCombo->addItem(QStringLiteral("1 Second"), 1.0);
    spanCombo->addItem(QStringLiteral("5 Seconds"), 5.0);
    spanCombo->addItem(QStringLiteral("10 Seconds (Default)"), 10.0);
    spanCombo->addItem(QStringLiteral("30 Seconds"), 30.0);
    spanCombo->setCurrentIndex(2);
    connect(spanCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_daqChart->setTimeSpanSeconds(spanCombo->itemData(idx).toDouble());
    });
    daqControlsLayout->addWidget(spanLbl, 1, 0);
    daqControlsLayout->addWidget(spanCombo, 1, 1);

    // Interpolation
    auto *interpLbl = new QLabel(QStringLiteral("Interpolation:"), daqControlsBox);
    auto *interpCombo = new QComboBox(daqControlsBox);
    interpCombo->addItem(QStringLiteral("Linear"), static_cast<int>(StripChart::Interpolation::Linear));
    interpCombo->addItem(QStringLiteral("Step (Sample & Hold)"), static_cast<int>(StripChart::Interpolation::Step));
    connect(interpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_daqChart->setInterpolation(static_cast<StripChart::Interpolation>(interpCombo->itemData(idx).toInt()));
    });
    daqControlsLayout->addWidget(interpLbl, 1, 2);
    daqControlsLayout->addWidget(interpCombo, 1, 3);

    // Time Label Format
    auto *labelFmtLbl = new QLabel(QStringLiteral("Label Format:"), daqControlsBox);
    auto *labelFmtCombo = new QComboBox(daqControlsBox);
    labelFmtCombo->addItem(QStringLiteral("Relative (-10s ... 0s)"), static_cast<int>(StripChart::TimeLabelFormat::Relative));
    labelFmtCombo->addItem(QStringLiteral("Absolute (HH:mm:ss)"), static_cast<int>(StripChart::TimeLabelFormat::Absolute));
    connect(labelFmtCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_daqChart->setTimeLabelFormat(static_cast<StripChart::TimeLabelFormat>(labelFmtCombo->itemData(idx).toInt()));
    });
    daqControlsLayout->addWidget(labelFmtLbl, 2, 0);
    daqControlsLayout->addWidget(labelFmtCombo, 2, 1);

    // Metrics label
    m_daqMetricsLabel = new QLabel(QStringLiteral("Streaming: 100 kHz (1,600 samples/frame) | Median Paint Time (last 60): -- ms"), daqControlsBox);
    m_daqMetricsLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; font-weight: bold; color: #00e5ff;"));
    daqControlsLayout->addWidget(m_daqMetricsLabel, 2, 2, 1, 2);

    daqLayout->addWidget(daqControlsBox);
    tabWidget->addTab(daqTab, QStringLiteral("High-Rate DAQ"));

    // ------------------------------------------------------------------------
    // TAB 7: Recorded data review (AnalysisChart)
    // ------------------------------------------------------------------------
    auto *analysisTab = new QWidget(tabWidget);
    auto *analysisLayout = new QVBoxLayout(analysisTab);
    analysisLayout->setContentsMargins(14, 14, 14, 14);
    analysisLayout->setSpacing(12);

    m_analysisChart = new AnalysisChart(analysisTab);
    {
        // About 1 M recorded samples over 60 s, generated analytically (no random numbers, no clock):
        // two channels at different rates, spikes, NaN dropouts and a recording gap
        using std::chrono::nanoseconds;
        const double pi = std::acos(-1.0);
        auto noise = [](quint64 i, quint64 seed) {
            const quint64 x = (i + seed * 1000003ULL) * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<double>(x >> 40) / static_cast<double>(1ULL << 24) - 0.5; // -0.5 .. 0.5
        };
        std::vector<nanoseconds> t;
        std::vector<double> v;
        const int vibration = m_analysisChart->addChannel(QStringLiteral("Vibration 10 kHz"), QColor(0, 229, 255), 1.4);
        for (qint64 i = 0; i < 600000; ++i) { // 10 kHz, 60 s
            const double ts = static_cast<double>(i) / 10000.0;
            double y = 0.6 * std::sin(2.0 * pi * 0.25 * ts) + 0.25 * std::sin(2.0 * pi * 47.0 * ts) + 0.12 * noise(static_cast<quint64>(i), 1);
            if (i % 73000 == 36500) y = 2.4;                               // impact spikes
            if ((ts >= 21.0 && ts < 21.15) || (ts >= 44.2 && ts < 44.25)) // sensor dropouts
                y = std::numeric_limits<double>::quiet_NaN();
            t.push_back(nanoseconds(i * 100000));
            v.push_back(y);
        }
        m_analysisChart->setChannelData(vibration, t.data(), v.data(), static_cast<qsizetype>(t.size()));
        t.clear();
        v.clear();
        const int pressure = m_analysisChart->addChannel(QStringLiteral("Pressure 5 kHz"), QColor(255, 170, 0), 1.4);
        for (qint64 i = 0; i < 300000; ++i) { // 5 kHz, 60 s, nothing recorded from 33 s to 35 s
            const double ts = static_cast<double>(i) / 5000.0;
            if (ts >= 33.0 && ts < 35.0) continue;
            const double ramp = ts < 10.0 ? ts / 10.0 : (ts < 50.0 ? 1.0 : 1.0 - (ts - 50.0) / 10.0);
            t.push_back(nanoseconds(i * 200000));
            v.push_back(-1.6 + 1.1 * ramp + 0.05 * std::sin(2.0 * pi * 3.0 * ts) + 0.03 * noise(static_cast<quint64>(i), 2));
        }
        m_analysisChart->setChannelData(pressure, t.data(), v.data(), static_cast<qsizetype>(t.size()));
        t.clear();
        v.clear();
        const int temperature = m_analysisChart->addChannel(QStringLiteral("Temperature 1 kHz"), QColor(0, 230, 118), 1.4);
        for (qint64 i = 0; i < 60000; ++i) { // 1 kHz, 60 s
            const double ts = static_cast<double>(i) / 1000.0;
            t.push_back(nanoseconds(i * 1000000));
            v.push_back(1.2 + 0.4 * (1.0 - std::exp(-ts / 20.0)) + 0.02 * noise(static_cast<quint64>(i), 3));
        }
        m_analysisChart->setChannelData(temperature, t.data(), v.data(), static_cast<qsizetype>(t.size()));
        m_analysisChart->resetView();
        m_analysisChart->setCursorVisible(AnalysisChart::Cursor::A, true);
        m_analysisChart->setCursorVisible(AnalysisChart::Cursor::B, true);
    }
    analysisLayout->addWidget(m_analysisChart, 1);

    auto *analysisControlsBox = new QGroupBox(QStringLiteral("Review Controls"), analysisTab);
    auto *analysisControlsLayout = new QGridLayout(analysisControlsBox);
    analysisControlsLayout->setContentsMargins(12, 16, 12, 12);
    analysisControlsLayout->setHorizontalSpacing(16);
    analysisControlsLayout->setVerticalSpacing(10);

    auto *cursorACheck = new QCheckBox(QStringLiteral("Cursor A"), analysisControlsBox);
    cursorACheck->setChecked(true);
    connect(cursorACheck, &QCheckBox::toggled, this, [this](bool on) { m_analysisChart->setCursorVisible(AnalysisChart::Cursor::A, on); });
    auto *cursorBCheck = new QCheckBox(QStringLiteral("Cursor B"), analysisControlsBox);
    cursorBCheck->setChecked(true);
    connect(cursorBCheck, &QCheckBox::toggled, this, [this](bool on) { m_analysisChart->setCursorVisible(AnalysisChart::Cursor::B, on); });
    auto *resetViewButton = new QPushButton(QStringLiteral("Reset View"), analysisControlsBox);
    connect(resetViewButton, &QPushButton::clicked, m_analysisChart, &AnalysisChart::resetView);
    analysisControlsLayout->addWidget(cursorACheck, 0, 0);
    analysisControlsLayout->addWidget(cursorBCheck, 0, 1);
    analysisControlsLayout->addWidget(resetViewButton, 0, 2);

    auto *analysisDecimLbl = new QLabel(QStringLiteral("Decimation Mode:"), analysisControlsBox);
    auto *analysisDecimCombo = new QComboBox(analysisControlsBox);
    analysisDecimCombo->addItem(QStringLiteral("Auto (M4 Min/Max)"), static_cast<int>(AnalysisChart::DecimationMode::Auto));
    analysisDecimCombo->addItem(QStringLiteral("Off (Raw Samples)"), static_cast<int>(AnalysisChart::DecimationMode::Off));
    analysisDecimCombo->addItem(QStringLiteral("Always (Force M4)"), static_cast<int>(AnalysisChart::DecimationMode::Always));
    connect(analysisDecimCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_analysisChart->setDecimationMode(static_cast<AnalysisChart::DecimationMode>(analysisDecimCombo->itemData(idx).toInt()));
    });
    analysisControlsLayout->addWidget(analysisDecimLbl, 0, 3);
    analysisControlsLayout->addWidget(analysisDecimCombo, 0, 4);

    auto *analysisLabelLbl = new QLabel(QStringLiteral("Label Format:"), analysisControlsBox);
    auto *analysisLabelCombo = new QComboBox(analysisControlsBox);
    analysisLabelCombo->addItem(QStringLiteral("Relative (from data start)"), static_cast<int>(AnalysisChart::TimeLabelFormat::Relative));
    analysisLabelCombo->addItem(QStringLiteral("Absolute (HH:mm:ss.zzz)"), static_cast<int>(AnalysisChart::TimeLabelFormat::Absolute));
    m_analysisChart->setTimeOrigin(QDateTime(QDate(2026, 10, 18), QTime(9, 30, 0), Qt::UTC));
    connect(analysisLabelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_analysisChart->setTimeLabelFormat(static_cast<AnalysisChart::TimeLabelFormat>(analysisLabelCombo->itemData(idx).toInt()));
    });
    analysisControlsLayout->addWidget(analysisLabelLbl, 0, 5);
    analysisControlsLayout->addWidget(analysisLabelCombo, 0, 6);

    auto *analysisHelp = new QLabel(QStringLiteral("Wheel: zoom time · Ctrl+wheel: zoom values · Drag: pan · Right drag or Shift+drag: box zoom · "
                                                   "Drag a cursor line to move it · Double-click or Home: reset · +/− and arrow keys"),
                                    analysisControlsBox);
    analysisHelp->setWordWrap(true);
    analysisControlsLayout->addWidget(analysisHelp, 1, 0, 1, 7);

    analysisLayout->addWidget(analysisControlsBox);
    tabWidget->addTab(analysisTab, QStringLiteral("Analysis Chart"));

    rootLayout->addWidget(tabWidget);
}

void MainWindow::toggleSimulation()
{
    if (m_isSimulating) {
        m_simTimer.stop();
        m_isSimulating = false;
        m_simButton->setText(QStringLiteral("Start Simulation"));
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
        m_simButton->setText(QStringLiteral("Stop Simulation"));

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
        m_tourButton->setText(QStringLiteral("Stop Tour"));
        m_tourButton->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #0984e3; border: 1px solid #74b9ff; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #0870c2; }"
        ));
        m_tourTimer.start(3500);
    } else {
        m_tourButton->setChecked(false);
        m_tourButton->setText(QStringLiteral("Auto-Tour"));
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
        m_fpsLabel->setText(QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', 1)));
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

    // Stream real-time waveforms into StripChart
    m_stripChart->addDataPoint(m_chRpm, (currentRpm / 8000.0) * 100.0);
    m_stripChart->addDataPoint(m_chBoost, boostBase * 35.0);
    m_stripChart->addDataPoint(m_chTemp, coolantTemp);

    // High-Rate DAQ Streaming (100 kHz)
    if (m_daqChart) {
        constexpr int blockSamples = 1600;
        std::vector<double> sineBlock(blockSamples);
        std::vector<double> squareBlock(blockSamples);

        const auto dt = std::chrono::nanoseconds(10000); // 10 us = 100 kHz
        const auto t0 = m_daqCurrentTime;
        m_daqCurrentTime += std::chrono::nanoseconds(blockSamples * 10000);

        for (int i = 0; i < blockSamples; ++i) {
            m_daqSampleIndex++;
            m_daqPhase += 0.005;

            // Sine wave + noise
            double noise = (static_cast<double>(std::rand()) / RAND_MAX - 0.5) * 4.0;
            double sVal = std::sin(m_daqPhase) * 40.0 + 50.0 + noise;

            // Rare single-sample spike
            if (m_daqSampleIndex % 4137 == 0) {
                sVal = 145.0;
            }

            // Occasional NaN dropout (sensor fault)
            if (m_daqSampleIndex % 8251 < 6) {
                sVal = std::numeric_limits<double>::quiet_NaN();
            }

            sineBlock[i] = sVal;

            // Square / pulse signal
            double sqVal = (std::sin(m_daqPhase * 0.2) >= 0.0) ? 80.0 : 20.0;
            if (m_daqSampleIndex % 5519 == 0) {
                sqVal = -15.0;
            }
            squareBlock[i] = sqVal;
        }

        m_daqChart->addUniformSamples(m_daqChannelSine, t0, dt, sineBlock.data(), blockSamples);
        m_daqChart->addUniformSamples(m_daqChannelSquare, t0, dt, squareBlock.data(), blockSamples);
    }

    // Dynamic multi-channel vibration levels (LevelMeter)
    if (m_vibrationMeter) {
        double vibX = 1.5 + (currentRpm / 8000.0) * 5.2 + 1.2 * std::sin(m_simTime * 14.0);
        double vibY = 1.2 + (boostBase / 3.0) * 4.8 + 1.0 * std::cos(m_simTime * 18.0);
        if (std::fmod(m_simTime, 4.0) < 0.1) {
            vibX += 2.2;
        }
        m_vibrationMeter->setValue(0, vibX);
        m_vibrationMeter->setValue(1, vibY);
    }

    // Dynamic acoustic / bus VU levels (LevelMeter)
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

    // Autopilot Course Correction for Compass
    if (m_isAutopilotActive && m_compassHeadingUp && m_compassNorthUp) {
        double dev = m_compassHeadingUp->courseDeviation();
        if (std::abs(dev) > 0.4) {
            double turnRate = std::clamp(dev * 0.08, -1.8, 1.8);
            double newHdg = Compass::normalizeDegrees(m_compassHeadingUp->heading() - turnRate);
            m_compassHeadingUp->setHeading(newHdg);
            m_compassNorthUp->setHeading(newHdg);
            if (m_headingSlider) {
                m_headingSlider->setValue(static_cast<int>(newHdg));
            }
        }
    }

    // Dynamic circular bargraph simulation (paused while a forced demo input is selected)
    if (m_circBargraphBar && (!m_circInputCombo || m_circInputCombo->currentIndex() == 0)) {
        double circVal = 50.0 + 42.0 * std::sin(m_simTime * 1.5);
        m_circBargraphBar->setValue(circVal);
        if (m_circBargraphDot) m_circBargraphDot->setValue(circVal);
        if (m_circBargraphSemi) m_circBargraphSemi->setValue(circVal);
        if (m_circBargraphSlider) {
            m_circBargraphSlider->blockSignals(true);
            m_circBargraphSlider->setValue(static_cast<int>(circVal));
            m_circBargraphSlider->blockSignals(false);
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
    m_themeButton->setText(dark ? QStringLiteral("Light Theme")
                                : QStringLiteral("Dark Theme"));

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
            // Without these rules a combo keeps the style's light background under the light text.
            // No QComboBox::drop-down rule: styling it hides the style's arrow. The popup list takes
            // its selection colours from the QComboBox rule.
            "QComboBox { background-color: #242c3d; border: 1px solid #364259; border-radius: 4px; color: #e1e7ec; padding: 2px 5px; selection-background-color: #2e384e; selection-color: #00e5ff; }"
            "QComboBox:hover, QComboBox:on { border-color: #00e5ff; }"
            "QComboBox QAbstractItemView { background-color: #242c3d; border: 1px solid #364259; color: #e1e7ec; selection-background-color: #2e384e; selection-color: #00e5ff; outline: 0; }"
            "QSlider::groove:horizontal { border: 1px solid #2e384e; height: 6px; background: #1a202c; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #00e5ff; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #00e5ff; width: 16px; margin-top: -5px; margin-bottom: -5px; border-radius: 8px; }"
            // Scroll bars and the tab-bar scroll buttons otherwise keep the style's light palette
            "QScrollBar:vertical { background: #1a202c; width: 14px; margin: 14px 0 14px 0; border: none; }"
            "QScrollBar:horizontal { background: #1a202c; height: 14px; margin: 0 14px 0 14px; border: none; }"
            "QScrollBar::handle { background: #8e9bb0; border-radius: 4px; }"
            "QScrollBar::handle:vertical { min-height: 24px; margin: 0 2px; }"
            "QScrollBar::handle:horizontal { min-width: 24px; margin: 2px 0; }"
            "QScrollBar::handle:hover { background: #00e5ff; }"
            "QScrollBar::add-line, QScrollBar::sub-line { background: #242c3d; border: 1px solid #364259; }"
            "QScrollBar::add-line:vertical { subcontrol-position: bottom; subcontrol-origin: margin; height: 14px; }"
            "QScrollBar::sub-line:vertical { subcontrol-position: top; subcontrol-origin: margin; height: 14px; }"
            "QScrollBar::add-line:horizontal { subcontrol-position: right; subcontrol-origin: margin; width: 14px; }"
            "QScrollBar::sub-line:horizontal { subcontrol-position: left; subcontrol-origin: margin; width: 14px; }"
            "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"
            // Styling the scroll-bar buttons hides the style's arrows, so they come from the gallery resources
            "QScrollBar::up-arrow:vertical { image: url(:/gallery/images/arrow-up.png); width: 8px; height: 5px; }"
            "QScrollBar::down-arrow:vertical { image: url(:/gallery/images/arrow-down.png); width: 8px; height: 5px; }"
            "QScrollBar::left-arrow:horizontal { image: url(:/gallery/images/arrow-left.png); width: 5px; height: 8px; }"
            "QScrollBar::right-arrow:horizontal { image: url(:/gallery/images/arrow-right.png); width: 5px; height: 8px; }"
            "QTabBar QToolButton { background-color: #242c3d; border: 1px solid #364259; }"
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

        if (m_analysisChart) {
            m_analysisChart->setBackgroundColor(QColor(14, 18, 25));
            m_analysisChart->setGridColor(QColor(42, 54, 70));
            m_analysisChart->setBezelColor(QColor(38, 46, 60));
            m_analysisChart->setTextColor(QColor(210, 220, 235));
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

        // The default dial colour (24, 28, 36) is almost the dark panel colour: use black so the
        // disc stays visible (it is decorative, the segments carry the value)
        for (CircularBargraph *bg : {m_circBargraphBar, m_circBargraphDot, m_circBargraphSemi}) {
            if (bg) bg->setDialColor(QColor(0, 0, 0));
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

        if (m_analysisChart) {
            m_analysisChart->setBackgroundColor(QColor(242, 246, 252));
            m_analysisChart->setGridColor(QColor(208, 218, 230));
            m_analysisChart->setBezelColor(QColor(215, 222, 230));
            m_analysisChart->setTextColor(QColor(40, 48, 60));
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

        // Back to the CircularBargraph default dial colour
        for (CircularBargraph *bg : {m_circBargraphBar, m_circBargraphDot, m_circBargraphSemi}) {
            if (bg) bg->setDialColor(QColor(24, 28, 36));
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // The measured paint time would make exported images differ from run to run
    if (watched == m_daqChart && event->type() == QEvent::Paint && !m_exportMode) {
        QElapsedTimer timer;
        timer.start();
        m_daqChart->removeEventFilter(this);
        QApplication::sendEvent(m_daqChart, event);
        m_daqChart->installEventFilter(this);
        qint64 elapsedNs = timer.nsecsElapsed();
        double ms = static_cast<double>(elapsedNs) / 1e6;

        m_daqPaintTimes.push_back(ms);
        if (m_daqPaintTimes.size() > 60) {
            m_daqPaintTimes.erase(m_daqPaintTimes.begin());
        }

        if (m_daqMetricsLabel && !m_daqPaintTimes.empty()) {
            std::vector<double> sorted = m_daqPaintTimes;
            std::sort(sorted.begin(), sorted.end());
            double median = sorted[sorted.size() / 2];
            m_daqMetricsLabel->setText(QStringLiteral("Streaming: 100 kHz (1,600 samples/frame) | Median Paint Time (last 60): %1 ms")
                                           .arg(median, 0, 'f', 2));
        }
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::prepareForExport()
{
    m_exportMode = true;
    m_simTimer.stop();
    m_tourTimer.stop();

    // Dashboard telemetry (sample-index axis): 300 points per channel of analytic signals
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < 300; ++i) {
        const double t = i * 0.05;
        m_stripChart->addDataPoint(m_chRpm, 55.0 + 25.0 * std::sin(2.0 * pi * t / 6.0));
        m_stripChart->addDataPoint(m_chBoost, 52.0 + 18.0 * std::sin(2.0 * pi * t / 4.0 + 1.0));
        m_stripChart->addDataPoint(m_chTemp, 80.0 + 8.0 * (1.0 - std::exp(-t / 5.0)));
    }

    // High-rate DAQ (time axis, explicit timestamps): 10 s at 100 kHz, the simulation's
    // signals without its random noise
    constexpr int count = 1000000;
    std::vector<double> sine(count);
    std::vector<double> square(count);
    double phase = 0.0;
    for (int i = 0; i < count; ++i) {
        const int index = i + 1;
        phase += 0.005;
        double s = std::sin(phase) * 40.0 + 50.0;
        if (index % 4137 == 0) {
            s = 145.0; // single-sample spike
        }
        if (index % 8251 < 6) {
            s = std::numeric_limits<double>::quiet_NaN(); // sensor dropout
        }
        sine[static_cast<size_t>(i)] = s;
        square[static_cast<size_t>(i)] = (index % 5519 == 0) ? -15.0 : ((std::sin(phase * 0.2) >= 0.0) ? 80.0 : 20.0);
    }
    const auto dt = std::chrono::nanoseconds(10000); // 10 us = 100 kHz
    m_daqChart->addUniformSamples(m_daqChannelSine, std::chrono::nanoseconds(0), dt, sine.data(), count);
    m_daqChart->addUniformSamples(m_daqChannelSquare, std::chrono::nanoseconds(0), dt, square.data(), count);
}

bool MainWindow::exportTabImages(const QString &dir, qreal dpr, bool dark, QStringList *written, QString *error)
{
    static const char *const names[] = {"dashboard", "manual-controls", "circular-bargraph",
                                        "annunciator", "compass", "daq", "analysis-chart"};
    if (m_tabWidget->count() != static_cast<int>(sizeof(names) / sizeof(names[0]))) {
        *error = QStringLiteral("gallery has %1 tabs, the export knows %2").arg(m_tabWidget->count())
                     .arg(sizeof(names) / sizeof(names[0]));
        return false;
    }
    applyTheme(dark);
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        m_tabWidget->setCurrentIndex(i);
        // Freeze what moves with time; the event loop is never run, so no timer fires afterwards
        for (LedIndicator *led : findChildren<LedIndicator *>()) {
            if (led->isBlinking()) {
                led->setBlinking(false);
                led->setOn(true);
            }
        }
        for (LevelMeter *meter : findChildren<LevelMeter *>()) {
            meter->resetPeaks();
        }
        for (CircularBargraph *bargraph : findChildren<CircularBargraph *>()) {
            bargraph->resetPeak();
        }
        QCoreApplication::sendPostedEvents();

        QImage image(size() * dpr, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::black);
        {
            QPainter painter(&image);
            render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
        }
        const QString path = QStringLiteral("%1/tab-%2%3.png").arg(dir, QLatin1String(names[i]),
                                                                   dark ? QString() : QStringLiteral("-light"));
        QImageWriter writer(path, "png");
        writer.setQuality(0); // PNG: maximum compression
        if (!writer.write(image)) {
            *error = QStringLiteral("tab \"%1\": %2").arg(m_tabWidget->tabText(i), writer.errorString());
            return false;
        }
        written->append(path);
    }
    return true;
}
