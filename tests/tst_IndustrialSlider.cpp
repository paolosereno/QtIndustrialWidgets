// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #180, part 1: IndustrialSlider as a display. The setpoint and its clamp, the feedback and its states (#151), the
// deviation states (Off without a feedback, Unknown for NaN and ±Inf), the drawing of the handle, the bar, the hatches
// and the zones, the readouts and their 7 pt floor, the computed minimum size and the screen change of #177.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialSlider.h>

#include "internal/FontScale.h"
#include "internal/IndustrialSliderLayout.h"
#include "internal/IndustrialSliderTestAccess.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/TankDisplayLayout.h"

#include "FontTestUtils.h"
#include "HatchTestUtils.h"
#include "PixelTestUtils.h"
#include "ScreenChangeTestUtils.h"

#include <QtGui/QImage>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QVBoxLayout>

#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;
using DeviationState = IndustrialSlider::DeviationState;

class tst_IndustrialSlider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void setpointIsClampedAndFinite();
    void rangeChangeClampsTheSetpoint();
    void feedbackStates_data();
    void feedbackStates();
    void signalOrder();
    void deviationStates_data();
    void deviationStates();
    void deviationFollowsEveryInput();
    void withoutFeedbackNothingIsDrawn_data();
    void withoutFeedbackNothingIsDrawn();
    void unknownDeviationIsShown_data();
    void unknownDeviationIsShown();
    void handleAndBarAtTheirValues_data();
    void handleAndBarAtTheirValues();
    void outOfRangeBarStopsAtTheEndWithASign_data();
    void outOfRangeBarStopsAtTheEndWithASign();
    void deviationIsCrossHatched_data();
    void deviationIsCrossHatched();
    void invalidFeedbackHatchesTheTrack_data();
    void invalidFeedbackHatchesTheTrack();
    void zoneBand();
    void readoutParts_data();
    void readoutParts();
    void unitGoesFirstThenTheNumber();
    void sizeHintHoldsTheWholeReadouts_data();
    void sizeHintHoldsTheWholeReadouts();
    void readoutsInsideTheirPanelsNeverBelowSevenPoints_data();
    void readoutsInsideTheirPanelsNeverBelowSevenPoints();
    void textsInsideTheWidgetFromTheMinimumUp_data();
    void textsInsideTheWidgetFromTheMinimumUp();
    void layoutDoesNotMoveWhenAFeedbackComes();
    void textFollowsTheWidgetFontSize();
    void minimumSizeHintFollowsTheConfiguration();
    void screenChangeOfAVisibleSlider_data();
    void screenChangeOfAVisibleSlider();
    void nonFiniteInputAndExtremeSizes();
};

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Colours that no other part of the drawing uses
const QColor kHandle(0, 0, 255);
const QColor kFeedback(255, 0, 255);
const QColor kTrack(0, 64, 0);
const QColor kScale(0, 255, 0);
const QColor kText(255, 255, 0);
const QColor kReadout(255, 255, 255);
const QColor kNormal(0, 128, 128);
const QColor kWarning(255, 128, 0);
const QColor kError(128, 0, 255);
const QColor kBackground(0, 0, 0);
// The hatches contrast with the track whatever the text colour: white or black mixed into the track up to 4.5:1
const QColor kHatch = internal::hatchColorOn(kTrack);

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

// The slider with the test colours on an opaque black background
void prepare(IndustrialSlider &s, QSize size = QSize(300, 90))
{
    s.setHandleColor(kHandle);
    s.setFeedbackColor(kFeedback);
    s.setTrackColor(kTrack);
    s.setScaleColor(kScale);
    s.setTextColor(kText);
    s.setReadoutColor(kReadout);
    s.setNormalColor(kNormal);
    s.setWarningColor(kWarning);
    s.setErrorColor(kError);
    QPalette pal = s.palette();
    pal.setColor(QPalette::Window, kBackground);
    s.setPalette(pal);
    s.setAutoFillBackground(true);
    s.resize(size);
}

QSize sizeFor(bool vertical)
{
    return vertical ? QSize(140, 300) : QSize(300, 90);
}

QImage render(IndustrialSlider &s)
{
    return s.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

internal::SliderLayout layoutOf(const IndustrialSlider &s)
{
    return internal::IndustrialSliderTestAccess::lastLayout(s);
}

// Pixel at a logical point
QRgb at(const QImage &img, double x, double y)
{
    const double dpr = img.devicePixelRatio();
    const int px = static_cast<int>(std::floor(x * dpr));
    const int py = static_cast<int>(std::floor(y * dpr));
    if (px < 0 || py < 0 || px >= img.width() || py >= img.height()) {
        return qRgba(0, 0, 0, 0);
    }
    return img.pixel(px, py);
}

// Pixel at an along / across point of the layout
QRgb atScale(const QImage &img, const internal::SliderLayout &l, double along, double across)
{
    return l.vertical ? at(img, across, along) : at(img, along, across);
}

// A rectangle between two along and two across coordinates
QRectF scaleRect(const internal::SliderLayout &l, double a0, double a1, double c0, double c1)
{
    const double lo = std::min(a0, a1);
    const double hi = std::max(a0, a1);
    return l.vertical ? QRectF(c0, lo, c1 - c0, hi - lo) : QRectF(lo, c0, hi - lo, c1 - c0);
}

double alongOf(const IndustrialSlider &s, double value)
{
    return layoutOf(s).along((value - s.minimum()) / (s.maximum() - s.minimum()));
}

// Hatch ink: a pixel covered at least 150/255 of the way from the track colour to the hatch colour (the threshold of
// HatchTest::isHatchPixel(), green > 150 on black); antialiased fringes below that are not counted
bool isHatch(QRgb p)
{
    const double d[3] = {double(kHatch.red() - kTrack.red()), double(kHatch.green() - kTrack.green()),
                         double(kHatch.blue() - kTrack.blue())};
    const double v[3] = {double(qRed(p) - kTrack.red()), double(qGreen(p) - kTrack.green()), double(qBlue(p) - kTrack.blue())};
    const double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    const double t = (v[0] * d[0] + v[1] * d[1] + v[2] * d[2]) / dd;
    const double off = std::sqrt(std::max(0.0, v[0] * v[0] + v[1] * v[1] + v[2] * v[2] - t * t * dd));
    return t >= 150.0 / 255.0 && off <= 40.0;
}

// The hatch pixels of \a img in pure green on black, as HatchTest::hatchError() reads them
QImage hatchMask(const QImage &img)
{
    QImage mask(img.size(), QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            mask.setPixel(x, y, isHatch(img.pixel(x, y)) ? qRgb(0, 255, 0) : qRgb(0, 0, 0));
        }
    }
    return mask;
}

using PixelTest::boundsOf;
using PixelTest::countIn;

void addOrientations()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("horizontal") << false;
    QTest::newRow("vertical") << true;
}

void setVertical(IndustrialSlider &s, bool vertical)
{
    s.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
}

// The device pixels of the readout panels that hold readout ink (near white), inside \a pod
int readoutInk(const QImage &img, const QRectF &pod)
{
    return countIn(img, pod, [](QRgb p) { return near(p, kReadout, 110); });
}

} // namespace

