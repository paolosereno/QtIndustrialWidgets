#include "MainWindow.h"

#include <QtIndustrialWidgets/QRadialGauge.h>
#include <QtIndustrialWidgets/QLinearGauge.h>
#include <QtIndustrialWidgets/QSevenSegmentDisplay.h>
#include <QtIndustrialWidgets/QLedIndicator.h>
#include <QtIndustrialWidgets/QIndustrialKnob.h>
#include <QtIndustrialWidgets/QStripChart.h>
#include <QtIndustrialWidgets/QIndustrialSwitch.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleFactory>
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
    topLayout->addWidget(m_themeButton);

    rootLayout->addWidget(topBar);

    // ========================================================================
    // Tabs: Dashboard vs Manual Controls
    // ========================================================================
    auto *tabWidget = new QTabWidget(this);
    tabWidget->setObjectName(QStringLiteral("mainTabs"));

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

    linearLayout->addLayout(combinedLinearLayout);
    lowerRowLayout->addWidget(linearGroup, 3);

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

    controlsLayout->setRowStretch(row, 1);
    tabWidget->addTab(controlsTab, QStringLiteral("🎛️ Manual Controls & Diagnostics"));

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
    }
}