void tst_IndustrialSlider::defaults()
{
    IndustrialSlider s;
    QCOMPARE(s.orientation(), Qt::Horizontal);
    QCOMPARE(s.minimum(), 0.0);
    QCOMPARE(s.maximum(), 100.0);
    QCOMPARE(s.setpoint(), 0.0);
    QVERIFY(!s.hasFeedback());
    QVERIFY(std::isnan(s.feedback()));
    QVERIFY(std::isnan(s.rawFeedback()));
    QCOMPARE(s.feedbackState(), ValueState::Invalid);
    QCOMPARE(s.deviationThreshold(), 0.0);
    QCOMPARE(s.deviationState(), DeviationState::Off);
    QCOMPARE(s.precision(), 1);
    QCOMPARE(s.unit(), QStringLiteral("%"));
    QCOMPARE(s.majorTicks(), 5);
    QCOMPARE(s.minorTicks(), 4);
    QVERIFY(std::isnan(s.warningThreshold()));
    QVERIFY(std::isnan(s.errorThreshold()));
    QVERIFY(s.isReadoutVisible());
    QVERIFY(s.isFeedbackReadoutVisible());
    QCOMPARE(s.sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(s.sizePolicy().verticalPolicy(), QSizePolicy::Maximum); // down to minimumSizeHint(), never past sizeHint()
    s.setOrientation(Qt::Vertical);
    QCOMPARE(s.sizePolicy().horizontalPolicy(), QSizePolicy::Maximum);
    QCOMPARE(s.sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
}

// #182: the setpoint is clamped to the range; NaN and ±Inf are rejected with a warning and change nothing
void tst_IndustrialSlider::setpointIsClampedAndFinite()
{
    IndustrialSlider s;
    QSignalSpy spy(&s, &IndustrialSlider::setpointChanged);
    s.setSetpoint(42.5);
    QCOMPARE(s.setpoint(), 42.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toDouble(), 42.5);
    s.setSetpoint(150.0);
    QCOMPARE(s.setpoint(), 100.0);
    s.setSetpoint(-3.0);
    QCOMPARE(s.setpoint(), 0.0);
    QCOMPARE(spy.count(), 2);
    spy.clear();
    s.setSetpoint(0.0);
    QCOMPARE(spy.count(), 0);
    s.setSetpoint(-50.0); // clamped to the same setpoint: no change
    QCOMPARE(spy.count(), 0);
    s.setSetpoint(30.0);
    spy.clear();
    for (double v : {kNaN, kInf, -kInf}) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("IndustrialSlider::setSetpoint: non-finite")));
        s.setSetpoint(v);
        QCOMPARE(s.setpoint(), 30.0);
    }
    QCOMPARE(spy.count(), 0);
    // The property write goes through the same check
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("non-finite")));
    QVERIFY(s.setProperty("setpoint", kNaN));
    QCOMPARE(s.setpoint(), 30.0);
}

void tst_IndustrialSlider::rangeChangeClampsTheSetpoint()
{
    IndustrialSlider s;
    s.setSetpoint(80.0);
    QSignalSpy setpoint(&s, &IndustrialSlider::setpointChanged);
    QSignalSpy range(&s, &IndustrialSlider::rangeChanged);
    s.setRange(0.0, 50.0);
    QCOMPARE(range.count(), 1);
    QCOMPARE(s.setpoint(), 50.0);
    QCOMPARE(setpoint.count(), 1);
    QCOMPARE(setpoint.takeFirst().at(0).toDouble(), 50.0);
    s.setRange(60.0, 70.0);
    QCOMPARE(s.setpoint(), 60.0);
    s.setRange(0.0, 100.0); // the setpoint stays where the clamp put it
    QCOMPARE(s.setpoint(), 60.0);
    // Invalid ranges are ignored
    for (auto r : {std::pair<double, double>{kNaN, 5.0}, {0.0, kInf}, {5.0, 5.0}, {10.0, 5.0}}) {
        s.setRange(r.first, r.second);
    }
    QCOMPARE(s.minimum(), 0.0);
    QCOMPARE(s.maximum(), 100.0);
    QCOMPARE(range.count(), 3);
}

void tst_IndustrialSlider::feedbackStates_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<double>("shown");
    QTest::addColumn<int>("state");
    QTest::newRow("valid") << 42.0 << 42.0 << int(ValueState::Valid);
    QTest::newRow("minimum") << 0.0 << 0.0 << int(ValueState::Valid);
    QTest::newRow("maximum") << 100.0 << 100.0 << int(ValueState::Valid);
    QTest::newRow("over") << 120.0 << 100.0 << int(ValueState::Overrange);
    QTest::newRow("under") << -0.5 << 0.0 << int(ValueState::Underrange);
    QTest::newRow("+inf") << kInf << 100.0 << int(ValueState::Overrange);
    QTest::newRow("-inf") << -kInf << 0.0 << int(ValueState::Underrange);
    QTest::newRow("nan") << kNaN << kNaN << int(ValueState::Invalid);
}

// #151: the input is kept unclamped and classified; the shown value is clamped, NaN for NaN
void tst_IndustrialSlider::feedbackStates()
{
    QFETCH(double, input);
    QFETCH(double, shown);
    QFETCH(int, state);
    IndustrialSlider s;
    s.setFeedback(input);
    QVERIFY(s.hasFeedback());
    QCOMPARE(int(s.feedbackState()), state);
    if (std::isnan(shown)) {
        QVERIFY(std::isnan(s.feedback()));
        QVERIFY(std::isnan(s.rawFeedback()));
    } else {
        QCOMPARE(s.feedback(), shown);
        QCOMPARE(s.rawFeedback(), input);
    }
    // The range classifies the input again
    if (std::isfinite(input) && (input > 100.0 || input < 0.0)) {
        s.setRange(std::min(input, 0.0), std::max(input, 100.0));
        QCOMPARE(s.feedbackState(), ValueState::Valid);
        QCOMPARE(s.feedback(), input);
    }
    s.clearFeedback();
    QVERIFY(!s.hasFeedback());
    QVERIFY(std::isnan(s.feedback()));
    QVERIFY(std::isnan(s.rawFeedback()));
    QCOMPARE(s.feedbackState(), ValueState::Invalid);
}

// The setpoint, then the feedback value, then its state, then the deviation state
void tst_IndustrialSlider::signalOrder()
{
    IndustrialSlider s;
    s.setDeviationThreshold(5.0);
    QStringList order;
    connect(&s, &IndustrialSlider::setpointChanged, this, [&] { order << QStringLiteral("setpoint"); });
    connect(&s, &IndustrialSlider::feedbackChanged, this, [&] { order << QStringLiteral("feedback"); });
    connect(&s, &IndustrialSlider::feedbackStateChanged, this, [&] { order << QStringLiteral("state"); });
    connect(&s, &IndustrialSlider::deviationStateChanged, this, [&] { order << QStringLiteral("deviation"); });
    s.setFeedback(150.0);
    QCOMPARE(order, (QStringList{QStringLiteral("feedback"), QStringLiteral("state"), QStringLiteral("deviation")}));
    order.clear();
    s.setFeedback(150.0); // the same input: nothing
    QVERIFY(order.isEmpty());
    s.setFeedback(kNaN);
    QCOMPARE(order, (QStringList{QStringLiteral("feedback"), QStringLiteral("state"), QStringLiteral("deviation")}));
    order.clear();
    s.setFeedback(kNaN);
    QVERIFY(order.isEmpty());
    s.setFeedback(3.0);
    order.clear();
    s.setSetpoint(50.0);
    QCOMPARE(order, (QStringList{QStringLiteral("setpoint"), QStringLiteral("deviation")}));
    order.clear();
    QCOMPARE(s.deviationState(), DeviationState::Deviating); // setpoint 50, feedback 3
    order.clear();
    s.setRange(0.0, 2.0); // the setpoint and the feedback clamped, the feedback over range, the deviation evaluated
    QCOMPARE(order, (QStringList{QStringLiteral("setpoint"), QStringLiteral("feedback"), QStringLiteral("state"),
                                 QStringLiteral("deviation")}));
    QCOMPARE(s.deviationState(), DeviationState::Normal); // setpoint 2, feedback input 3, threshold 5
}

void tst_IndustrialSlider::deviationStates_data()
{
    QTest::addColumn<bool>("withFeedback");
    QTest::addColumn<double>("threshold");
    QTest::addColumn<double>("setpoint");
    QTest::addColumn<double>("feedback");
    QTest::addColumn<int>("expected");
    QTest::newRow("no feedback") << false << 5.0 << 50.0 << 0.0 << int(DeviationState::Off);
    QTest::newRow("no threshold") << true << 0.0 << 50.0 << 10.0 << int(DeviationState::Off);
    QTest::newRow("no threshold, NaN") << true << 0.0 << 50.0 << kNaN << int(DeviationState::Off);
    QTest::newRow("equal") << true << 5.0 << 50.0 << 50.0 << int(DeviationState::Normal);
    QTest::newRow("at the threshold") << true << 5.0 << 50.0 << 45.0 << int(DeviationState::Normal);
    QTest::newRow("past the threshold, below") << true << 5.0 << 50.0 << 44.9 << int(DeviationState::Deviating);
    QTest::newRow("past the threshold, above") << true << 5.0 << 50.0 << 55.1 << int(DeviationState::Deviating);
    QTest::newRow("NaN") << true << 5.0 << 50.0 << kNaN << int(DeviationState::Unknown);
    QTest::newRow("+Inf") << true << 5.0 << 100.0 << kInf << int(DeviationState::Unknown);
    QTest::newRow("-Inf") << true << 5.0 << 0.0 << -kInf << int(DeviationState::Unknown);
    // A finite input outside the range counts as it is: clamped, it would equal the setpoint
    QTest::newRow("over range, not clamped") << true << 5.0 << 100.0 << 120.0 << int(DeviationState::Deviating);
    QTest::newRow("under range, not clamped") << true << 5.0 << 0.0 << -20.0 << int(DeviationState::Deviating);
    QTest::newRow("over range, within") << true << 5.0 << 100.0 << 104.0 << int(DeviationState::Normal);
}

void tst_IndustrialSlider::deviationStates()
{
    QFETCH(bool, withFeedback);
    QFETCH(double, threshold);
    QFETCH(double, setpoint);
    QFETCH(double, feedback);
    QFETCH(int, expected);
    IndustrialSlider s;
    s.setDeviationThreshold(threshold);
    s.setSetpoint(setpoint);
    if (withFeedback) {
        s.setFeedback(feedback);
    }
    QCOMPARE(int(s.deviationState()), expected);
    // The same state whatever the order of the inputs
    IndustrialSlider t;
    if (withFeedback) {
        t.setFeedback(feedback);
    }
    t.setSetpoint(setpoint);
    t.setDeviationThreshold(threshold);
    QCOMPARE(int(t.deviationState()), expected);
    // Without the feedback, always Off
    s.clearFeedback();
    QCOMPARE(s.deviationState(), DeviationState::Off);
}

// The deviation is evaluated again at every change of the setpoint, the feedback, the threshold and the range
void tst_IndustrialSlider::deviationFollowsEveryInput()
{
    IndustrialSlider s;
    QSignalSpy spy(&s, &IndustrialSlider::deviationStateChanged);
    QSignalSpy threshold(&s, &IndustrialSlider::deviationThresholdChanged);
    s.setFeedback(40.0);
    QCOMPARE(spy.count(), 0); // still Off: no threshold
    s.setDeviationThreshold(5.0);
    QCOMPARE(threshold.count(), 1);
    QCOMPARE(s.deviationState(), DeviationState::Deviating); // setpoint 0
    s.setSetpoint(38.0);
    QCOMPARE(s.deviationState(), DeviationState::Normal);
    s.setDeviationThreshold(1.0);
    QCOMPARE(s.deviationState(), DeviationState::Deviating);
    s.setFeedback(38.5);
    QCOMPARE(s.deviationState(), DeviationState::Normal);
    s.setRange(0.0, 30.0); // setpoint clamped to 30, the feedback input 38.5 counts as it is
    QCOMPARE(s.deviationState(), DeviationState::Deviating);
    QCOMPARE(s.feedbackState(), ValueState::Overrange);
    s.setFeedback(kNaN);
    QCOMPARE(s.deviationState(), DeviationState::Unknown);
    s.setDeviationThreshold(0.0);
    QCOMPARE(s.deviationState(), DeviationState::Off);
    QCOMPARE(spy.count(), 7);
    QCOMPARE(spy.last().at(0).value<DeviationState>(), DeviationState::Off);
    // Negative and infinite thresholds are ignored
    for (double t : {-1.0, kInf, -kInf}) {
        s.setDeviationThreshold(t);
    }
    QCOMPARE(s.deviationThreshold(), 0.0);
    QCOMPARE(threshold.count(), 3);
    // NaN turns the threshold off, as 0 does (#180 plan): from 5, the threshold is 0 and the deviation Off
    s.setFeedback(10.0);
    s.setDeviationThreshold(5.0);
    QCOMPARE(s.deviationState(), DeviationState::Deviating);
    threshold.clear();
    spy.clear();
    s.setDeviationThreshold(kNaN);
    QCOMPARE(s.deviationThreshold(), 0.0);
    QCOMPARE(s.deviationState(), DeviationState::Off);
    QCOMPARE(threshold.count(), 1);
    QCOMPARE(threshold.first().at(0).toDouble(), 0.0);
    QCOMPARE(spy.count(), 1);
    s.setDeviationThreshold(kNaN); // already off: nothing
    QCOMPARE(threshold.count(), 1);
}

void tst_IndustrialSlider::withoutFeedbackNothingIsDrawn_data()
{
    addOrientations();
}

// Decision 2 of #180: without a feedback (never set, or cleared) nothing of it is drawn, whatever the deviation
// threshold: no bar, no hatch, no feedback panel, no deviation mark. The drawing equals that of a slider whose
// feedback colour and readout place differ only
void tst_IndustrialSlider::withoutFeedbackNothingIsDrawn()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setDeviationThreshold(5.0);
    s.setSetpoint(60.0);
    const QImage never = render(s);
    s.setFeedback(kNaN); // Unknown: hatch and mark drawn
    QVERIFY(FontTest::differingPixelCount(never, render(s)) > 0);
    s.setFeedback(10.0); // Deviating: bar, cross-hatch and mark drawn
    QVERIFY(FontTest::differingPixelCount(never, render(s)) > 0);
    s.clearFeedback();
    const QImage cleared = render(s);
    QCOMPARE(FontTest::differingPixelCount(never, cleared), 0);
    QVERIFY(internal::IndustrialSliderTestAccess::lastFeedbackReadout(s).isEmpty());
    // No pixel of the feedback colour, and the place of the feedback panel is background
    QCOMPARE(countIn(cleared, QRectF(0, 0, s.width(), s.height()), [](QRgb p) { return near(p, kFeedback, 60); }), 0);
    const QRectF place = layoutOf(s).feedbackPod;
    QVERIFY(!place.isEmpty());
    QCOMPARE(countIn(cleared, place, [](QRgb p) { return !near(p, kBackground, 4); }), 0);
    // The track holds no hatch
    QCOMPARE(countIn(cleared, layoutOf(s).track.adjusted(1.5, 1.5, -1.5, -1.5), isHatch), 0);
}

void tst_IndustrialSlider::unknownDeviationIsShown_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<double>("input");
    for (bool vertical : {false, true}) {
        for (double v : {kNaN, kInf, -kInf}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(vertical ? "vertical" : "horizontal").arg(v))) << vertical << v;
        }
    }
}

// Decision 2 of #180: a feedback that is NaN or ±Inf gives Unknown, never Normal, and it is visible: the feedback
// panel carries the "?" mark, which the same input without a threshold (Off) does not have
void tst_IndustrialSlider::unknownDeviationIsShown()
{
    QFETCH(bool, vertical);
    QFETCH(double, input);
    IndustrialSlider s;
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setSetpoint(std::isinf(input) ? (input > 0 ? 100.0 : 0.0) : 50.0); // ±Inf: the bar at the setpoint
    s.setFeedback(input);
    const QImage off = render(s);
    const QString offText = internal::IndustrialSliderTestAccess::lastFeedbackReadout(s);
    s.setDeviationThreshold(5.0);
    QCOMPARE(s.deviationState(), DeviationState::Unknown);
    const QImage unknown = render(s);
    const QString unknownText = internal::IndustrialSliderTestAccess::lastFeedbackReadout(s);
    QVERIFY2(unknownText.startsWith(QStringLiteral("? POS")), qPrintable(unknownText));
    QVERIFY2(!offText.contains(QLatin1Char('?')), qPrintable(offText));
    // The mark changes pixels of the panel, and only of the panel
    const QRectF pod = layoutOf(s).feedbackPod;
    const double dpr = unknown.devicePixelRatio();
    const QRect podDevice = QRectF(pod.topLeft() * dpr, pod.size() * dpr).toAlignedRect();
    int inside = 0;
    int outside = 0;
    for (int y = 0; y < unknown.height(); ++y) {
        for (int x = 0; x < unknown.width(); ++x) {
            if (unknown.pixel(x, y) != off.pixel(x, y)) {
                (podDevice.contains(x, y) ? inside : outside) += 1;
            }
        }
    }
    QVERIFY2(inside > 0, "the Unknown state is not drawn");
    QCOMPARE(outside, 0);
}

void tst_IndustrialSlider::handleAndBarAtTheirValues_data()
{
    addOrientations();
}

// The tip of the handle and the centre of the bar at the along coordinate of their values, the minimum on the left
// or at the bottom
void tst_IndustrialSlider::handleAndBarAtTheirValues()
{
    QFETCH(bool, vertical);
    for (double setpoint : {0.0, 25.0, 70.0, 100.0}) {
        for (double feedback : {10.0, 55.0, 90.0}) {
            IndustrialSlider s;
            setVertical(s, vertical);
            prepare(s, sizeFor(vertical));
            s.setSetpoint(setpoint);
            s.setFeedback(feedback);
            const QImage img = render(s);
            const internal::SliderLayout l = layoutOf(s);
            const QString where = QStringLiteral("setpoint %1, feedback %2").arg(setpoint).arg(feedback);
            // The handle: its colour at its value, 2 px from its back; none 9 px away along (half its width is 6)
            const double sp = alongOf(s, setpoint);
            QVERIFY2(near(atScale(img, l, sp, l.handleBase + 2.0), kHandle, 40), qPrintable(where));
            for (double off : {-9.0, 9.0}) {
                const QRgb p = atScale(img, l, sp + off, l.handleBase + 2.0);
                QVERIFY2(!near(p, kHandle, 60), qPrintable(where + QStringLiteral(": handle at %1").arg(off)));
            }
            // The bar: its colour past the far side of the track at its value; none 4 px away along
            const double fb = alongOf(s, feedback);
            const double barAcross = l.trackFar() + internal::kSliderBarOverhang * 0.5;
            QVERIFY2(near(atScale(img, l, fb, barAcross), kFeedback, 60), qPrintable(where));
            for (double off : {-4.0, 4.0}) {
                QVERIFY2(!near(atScale(img, l, fb + off, barAcross), kFeedback, 60),
                         qPrintable(where + QStringLiteral(": bar at %1").arg(off)));
            }
            // The direction: a larger value is right (horizontal) or higher (vertical)
            if (setpoint > 0.0) {
                QVERIFY(vertical ? sp < alongOf(s, 0.0) : sp > alongOf(s, 0.0));
            }
        }
    }
}

void tst_IndustrialSlider::outOfRangeBarStopsAtTheEndWithASign_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<double>("input");
    for (bool vertical : {false, true}) {
        for (double v : {130.0, kInf, -7.0, -kInf}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(vertical ? "vertical" : "horizontal").arg(v))) << vertical << v;
        }
    }
}

// #151: outside the range the bar stops at that end of the scale and a triangle of its colour stands past it,
// pointing away from the scale; a valid feedback at the same end has no triangle
void tst_IndustrialSlider::outOfRangeBarStopsAtTheEndWithASign()
{
    QFETCH(bool, vertical);
    QFETCH(double, input);
    IndustrialSlider s;
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setSetpoint(50.0);
    const bool over = input > 0.0;
    const auto signPixels = [&](const QImage &img) {
        const internal::SliderLayout l = layoutOf(s);
        const double end = over ? l.scaleMax : l.scaleMin;
        // Towards the larger values: right, or up on a vertical scale
        const double outward = (over ? 1.0 : -1.0) * (vertical ? -1.0 : 1.0);
        const double from = end + outward * (internal::kSliderBarWidth * 0.5 + 1.2);
        const double to = end + outward * (internal::kSliderBarWidth * 0.5 + 1.0 + internal::kSliderBeyondSign - 0.5);
        return countIn(img, scaleRect(l, from, to, l.trackNear(), l.trackFar()), [](QRgb p) { return near(p, kFeedback, 90); });
    };
    s.setFeedback(over ? 100.0 : 0.0);
    QCOMPARE(signPixels(render(s)), 0);
    s.setFeedback(input);
    const QImage img = render(s);
    const internal::SliderLayout l = layoutOf(s);
    const double end = over ? l.scaleMax : l.scaleMin;
    QVERIFY(near(atScale(img, l, end, l.trackFar() + internal::kSliderBarOverhang * 0.5), kFeedback, 60));
    QVERIFY2(signPixels(img) > 0, "no sign past the bar");
}

void tst_IndustrialSlider::deviationIsCrossHatched_data()
{
    addOrientations();
}

// A deviation cross-hatches the track between the handle and the bar, and nowhere else; Normal and Off draw no hatch.
// The hatch contrasts with the track even when the text colour is that of the track (dark text of a light theme)
void tst_IndustrialSlider::deviationIsCrossHatched()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setTextColor(kTrack);
    s.setDeviationThreshold(5.0);
    s.setSetpoint(70.0);
    s.setFeedback(20.0);
    QCOMPARE(s.deviationState(), DeviationState::Deviating);
    QImage img = render(s);
    internal::SliderLayout l = layoutOf(s);
    const double c0 = l.trackNear() + 1.0;
    const double c1 = l.trackFar() - 1.0;
    const auto hatch = isHatch;
    const double sp = alongOf(s, 70.0);
    const double fb = alongOf(s, 20.0);
    const double dir = vertical ? -1.0 : 1.0;
    // Between them, clear of the bar and of the tip of the handle
    const QRectF between = scaleRect(l, fb + dir * 4.0, sp - dir * 2.0, c0, c1);
    const int inside = countIn(img, between, hatch);
    const double dpr = img.devicePixelRatio();
    const double area = between.width() * between.height() * dpr * dpr;
    QVERIFY2(inside > area * 0.15, qPrintable(QStringLiteral("%1 hatch pixels of %2").arg(inside).arg(area)));
    QVERIFY(countIn(img, scaleRect(l, alongOf(s, 0.0), fb - dir * 3.0, c0, c1), hatch) == 0);
    QVERIFY(countIn(img, scaleRect(l, sp + dir * 7.0, alongOf(s, 100.0), c0, c1), hatch) == 0);
    for (double feedback : {72.0, 70.0}) {
        s.setFeedback(feedback);
        QCOMPARE(s.deviationState(), DeviationState::Normal);
        img = render(s);
        QCOMPARE(countIn(img, l.track.adjusted(1.0, 1.0, -1.0, -1.0), hatch), 0);
    }
    s.setFeedback(20.0);
    s.setDeviationThreshold(0.0);
    QCOMPARE(countIn(render(s), l.track.adjusted(1.0, 1.0, -1.0, -1.0), hatch), 0);
}

void tst_IndustrialSlider::invalidFeedbackHatchesTheTrack_data()
{
    addOrientations();
}

// #151: a NaN feedback has no position: the whole track is hatched (internal::drawInvalidHatch()) and no bar is drawn.
// in a colour that contrasts with the track, even when the text colour is that of the track
void tst_IndustrialSlider::invalidFeedbackHatchesTheTrack()
{
    QFETCH(bool, vertical);
    IndustrialSlider s;
    setVertical(s, vertical);
    prepare(s, sizeFor(vertical));
    s.setTextColor(kTrack);
    s.setSetpoint(100.0); // the handle at the end, clear of most of the track
    s.setFeedback(40.0);
    const auto areaOf = [&](const internal::SliderLayout &l, double dpr, const QImage &img) {
        // The track, 1 px inside its outline, away from the handle
        const QRectF r = scaleRect(l, alongOf(s, 0.0), alongOf(s, 90.0), l.trackNear() + 1.0, l.trackFar() - 1.0);
        QVector<QPoint> points;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (r.contains(QPointF((x + 0.5) / dpr, (y + 0.5) / dpr))) {
                    points.push_back(QPoint(x, y));
                }
            }
        }
        return points;
    };
    const QImage valid = render(s);
    const QVector<QPoint> area = areaOf(layoutOf(s), valid.devicePixelRatio(), valid);
    QVERIFY(area.size() > 100);
    QVERIFY2(!HatchTest::hatchError(hatchMask(valid), area).isEmpty(), "a valid feedback is hatched");
    s.setFeedback(kNaN);
    const QImage img = render(s);
    const QString error = HatchTest::hatchError(hatchMask(img), area);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(countIn(img, QRectF(0, 0, s.width(), s.height()), [](QRgb p) { return near(p, kFeedback, 60); }), 0);
}

// The zone band: normalColor up to the first threshold, warningColor and errorColor above; none when both are NaN
void tst_IndustrialSlider::zoneBand()
{
    for (bool vertical : {false, true}) {
        IndustrialSlider s;
        setVertical(s, vertical);
        prepare(s, sizeFor(vertical));
        s.setSetpoint(100.0);
        const auto bandAt = [&](const QImage &img, double value) {
            const internal::SliderLayout l = layoutOf(s);
            return atScale(img, l, alongOf(s, value), l.zoneNear + internal::kSliderZone * 0.5);
        };
        QImage img = render(s);
        for (double v : {10.0, 50.0, 95.0}) {
            QVERIFY(near(bandAt(img, v), kBackground, 4));
        }
        QSignalSpy spy(&s, &IndustrialSlider::thresholdChanged);
        s.setWarningThreshold(60.0);
        s.setErrorThreshold(85.0);
        QCOMPARE(spy.count(), 2);
        img = render(s);
        QVERIFY(near(bandAt(img, 30.0), kNormal));
        QVERIFY(near(bandAt(img, 70.0), kWarning));
        QVERIFY(near(bandAt(img, 95.0), kError));
        s.setWarningThreshold(kNaN); // the error zone alone
        img = render(s);
        QVERIFY(near(bandAt(img, 70.0), kNormal));
        QVERIFY(near(bandAt(img, 95.0), kError));
        s.setWarningThreshold(kInf); // ignored
        QVERIFY(std::isnan(s.warningThreshold()));
        s.setErrorThreshold(kNaN);
        img = render(s);
        QVERIFY(near(bandAt(img, 95.0), kBackground, 4));
    }
}

void tst_IndustrialSlider::readoutParts_data()
{
    QTest::addColumn<double>("threshold");
    QTest::addColumn<double>("feedback");
    QTest::addColumn<QString>("expected");
    QTest::newRow("valid") << 0.0 << 58.0 << QStringLiteral("POS 58.0 %");
    QTest::newRow("normal") << 5.0 << 58.0 << QStringLiteral("POS 58.0 %");
    QTest::newRow("deviating") << 5.0 << 15.0 << QStringLiteral("Δ POS 15.0 %");
    QTest::newRow("invalid, off") << 0.0 << kNaN << QStringLiteral("POS ---");
    QTest::newRow("invalid, unknown") << 5.0 << kNaN << QStringLiteral("? POS ---");
    QTest::newRow("over, unknown") << 5.0 << kInf << QStringLiteral("? POS ▲ >100.0 %");
    QTest::newRow("over, deviating") << 5.0 << 120.0 << QStringLiteral("Δ POS ▲ 120.0 %");
    QTest::newRow("under") << 0.0 << -12.5 << QStringLiteral("POS ▼ -12.5 %");
}

// What each panel draws at its preferred size: the tag, the marks, the number and the unit
void tst_IndustrialSlider::readoutParts()
{
    QFETCH(double, threshold);
    QFETCH(double, feedback);
    QFETCH(QString, expected);
    IndustrialSlider s;
    prepare(s, s.sizeHint());
    s.setDeviationThreshold(threshold);
    s.setSetpoint(60.0);
    s.setFeedback(feedback);
    (void)render(s);
    QCOMPARE(internal::IndustrialSliderTestAccess::lastSetpointReadout(s), QStringLiteral("SP 60.0 %"));
    QCOMPARE(internal::IndustrialSliderTestAccess::lastFeedbackReadout(s), expected);
    s.setFeedbackReadoutVisible(false);
    (void)render(s);
    QVERIFY(internal::IndustrialSliderTestAccess::lastFeedbackReadout(s).isEmpty());
    s.setReadoutVisible(false);
    (void)render(s);
    QVERIFY(internal::IndustrialSliderTestAccess::lastSetpointReadout(s).isEmpty());
}

// #60, #167: without room the unit goes first, then the number; the tag and the marks stay. Narrowing the slider
// from its preferred width to its minimum goes through those steps in that order, never back
void tst_IndustrialSlider::unitGoesFirstThenTheNumber()
{
    for (bool vertical : {false, true}) {
        IndustrialSlider probe;
        setVertical(probe, vertical);
        probe.setRange(-10000.0, 10000.0);
        probe.setUnit(QStringLiteral("m³/h"));
        const QSize min = probe.minimumSizeHint();
        const QSize hint = probe.sizeHint();
        QVERIFY(hint.width() > min.width());
        int stage = 0; // 0: number and unit, 1: number, 2: tag only
        QVector<int> stages;
        for (int w = hint.width(); w >= min.width(); --w) {
            IndustrialSlider s;
            setVertical(s, vertical);
            prepare(s, QSize(w, vertical ? 300 : min.height()));
            s.setRange(-10000.0, 10000.0);
            s.setUnit(QStringLiteral("m³/h"));
            s.setDeviationThreshold(1.0);
            s.setSetpoint(-9876.5);
            s.setFeedback(kNaN);
            (void)render(s);
            const QString sp = internal::IndustrialSliderTestAccess::lastSetpointReadout(s);
            const QString fb = internal::IndustrialSliderTestAccess::lastFeedbackReadout(s);
            const QString where = QStringLiteral("%1 px: \"%2\", \"%3\"").arg(w).arg(sp, fb);
            QVERIFY2(sp.startsWith(QStringLiteral("SP")), qPrintable(where));
            QCOMPARE(fb, QStringLiteral("? POS ---")); // the essential parts, at every width
            const int now = sp == QStringLiteral("SP -9876.5 m³/h") ? 0 : sp == QStringLiteral("SP -9876.5") ? 1
                          : sp == QStringLiteral("SP") ? 2 : -1;
            QVERIFY2(now >= 0, qPrintable(where));
            QVERIFY2(now >= stage, qPrintable(where + QStringLiteral(": back to an earlier stage")));
            stage = now;
            if (stages.isEmpty() || stages.last() != now) {
                stages << now;
            }
        }
        QCOMPARE(stages.first(), 0);
        QVERIFY2(stages.contains(1), "no width shows the number without the unit");
    }
}

void tst_IndustrialSlider::sizeHintHoldsTheWholeReadouts_data()
{
    addOrientations();
}

// At sizeHint() both readouts are whole, at their preferred size, for the widest values of the range: the end
// values, with the unit and, for the position, the deviation mark; and a position at ±Inf, shown as the end value
// with "?", the triangle and ">" or "<"; and a deviating position out of the range, with as many digits as the end
// value (the deviation mark and the triangle together). Long values, so that the room of the texts (and not the
// 260 px floor of a horizontal slider) sets the hint
void tst_IndustrialSlider::sizeHintHoldsTheWholeReadouts()
{
    QFETCH(bool, vertical);
    {
        IndustrialSlider s;
        setVertical(s, vertical);
        s.setRange(0.0, 90000.0);
        s.setUnit(QStringLiteral("m³/h"));
        s.setDeviationThreshold(1.0);
        s.setFeedback(95000.0);
        prepare(s, s.sizeHint() * 3);
        (void)render(s);
        const int preferred = internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s);
        prepare(s, s.sizeHint());
        (void)render(s);
        QCOMPARE(internal::IndustrialSliderTestAccess::lastFeedbackReadout(s), QStringLiteral("Δ POS ▲ 95000.0 m³/h"));
        QCOMPARE(internal::IndustrialSliderTestAccess::lastFeedbackReadoutPixelSize(s), preferred);
    }
    for (const auto &range : {std::pair<double, double>{-99999.9, 99999.9}, {0.0, 100.0}}) {
        for (int position : {0, 1, 2, 3}) { // the setpoint at the minimum or the maximum, the position at ±Inf
            const bool atMinimum = position % 2 == 0;
            const bool infinite = position >= 2;
            IndustrialSlider s;
            setVertical(s, vertical);
            s.setRange(range.first, range.second);
            s.setUnit(QStringLiteral("m³/h"));
            s.setDeviationThreshold(1.0);
            const double setpoint = atMinimum ? range.first : range.second;
            const double feedback = infinite ? (atMinimum ? kInf : -kInf) : (atMinimum ? range.second : range.first);
            s.setSetpoint(setpoint);
            s.setFeedback(feedback);
            // The preferred pixel size: the one drawn with ample room
            prepare(s, s.sizeHint() * 3);
            (void)render(s);
            const int preferred = internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s);
            prepare(s, s.sizeHint());
            (void)render(s);
            const QString where = QStringLiteral("%1x%2, setpoint %3, position %4")
                                      .arg(s.width()).arg(s.height()).arg(setpoint).arg(feedback);
            const auto number = [](double v) { return QString::number(v, 'f', 1); };
            QCOMPARE(internal::IndustrialSliderTestAccess::lastSetpointReadout(s),
                     QStringLiteral("SP %1 m³/h").arg(number(setpoint)));
            const QString expected = !infinite ? QStringLiteral("Δ POS %1 m³/h").arg(number(feedback))
                                     : feedback > 0 ? QStringLiteral("? POS ▲ >%1 m³/h").arg(number(range.second))
                                                    : QStringLiteral("? POS ▼ <%1 m³/h").arg(number(range.first));
            QCOMPARE(internal::IndustrialSliderTestAccess::lastFeedbackReadout(s), expected);
            for (int px : {internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s),
                           internal::IndustrialSliderTestAccess::lastFeedbackReadoutPixelSize(s)}) {
                QVERIFY2(px == preferred,
                         qPrintable(where + QStringLiteral(": readout at %1 px, %2 px preferred").arg(px).arg(preferred)));
            }
        }
    }
}

void tst_IndustrialSlider::readoutsInsideTheirPanelsNeverBelowSevenPoints_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<int>("extra");
    for (bool vertical : {false, true}) {
        for (int extra : {0, 1, 7, 30}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 +%2").arg(vertical ? "vertical" : "horizontal").arg(extra)))
                << vertical << extra;
        }
    }
}

// From the minimum width up: the readout ink of each panel stays inside it, and the setpoint readout is never below
// 7 pt (#60), for long values and units and for the feedback states with their marks; read-only too, with the
// padlock, which is never left out (#180)
void tst_IndustrialSlider::readoutsInsideTheirPanelsNeverBelowSevenPoints()
{
    QFETCH(bool, vertical);
    QFETCH(int, extra);
    for (const bool readOnly : {false, true})
    for (double feedback : {42.0, kNaN, kInf, -1e9, 1e12}) {
        IndustrialSlider s;
        setVertical(s, vertical);
        s.setRange(-100000.0, 100000.0);
        s.setUnit(QStringLiteral("m³ (15 °C)"));
        s.setReadOnly(readOnly);
        const QSize min = s.minimumSizeHint();
        prepare(s, QSize(min.width() + extra, min.height() + (vertical ? 40 : 0)));
        s.setDeviationThreshold(1.0);
        s.setSetpoint(-99999.9);
        s.setFeedback(feedback);
        const QImage img = render(s);
        const internal::SliderLayout l = layoutOf(s);
        const QString where = QStringLiteral("%1feedback %2, %3x%4")
                                  .arg(readOnly ? "read-only, " : "").arg(feedback).arg(s.width()).arg(s.height());
        const QString sp = internal::IndustrialSliderTestAccess::lastSetpointReadout(s);
        QVERIFY2(readOnly ? sp.startsWith(QStringLiteral("🔒 SP")) : (sp.startsWith(QStringLiteral("SP")) && !sp.contains(QStringLiteral("🔒"))),
                 qPrintable(where + QStringLiteral(": ") + internal::IndustrialSliderTestAccess::lastSetpointReadout(s)));
        QVERIFY2(internal::IndustrialSliderTestAccess::lastReadoutPixelSize(s)
                     >= internal::minimumReadoutPixelSize(s.logicalDpiY()),
                 qPrintable(where));
        const double dpr = img.devicePixelRatio();
        // Readout ink (near white) outside the two panels: none
        QRectF panels = l.setpointPod.united(l.feedbackPod);
        int outside = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QPointF p((x + 0.5) / dpr, (y + 0.5) / dpr);
                const bool inPod = l.setpointPod.contains(p) || l.feedbackPod.contains(p);
                if (!inPod && near(img.pixel(x, y), kReadout, 60) && panels.adjusted(-6, -6, 6, 6).contains(p)) {
                    ++outside;
                }
            }
        }
        QVERIFY2(outside == 0, qPrintable(where + QStringLiteral(": %1 readout pixels outside the panels").arg(outside)));
        // The ink of each panel stays clear of its outline: no readout ink in the outer 1 px of the panel
        for (const QRectF &pod : {l.setpointPod, l.feedbackPod}) {
            const int all = readoutInk(img, pod);
            const int inner = readoutInk(img, pod.adjusted(1.0, 1.0, -1.0, -1.0));
            QVERIFY2(all > 0 && all == inner, qPrintable(where + QStringLiteral(": ink %1, inner %2").arg(all).arg(inner)));
        }
    }
}

void tst_IndustrialSlider::textsInsideTheWidgetFromTheMinimumUp_data()
{
    addOrientations();
}

// From minimumSizeHint() up, with wide labels: the text colour (the scale labels) only in the room of the labels,
// never on the outer pixel of the widget; the end labels drawn at the ends of the scale
void tst_IndustrialSlider::textsInsideTheWidgetFromTheMinimumUp()
{
    QFETCH(bool, vertical);
    IndustrialSlider probe;
    setVertical(probe, vertical);
    probe.setRange(-50000.0, 50000.0);
    const QSize min = probe.minimumSizeHint();
    for (int dw : {0, 1, 9, 40}) {
        for (int dh : {0, 1, 13, 50}) {
            IndustrialSlider s;
            setVertical(s, vertical);
            prepare(s, min + QSize(dw, dh));
            s.setRange(-50000.0, 50000.0);
            s.setSetpoint(12345.6);
            QCOMPARE(s.minimumSizeHint(), min);
            const QImage img = render(s);
            const internal::SliderLayout l = layoutOf(s);
            const QString where = QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
            const auto text = [](QRgb p) { return near(p, kText, 120); };
            int edge = 0;
            for (int x = 0; x < img.width(); ++x) {
                edge += text(img.pixel(x, 0)) + text(img.pixel(x, img.height() - 1));
            }
            for (int y = 0; y < img.height(); ++y) {
                edge += text(img.pixel(0, y)) + text(img.pixel(img.width() - 1, y));
            }
            QVERIFY2(edge == 0, qPrintable(where + QStringLiteral(": %1 text pixels on the edge").arg(edge)));
            // No label ink before the room of the labels (over the ticks, the band, the track)
            const QRectF beforeLabels = vertical ? QRectF(0, 0, l.labelNear - 1.0, s.height())
                                                 : QRectF(0, 0, s.width(), l.labelNear - 1.0);
            QCOMPARE(countIn(img, beforeLabels, text), 0);
            // A label at each end of the scale
            for (double end : {l.scaleMin, l.scaleMax}) {
                const QRectF room = vertical ? QRectF(l.labelNear, end - 6.0, s.width() - l.labelNear, 12.0)
                                             : QRectF(end - (end == l.scaleMin ? 0.0 : 10.0), l.labelNear, 10.0, s.height() - l.labelNear);
                QVERIFY2(countIn(img, room, text) > 0, qPrintable(where + QStringLiteral(": no label at %1").arg(end)));
            }
        }
    }
}

// The place of the feedback panel is kept: a feedback that comes or goes moves nothing else, and the minimum is the
// same
void tst_IndustrialSlider::layoutDoesNotMoveWhenAFeedbackComes()
{
    for (bool vertical : {false, true}) {
        IndustrialSlider s;
        setVertical(s, vertical);
        prepare(s, sizeFor(vertical));
        const QSize min = s.minimumSizeHint();
        (void)render(s);
        const internal::SliderLayout before = layoutOf(s);
        s.setFeedback(30.0);
        s.setDeviationThreshold(2.0);
        QCOMPARE(s.minimumSizeHint(), min);
        (void)render(s);
        const internal::SliderLayout after = layoutOf(s);
        QCOMPARE(after.setpointPod, before.setpointPod);
        QCOMPARE(after.feedbackPod, before.feedbackPod);
        QCOMPARE(after.track, before.track);
        QCOMPARE(after.labelNear, before.labelNear);
    }
}

// #51, rule (b): the labels and the readouts follow the size of the widget font; with a font twice the application
// font the tallest glyph of the labels is at least 1.5 times taller, and the minimum grows
void tst_IndustrialSlider::textFollowsTheWidgetFontSize()
{
    if (!(internal::effectivePixelSize(QApplication::font()) > 0.0)) {
        QFAIL("the application font has no size");
    }
    for (bool vertical : {false, true}) {
        IndustrialSlider base;
        setVertical(base, vertical);
        prepare(base, QSize(400, 400));
        IndustrialSlider grown;
        setVertical(grown, vertical);
        prepare(grown, QSize(400, 400));
        grown.setFont(FontTest::doubledFont());
        const int a = FontTest::textGlyphHeight(base);
        const int b = FontTest::textGlyphHeight(grown);
        QVERIFY2(a > 0 && b >= 1.5 * a, qPrintable(QStringLiteral("tallest glyph %1 px, %2 px with a font twice as large").arg(a).arg(b)));
        QVERIFY(grown.minimumSizeHint().width() > base.minimumSizeHint().width());
        QVERIFY(grown.minimumSizeHint().height() > base.minimumSizeHint().height());
    }
}

// The minimum follows what it is made of, and a change of it reaches the layout (updateGeometry())
void tst_IndustrialSlider::minimumSizeHintFollowsTheConfiguration()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *s = new IndustrialSlider;
    layout->addWidget(s);
    ScreenChange::LayoutRequests requests;
    host.installEventFilter(&requests);
    host.show(); // a hidden widget does not tell its layout
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const QSize base = s->minimumSizeHint();
    const auto check = [&](const char *what, const std::function<void()> &change, int wider, int taller) {
        const QSize before = s->minimumSizeHint();
        QCoreApplication::processEvents();
        requests.count = 0;
        change();
        QCoreApplication::processEvents();
        const QSize after = s->minimumSizeHint();
        const auto sign = [](int d) { return d > 0 ? 1 : d < 0 ? -1 : 0; };
        QVERIFY2(sign(after.width() - before.width()) == wider && sign(after.height() - before.height()) == taller,
                 qPrintable(QStringLiteral("%1: %2x%3 -> %4x%5").arg(QLatin1String(what)).arg(before.width())
                                .arg(before.height()).arg(after.width()).arg(after.height())));
        QVERIFY2(requests.count > 0, qPrintable(QStringLiteral("%1: the layout was not told").arg(QLatin1String(what))));
    };
    // Horizontal, with the readouts the two panels set the width; without them the end labels do
    check("no readouts", [&] { s->setReadoutVisible(false); }, -1, -1);
    check("wider end labels", [&] { s->setRange(0.0, 1000000.0); }, 1, 0);
    check("negative minimum", [&] { s->setRange(-1000000.0, 1000000.0); }, 1, 0);
    s->setRange(0.0, 100.0);
    check("readouts", [&] { s->setReadoutVisible(true); }, 1, 1);
    QCOMPARE(s->minimumSizeHint(), base);
    check("larger font", [&] { s->setFont(FontTest::doubledFont()); }, 1, 1);
    check("application font", [&] { s->setFont(QFont()); }, -1, -1);
    check("no feedback place", [&] { s->setFeedbackReadoutVisible(false); }, -1, 0);
    check("feedback place", [&] { s->setFeedbackReadoutVisible(true); }, 1, 0);
    QCOMPARE(s->minimumSizeHint(), base);
    check("vertical", [&] { s->setOrientation(Qt::Vertical); }, -1, 1);
    // A vertical slider stacks the panels: hiding the place of the feedback makes it lower, and narrower when the
    // essential texts of the feedback panel ("? POS ---") are what sets the width (without fonts, for instance)
    const QSize stacked = s->minimumSizeHint();
    requests.count = 0;
    s->setFeedbackReadoutVisible(false);
    QCoreApplication::processEvents();
    QVERIFY2(s->minimumSizeHint().width() <= stacked.width() && s->minimumSizeHint().height() < stacked.height(),
             qPrintable(QStringLiteral("vertical, no feedback place: %1x%2 -> %3x%4").arg(stacked.width())
                            .arg(stacked.height()).arg(s->minimumSizeHint().width()).arg(s->minimumSizeHint().height())));
    QVERIFY(requests.count > 0);
}

void tst_IndustrialSlider::screenChangeOfAVisibleSlider_data()
{
    ScreenChange::addScreens();
}

// #177: moved to a screen of another DPR or logical DPI, the slider draws and sizes itself as a new one there, and its
// layout gets the new minimum once
void tst_IndustrialSlider::screenChangeOfAVisibleSlider()
{
    QFETCH(double, dpr);
    QFETCH(int, dpi);
    const std::vector<std::pair<const char *, std::function<void(IndustrialSlider &)>>> configurations = {
        {"default", [](IndustrialSlider &) {}},
        {"deviating", [](IndustrialSlider &s) { s.setDeviationThreshold(5.0); s.setSetpoint(60.0); s.setFeedback(15.0); }},
        {"vertical, unknown", [](IndustrialSlider &s) {
             s.setOrientation(Qt::Vertical);
             s.setDeviationThreshold(5.0);
             s.setFeedback(kNaN);
         }},
        {"wide range, zones", [](IndustrialSlider &s) {
             s.setRange(-5000.0, 5000.0);
             s.setWarningThreshold(3000.0);
             s.setErrorThreshold(4000.0);
             s.setFeedback(4500.0);
         }},
    };
    int changed = 0;
    for (const auto &configuration : configurations) {
        bool minimumChanged = false;
        ScreenChange::checkScreenChange<IndustrialSlider>(configuration.second, dpr, dpi, &minimumChanged);
        if (QTest::currentTestFailed()) {
            qWarning("configuration: %s", configuration.first);
            return;
        }
        changed += minimumChanged ? 1 : 0;
    }
    // The texts are never below 7 pt: at DPI 120 and 144 the minimum grows
    if (dpi != 96) {
        QVERIFY2(changed > 0, "the minimum is the same on both screens for every configuration");
    }
}

// Nothing non-finite reaches the painter, at any size
void tst_IndustrialSlider::nonFiniteInputAndExtremeSizes()
{
    IndustrialSlider s;
    prepare(s);
    s.setDeviationThreshold(1.0);
    s.setMajorTicks(0);
    s.setMinorTicks(-1);
    QCOMPARE(s.majorTicks(), 5);
    QCOMPARE(s.minorTicks(), 4);
    s.setPrecision(-3);
    QCOMPARE(s.precision(), 0);
    for (bool vertical : {false, true}) {
        setVertical(s, vertical);
        for (QSize size : {QSize(300, 90), QSize(1, 1), QSize(0, 0), QSize(20, 300), QSize(300, 20), QSize(4000, 30)}) {
            s.resize(size);
            for (double v : {kInf, -kInf, kNaN, 1e308, -1e308, 50.0}) {
                s.setFeedback(v);
                s.setSetpoint(v == 50.0 ? 25.0 : 75.0);
                (void)render(s);
            }
        }
    }
    s.setRange(-1e300, 1e300);
    s.setFeedback(1e308);
    s.resize(300, 90);
    setVertical(s, false);
    (void)render(s);
    QCOMPARE(s.feedbackState(), ValueState::Overrange);
}

QTEST_MAIN(tst_IndustrialSlider)
#include "tst_IndustrialSlider.moc"
