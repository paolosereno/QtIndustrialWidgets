// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialKnob.h>

#include "FontTestUtils.h"
#include "LabelInkTestUtils.h"
#include "TextMetricsTestUtils.h"
#include "internal/FontScale.h"
#include "internal/IndustrialKnobTestAccess.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"

#include <array>
#include <memory>
#include <utility>
#include <QtGui/QAccessible>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <functional>
#include <vector>

using namespace QtIndustrialWidgets;

namespace {

// The label texts, boxes and selection the widget computes for its configuration
struct ScaleLabels {
    QFont font;
    std::vector<QString> texts;
    std::vector<internal::LayoutRect> boxes;
    std::vector<internal::LayoutRect> ink;
    internal::RadialScaleLabels selection;
    double minGap{0.0};
};

// The labels at a geometry
ScaleLabels scaleLabelsAt(const IndustrialKnob &knob, const internal::KnobScaleGeometry &geometry)
{
    ScaleLabels out;
    out.font = knob.font();
    out.font.setPixelSize(geometry.labelFontPx);
    out.font.setBold(true);
    const QFontMetricsF fm(out.font);
    const bool discrete = knob.mode() == IndustrialKnob::KnobMode::Discrete;
    const int ticks = std::max(1, discrete ? knob.discreteSteps() - 1 : knob.majorTicks());
    for (int i = 0; i <= ticks; ++i) {
        const double frac = static_cast<double>(i) / ticks;
        const double value = knob.minimum() + frac * (knob.maximum() - knob.minimum());
        const QString text = discrete ? QString::number(i + 1) : internal::scaleLabelText(value, knob.precision());
        const QRectF rect = fm.boundingRect(text);
        const internal::LayoutRect box = internal::labelBoxOnDevicePixel(
            internal::knobLabelBox(geometry, knob.startAngle() + frac * knob.spanAngle(), rect.width(), rect.height()),
            knob.devicePixelRatioF());
        const internal::TextInkBox ink = internal::measureCenteredTextInk(text, out.font, box.w, box.h, knob.devicePixelRatioF());
        out.texts.push_back(text);
        out.boxes.push_back(box);
        out.ink.push_back({box.x + box.w * 0.5 + ink.left, box.y + box.h * 0.5 + ink.top, ink.right - ink.left, ink.bottom - ink.top});
    }
    out.minGap = internal::scaleLabelMinGap(fm.height());
    out.selection = internal::layoutRadialScaleLabels(out.boxes, out.minGap);
    return out;
}

std::vector<internal::LayoutRect> drawnInk(const ScaleLabels &labels)
{
    std::vector<internal::LayoutRect> drawn;
    for (int i : labels.selection.drawn) {
        drawn.push_back(labels.ink[static_cast<size_t>(i)]);
    }
    return drawn;
}

internal::KnobScaleGeometry drawnGeometryOf(const IndustrialKnob &knob, int width, int height)
{
    return internal::knobScaleGeometry(width, height, [&](const internal::KnobScaleGeometry &g) {
        return drawnInk(scaleLabelsAt(knob, g));
    });
}

ScaleLabels scaleLabelsOf(const IndustrialKnob &knob)
{
    return scaleLabelsAt(knob, drawnGeometryOf(knob, knob.width(), knob.height()));
}

// The labels of a knob of that size keep the margin from the edge, and the first and last ones are drawn
bool sizeKeepsTheEnds(const IndustrialKnob &knob, int width, int height)
{
    const ScaleLabels labels = scaleLabelsAt(knob, drawnGeometryOf(knob, width, height));
    const std::vector<int> &drawn = labels.selection.drawn;
    const int last = static_cast<int>(labels.boxes.size()) - 1;
    return std::find(drawn.begin(), drawn.end(), 0) != drawn.end() && std::find(drawn.begin(), drawn.end(), last) != drawn.end()
           && internal::knobLabelsInside(drawnInk(labels), width, height);
}

// Device pixels that differ between two renders of the same size
QVector<QPoint> differingPixels(const QImage &a, const QImage &b)
{
    QVector<QPoint> out;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

QImage renderKnob(IndustrialKnob &knob)
{
    return knob.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// The dial as it is drawn, in logical px, from the pixels: its centre from the disc of the knob (the pixels of
// knobColor) and its radius from the ink of the scale (the pixels of scaleColor) farthest from that centre
struct DrawnDial {
    QPointF center;
    double radius{0.0};
    double grip{0.0}; // Half of the extent of what is drawn in knobColor
};

DrawnDial drawnDialOf(IndustrialKnob &knob)
{
    const qreal dpr = knob.devicePixelRatioF();
    knob.setKnobColor(QColor(10, 200, 30));
    const QImage first = renderKnob(knob);
    knob.setKnobColor(QColor(200, 30, 10));
    const QVector<QPoint> disc = differingPixels(first, renderKnob(knob));
    QRect box;
    for (const QPoint &p : disc) {
        box |= QRect(p, QSize(1, 1));
    }
    knob.setScaleColor(QColor(10, 200, 30));
    const QImage scaleFirst = renderKnob(knob);
    knob.setScaleColor(QColor(200, 30, 10));
    const QVector<QPoint> scale = differingPixels(scaleFirst, renderKnob(knob));

    DrawnDial dial;
    dial.center = QPointF(box.x() + box.width() * 0.5, box.y() + box.height() * 0.5) / dpr;
    dial.grip = std::max(box.width(), box.height()) * 0.5 / dpr;
    for (const QPoint &p : scale) {
        dial.radius = std::max(dial.radius, std::hypot((p.x() + 0.5) / dpr - dial.center.x(), (p.y() + 0.5) / dpr - dial.center.y()));
    }
    return dial;
}

// A knob on a screen of another DPR: moving a window to another monitor, with no second monitor in the test
class FakeDprKnob : public IndustrialKnob
{
public:
    using IndustrialKnob::IndustrialKnob;
    double fakeDpr{1.0};

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        if (metric == PdmDevicePixelRatio) {
            return static_cast<int>(std::ceil(fakeDpr));
        }
        if (metric == PdmDevicePixelRatioScaled) {
            return static_cast<int>(fakeDpr * devicePixelRatioFScale());
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // From Qt 6.8 a DPR other than 1 and 2 is read from these two, not from the scaled one
        if (metric == PdmDevicePixelRatioF_EncodedA || metric == PdmDevicePixelRatioF_EncodedB) {
            return encodeMetricF(metric, fakeDpr);
        }
#endif
        return IndustrialKnob::metric(metric);
    }
};

struct LayoutRequests : QObject {
    int count{0};
    bool eventFilter(QObject *, QEvent *event) override
    {
        count += event->type() == QEvent::LayoutRequest ? 1 : 0;
        return false;
    }
};

// The point of the dial at \a angle degrees (0 = 12 o'clock, clockwise)
QPointF dialPoint(const DrawnDial &dial, double angle)
{
    const double rad = angle * M_PI / 180.0;
    return QPointF(dial.center.x() + dial.radius * std::sin(rad), dial.center.y() - dial.radius * std::cos(rad));
}

void sendMouse(IndustrialKnob &knob, QEvent::Type type, const QPointF &point)
{
    const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type, point, knob.mapToGlobal(point), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&knob, &event);
}

void sendKey(IndustrialKnob &knob, QEvent::Type type, int key, bool autoRepeat)
{
    QKeyEvent event(type, key, Qt::NoModifier, QString(), autoRepeat);
    QCoreApplication::sendEvent(&knob, &event);
}

// The value at \a angle degrees (0 = 12 o'clock, clockwise) of a knob with the default angles
double valueAtAngle(const IndustrialKnob &knob, double angle)
{
    return knob.minimum() + (angle - knob.startAngle()) / knob.spanAngle() * (knob.maximum() - knob.minimum());
}

// The angle of \a value (0 = 12 o'clock, clockwise)
double angleOfValue(const IndustrialKnob &knob, double value)
{
    return knob.startAngle() + (value - knob.minimum()) / (knob.maximum() - knob.minimum()) * knob.spanAngle();
}

// #182: a parent that takes the events the knob passes on
class RecordingParent : public QWidget
{
public:
    int keyPresses{0};
    int mousePresses{0};

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPresses;
        event->accept();
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        ++mousePresses;
        event->accept();
    }
};

// Sends one wheel event of \a delta units (120 = one notch) to the knob and returns whether the knob accepted it.
// Qt passes an ignored wheel event from the window system on to the parent; one sent from here goes to the knob only,
// so its acceptance is what is checked
bool sendWheel(IndustrialKnob &knob, int delta)
{
    const QPointF point(knob.width() * 0.5, knob.height() * 0.5);
    QWheelEvent wheel(point, knob.mapToGlobal(point), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&knob, &wheel);
    return wheel.isAccepted();
}

// Gives \a widget the keyboard focus in its active window
bool focusOn(QWidget &widget)
{
    QWidget *window = widget.window();
    window->show();
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window)) {
        return false;
    }
    widget.setFocus(Qt::OtherFocusReason);
    return QTest::qWaitFor([&] { return widget.hasFocus(); });
}

// Device pixels of the pointer: those that change with pointerColor(). The value display, drawn in pointerColor() too,
// must be hidden
QVector<QPoint> pointerPixels(IndustrialKnob &knob)
{
    const QColor keep = knob.pointerColor();
    knob.setPointerColor(QColor(10, 200, 30));
    const QImage first = renderKnob(knob);
    knob.setPointerColor(QColor(200, 30, 10));
    const QVector<QPoint> out = differingPixels(first, renderKnob(knob));
    knob.setPointerColor(keep);
    return out;
}

// Distance of the centre of device pixel \a p from \a center, in logical px
double distanceOf(const QPoint &p, const QPointF &center, qreal dpr)
{
    return std::hypot((p.x() + 0.5) / dpr - center.x(), (p.y() + 0.5) / dpr - center.y());
}

// Angle of the centre of device pixel \a p around \a center, in degrees (0 = 12 o'clock, clockwise), in (-180, 180]
double angleOf(const QPoint &p, const QPointF &center, qreal dpr)
{
    return std::atan2((p.x() + 0.5) / dpr - center.x(), center.y() - (p.y() + 0.5) / dpr) * 180.0 / M_PI;
}

// #183: the accessibility events a knob sends while the recorder lives, seen through the test hook: under
// QT_QPA_PLATFORM=offscreen there is no accessibility backend, and Qt drops the events before any update handler
class AccessibleEvents
{
public:
    struct Event {
        QAccessible::Event type;
        QObject *object;
        double value;      // ValueChanged
        bool readOnly;     // StateChanged: the readOnly state changed
    };

    explicit AccessibleEvents(IndustrialKnob &knob)
        : m_knob(knob)
    {
        internal::IndustrialKnobTestAccess::setAccessibleObserver(knob, [this](QAccessibleEvent *event) {
            Event e{event->type(), event->object(), 0.0, false};
            if (event->type() == QAccessible::ValueChanged) {
                e.value = static_cast<QAccessibleValueChangeEvent *>(event)->value().toDouble();
            } else if (event->type() == QAccessible::StateChanged) {
                e.readOnly = static_cast<QAccessibleStateChangeEvent *>(event)->changedStates().readOnly;
            }
            m_events.push_back(e);
        });
    }
    ~AccessibleEvents() { internal::IndustrialKnobTestAccess::setAccessibleObserver(m_knob, {}); }
    AccessibleEvents(const AccessibleEvents &) = delete;
    AccessibleEvents &operator=(const AccessibleEvents &) = delete;

    // The events recorded since the last take()
    QList<Event> take() { return std::exchange(m_events, {}); }

private:
    IndustrialKnob &m_knob;
    QList<Event> m_events;
};

double lightness(QRgb p) { return (0.2126 * qRed(p) + 0.7152 * qGreen(p) + 0.0722 * qBlue(p)) / 255.0; }

int colourDistance(QRgb a, const QColor &b)
{
    return std::max({std::abs(qRed(a) - b.red()), std::abs(qGreen(a) - b.green()), std::abs(qBlue(a) - b.blue())});
}

} // namespace

class tst_IndustrialKnob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void repaintDoesNotMeasureTheLabelsAgain();
    void textSizeIgnoresWidgetFontSize();
    void fontChangeRefreshesCache();
    void labelsFollowWidgetFont();
    void defaultValues();
    void rangeAndClamping();
    void modes();
    void keyboardInteraction();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void scaleLabelsKeepMinimumGap_data();
    void scaleLabelsKeepMinimumGap();
    void scaleLabelsStayInsideTheWidget_data();
    void scaleLabelsStayInsideTheWidget();
    void dragOnTheDrawnDialMovesByTheAngle_data();
    void dragOnTheDrawnDialMovesByTheAngle();
    void pressOutsideTheDrawnDialStartsNoDrag();
    void dragCommitsOnceAtTheRelease();
    void keyStepCommitsAtTheRelease();
    void heldKeyCommitsOnceAtTheRelease_data();
    void heldKeyCommitsOnceAtTheRelease();
    void keyAtTheEndDoesNotCommit();
    void pageHomeAndEndKeysCommit();
    void setValueAndSetRangeNeverCommit();
    void focusOutOrHideEndsTheAction_data();
    void focusOutOrHideEndsTheAction();
    void pageStepFollowsStepUntilSet();
    void clickInTheGapChangesNothing();
    void dragIntoTheGapStopsAtTheNearestEnd();
    void dragPastTheMaximumDoesNotReachZero();
    void minimumSizeHintLimits();
    void minimumSizeKeepsTheEnds_data();
    void minimumSizeKeepsTheEnds();
    void minimumSizeHintMakesRoomForAWideFont();
    void minimumSizeHintMakesRoomForALabelAtTheTop();
    void minimumSizeHintFollowsConfiguration_data();
    void minimumSizeHintFollowsConfiguration();
    void minimumWithTableEqualsReference_data();
    void minimumWithTableEqualsReference();
    void minimumMeasuresEachTextOnce_data();
    void minimumMeasuresEachTextOnce();
    void minimumFormatsEachLabelOnce_data();
    void minimumFormatsEachLabelOnce();
    void minimumTableNotKeptAcrossComputations();
    void dialFollowsTheDrawnLabels_data();
    void dialFollowsTheDrawnLabels();
    void precisionChangesTheLabelsDrawn();
    void labelInkStaysInsideItsMeasure();
    void dprChangeOfAVisibleKnob_data();
    void dprChangeOfAVisibleKnob();

    // #182, PR 2: read-only, value limits, mouse wheel
    void readOnlyIgnoresTheOperatorInput();
    void readOnlyEndsTheRunningAction_data();
    void readOnlyEndsTheRunningAction();
    void readOnlyDrawsThePadlockInsideThePointer_data();
    void readOnlyDrawsThePadlockInsideThePointer();
    void valueLimitsReadWithinTheRange();
    void setValueOutsideTheLimitsIsKept();
    void operatorInputStaysInsideTheLimits();
    void fromOutsideTheLimitsOnlyInwardInputApplies();
    void discreteModeReachesOnlyTheStepsInsideTheLimits();
    void limitsAreHatchedWithStopMarks_data();
    void limitsAreHatchedWithStopMarks();
    void wheelIsOffByDefault();
    void wheelStepsOnlyWithFocus();

    // #183: focus and accessibility
    void focusRingIsASolidRingInTextColor_data();
    void focusRingIsASolidRingInTextColor();
    void focusTraversalReachesTheKnob_data();
    void focusTraversalReachesTheKnob();
    void accessibleInterfaceDescribesTheKnob();
    void accessibleNameHasNoFallback();
    void accessibleFocusState();
    void accessibleSetValueIsOperatorInput();
    void accessibleEventsFollowTheValue();
};

void tst_IndustrialKnob::defaultValues()
{
    IndustrialKnob knob;
    QCOMPARE(knob.minimum(), 0.0);
    QCOMPARE(knob.maximum(), 100.0);
    QCOMPARE(knob.value(), 25.0);
    QCOMPARE(knob.mode(), IndustrialKnob::KnobMode::Continuous);
}

void tst_IndustrialKnob::rangeAndClamping()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 50.0);

    // Below minimum
    knob.setValue(-25.0);
    QCOMPARE(knob.value(), 0.0);

    // Above maximum
    knob.setValue(75.0);
    QCOMPARE(knob.value(), 50.0);

    // Valid value
    knob.setValue(25.0);
    QCOMPARE(knob.value(), 25.0);
}

void tst_IndustrialKnob::modes()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);

    QCOMPARE(knob.mode(), IndustrialKnob::KnobMode::Discrete);
    QCOMPARE(knob.discreteSteps(), 5);

    // In discrete 5 steps over [0, 100], steps are 0, 25, 50, 75, 100
    knob.setValue(22.0);
    QCOMPARE(knob.value(), 25.0);

    knob.setValue(68.0);
    QCOMPARE(knob.value(), 75.0);
}

void tst_IndustrialKnob::keyboardInteraction()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(10.0);
    knob.setStep(5.0);

    QTest::keyClick(&knob, Qt::Key_Up);
    QCOMPARE(knob.value(), 15.0);

    QTest::keyClick(&knob, Qt::Key_Down);
    QCOMPARE(knob.value(), 10.0);
}

void tst_IndustrialKnob::signalEmission()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);

    QSignalSpy spy(&knob, &IndustrialKnob::valueChanged);
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);
}

void tst_IndustrialKnob::renderOffscreen()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(45.0);
    knob.resize(120, 120);

    QPixmap pix(knob.size());
    knob.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_IndustrialKnob::extremeResizeNoCrash()
{
    IndustrialKnob knob;
    knob.resize(1, 1);
    QPixmap pixSmall(knob.size());
    knob.render(&pixSmall);

    knob.resize(2560, 1440);
    QPixmap pixLarge(knob.size());
    knob.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_IndustrialKnob::nanAndInfinityResilience()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(50.0);
    QCOMPARE(knob.value(), 50.0);

    // #182: NaN, +Inf and -Inf are rejected with a warning, in both modes: the value and the signals do not move
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double input : {nan, inf, -inf}) {
        QTest::ignoreMessage(QtWarningMsg, qPrintable(QStringLiteral("IndustrialKnob::setValue: non-finite value %1 ignored")
                                                          .arg(QString::number(input))));
        knob.setValue(input);
        QCOMPARE(knob.value(), 50.0);
    }
    QCOMPARE(changed.count(), 0);
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);
    knob.setValue(50.0);
    const int afterDiscrete = changed.count();
    for (double input : {nan, inf, -inf}) {
        QTest::ignoreMessage(QtWarningMsg, qPrintable(QStringLiteral("IndustrialKnob::setValue: non-finite value %1 ignored")
                                                          .arg(QString::number(input))));
        knob.setValue(input);
        QCOMPARE(knob.value(), 50.0);
    }
    QCOMPARE(changed.count(), afterDiscrete);

    // Range with NaN/Inf ignored
    knob.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(knob.minimum(), 0.0);

    // Offscreen render check
    knob.resize(150, 150);
    QPixmap pix(knob.size());
    knob.render(&pix);
    QVERIFY(!pix.isNull());
}

// #51: the scale labels (value display hidden) are drawn into the static cache with the widget font (family, style, spacing),
// not with the application font: a widget created with another font of the same size draws other
// text pixels. Before #51 the cached text used painter.font() (the application font on a pixmap)
void tst_IndustrialKnob::labelsFollowWidgetFont()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->setValueDisplayVisible(false);
        g->resize(220, 240);
        return g;
    };
    auto standard = make(nullptr);
    const QFont other = FontTest::otherFont(standard->font());
    auto changed = make(&other);
    const int pixels = FontTest::differingPixelCount(FontTest::render(*standard), FontTest::render(*changed));
    QVERIFY2(pixels > 100, qPrintable(QStringLiteral("only %1 pixels change with the widget font").arg(pixels)));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_IndustrialKnob::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->resize(220, 240);
        return g;
    };
    // Shown widgets: grab() on a hidden one delivers a pending resize event, which refreshes the
    // cache by itself and hides the defect
    auto painted = make(nullptr);
    painted->show();
    QVERIFY(QTest::qWaitForWindowExposed(painted.get()));
    const QFont other = FontTest::otherFont(painted->font());
    const QImage before = FontTest::render(*painted);
    painted->setFont(other);
    QCoreApplication::processEvents();
    const QImage after = FontTest::render(*painted);
    // One window at a time: the active one has the focus (IndustrialKnob draws a focus ring)
    painted->hide();
    auto fresh = make(&other);
    fresh->show();
    QVERIFY(QTest::qWaitForWindowExposed(fresh.get()));
    const QImage expected = FontTest::render(*fresh);
    QVERIFY2(FontTest::differingPixelCount(before, expected) > 100, "the font change is not visible: the test checks nothing");
    const int stale = FontTest::differingPixelCount(after, expected);
    QVERIFY2(stale == 0, qPrintable(QStringLiteral("%1 pixels differ from a widget created with the new font").arg(stale)));
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_IndustrialKnob::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->setValueDisplayVisible(false);
        g->resize(220, 240);
        return g;
    };
    auto standard = make(nullptr);
    const QFont doubled = FontTest::doubledFont();
    auto large = make(&doubled);
    const int base = FontTest::textGlyphHeight(*standard);
    const int grown = FontTest::textGlyphHeight(*large);
    QVERIFY2(base > 0, "no text drawn in textColor");
    QVERIFY2(grown == base, qPrintable(QStringLiteral("tallest glyph %1 px with the default font, %2 px with a font twice as large").arg(base).arg(grown)));
}

void tst_IndustrialKnob::scaleLabelsStayInsideTheWidget_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("discreteSteps"); // 0: continuous

    struct Size { int w; int h; };
    struct Range { const char *name; double minimum; double maximum; };
    for (const Size size : {Size{80, 90}, Size{100, 112}, Size{120, 135}, Size{160, 180}, Size{220, 240}, Size{300, 330}, Size{400, 300}}) {
        for (const Range &r : {Range{"0-100", 0.0, 100.0}, Range{"0-8000", 0.0, 8000.0}, Range{"-1000-1000", -1000.0, 1000.0},
                               Range{"0-100000", 0.0, 100000.0}, Range{"0-10000000", 0.0, 10000000.0}}) {
            QTest::addRow("%dx%d, %s", size.w, size.h, r.name) << size.w << size.h << r.minimum << r.maximum << 0;
        }
        for (int steps : {5, 12}) {
            QTest::addRow("%dx%d, discrete %d", size.w, size.h, steps) << size.w << size.h << 0.0 << 100.0 << steps;
        }
    }
}

// #170: the rule of #44: from minimumSizeHint() up no scale label reaches the edge of the widget, where it would
// be cut. All the ink of the labels, from two renders that differ only in textColor
void tst_IndustrialKnob::scaleLabelsStayInsideTheWidget()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, discreteSteps);

    IndustrialKnob knob;
    knob.setRange(minimum, maximum);
    if (discreteSteps > 0) {
        knob.setMode(IndustrialKnob::KnobMode::Discrete);
        knob.setDiscreteSteps(discreteSteps);
    }
    knob.resize(width, height);
    if (width < knob.minimumSizeHint().width() || height < knob.minimumSizeHint().height()) {
        return; // below the supported size the rule does not apply
    }

    knob.setTextColor(QColor(255, 0, 255));
    const QImage first = renderKnob(knob);
    knob.setTextColor(QColor(255, 255, 0));
    const QVector<QPoint> ink = differingPixels(first, renderKnob(knob));
    QVERIFY(ink.size() > 20);
    // The margin of 1 logical px, counted on whole device pixels, as the ink is measured
    const int margin = static_cast<int>(std::ceil(knob.devicePixelRatioF()));
    int onEdge = 0;
    for (const QPoint &p : ink) {
        onEdge += (p.x() < margin || p.y() < margin || p.x() >= first.width() - margin || p.y() >= first.height() - margin) ? 1 : 0;
    }
    QVERIFY2(onEdge == 0, qPrintable(QStringLiteral("%1 pixels of label ink in the margin of the widget").arg(onEdge)));
}

void tst_IndustrialKnob::dragOnTheDrawnDialMovesByTheAngle_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("maximum");
    struct Size { int w; int h; };
    for (const Size size : {Size{220, 240}, Size{160, 180}, Size{400, 300}, Size{300, 400}}) {
        QTest::addRow("%dx%d, 0-100", size.w, size.h) << size.w << size.h << 100.0;
        QTest::addRow("%dx%d, 0-10000000", size.w, size.h) << size.w << size.h << 10000000.0;
    }
}

// #170, #182: a press changes nothing; the drag that follows moves the value by the angle the pointer moves around
// the dial that is drawn. The points are taken from the pixels of the dial, not from its formulas: on its rim, away
// from the angle of the value. 1.5 px across the radius covers the centre measured
void tst_IndustrialKnob::dragOnTheDrawnDialMovesByTheAngle()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, maximum);

    IndustrialKnob knob;
    knob.setRange(0.0, maximum);
    knob.setStep(maximum / 100000.0);
    knob.setPrecision(0);
    knob.setValueDisplayVisible(false);
    knob.resize(width, height);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));

    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    const double tolerance = maximum * (1.5 / dial.radius * 180.0 / M_PI) / knob.spanAngle();
    struct Drag { double press; double to; };
    for (const Drag drag : {Drag{-90.0, -45.0}, Drag{-45.0, 45.0}, Drag{90.0, 0.0}, Drag{45.0, -90.0}}) {
        knob.setValue(maximum * 0.5); // At 0 degrees: every press is away from the value
        QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
        QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
        sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, drag.press));
        QCOMPARE(knob.value(), maximum * 0.5);
        QCOMPARE(changed.count(), 0);
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, (drag.press + drag.to) * 0.5));
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, drag.to));
        const double expected = maximum * 0.5 + maximum * (drag.to - drag.press) / knob.spanAngle();
        QVERIFY2(std::abs(knob.value() - expected) <= tolerance,
                 qPrintable(QStringLiteral("drag %1 -> %2: value %3, expected %4 +/- %5 (dial at %6, %7, radius %8)")
                                .arg(drag.press).arg(drag.to).arg(knob.value()).arg(expected).arg(tolerance)
                                .arg(dial.center.x()).arg(dial.center.y()).arg(dial.radius)));
        sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, drag.to));
        QCOMPARE(committed.count(), 1);
        QCOMPARE(committed.at(0).at(0).toDouble(), knob.value());
    }
}

// #182: a press counts only on the dial as it is drawn, within the radius of its scale geometry. Outside it, on the
// side of the labels, a press and a drag change nothing; inside it they do
void tst_IndustrialKnob::pressOutsideTheDrawnDialStartsNoDrag()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    // dial.radius is the ink of the ticks, at 0.92 of the radius of the geometry: 1.2 times it is outside the dial
    const auto pointAt = [&](double angle, double radius) {
        const double rad = angle * M_PI / 180.0;
        return QPointF(dial.center.x() + radius * std::sin(rad), dial.center.y() - radius * std::cos(rad));
    };
    const QPointF outside = pointAt(90.0, dial.radius * 1.2);
    QVERIFY(QRectF(knob.rect()).contains(outside));

    knob.setValue(50.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    sendMouse(knob, QEvent::MouseButtonPress, outside);
    sendMouse(knob, QEvent::MouseMove, pointAt(45.0, dial.radius * 0.8));
    sendMouse(knob, QEvent::MouseMove, pointAt(0.0, dial.radius * 0.8));
    sendMouse(knob, QEvent::MouseButtonRelease, pointAt(0.0, dial.radius * 0.8));
    QCOMPARE(knob.value(), 50.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);

    // The same drag from inside the dial moves the value by the angle: -90 degrees of 270 on 0-100
    sendMouse(knob, QEvent::MouseButtonPress, pointAt(90.0, dial.radius * 0.8));
    sendMouse(knob, QEvent::MouseMove, pointAt(45.0, dial.radius * 0.8));
    sendMouse(knob, QEvent::MouseMove, pointAt(0.0, dial.radius * 0.8));
    sendMouse(knob, QEvent::MouseButtonRelease, pointAt(0.0, dial.radius * 0.8));
    QVERIFY2(std::abs(knob.value() - (50.0 - 100.0 / 3.0)) < 1.0, qPrintable(QString::number(knob.value())));
    QCOMPARE(committed.count(), 1);
}

// #182: valueChanged() at every move of a drag, valueCommitted() once at the release, and none for a drag that
// ends on the value it started from
void tst_IndustrialKnob::dragCommitsOnceAtTheRelease()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);

    knob.setValue(50.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    for (double angle : {30.0, 60.0, 90.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
    }
    QCOMPARE(changed.count(), 3);
    QCOMPARE(committed.count(), 0);
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), knob.value());
    QVERIFY2(std::abs(knob.value() - 83.3) < 1.0, qPrintable(QString::number(knob.value())));

    // Away and back to where it started: changes, but nothing to commit
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 90.0));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 60.0));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    QCOMPARE(changed.count(), 5);
    QCOMPARE(committed.count(), 1);
}

// #182: a single key step changes the value at the press and commits it at the release
void tst_IndustrialKnob::keyStepCommitsAtTheRelease()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(50.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);

    sendKey(knob, QEvent::KeyPress, Qt::Key_Up, false);
    QCOMPARE(knob.value(), 51.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(committed.count(), 0);
    sendKey(knob, QEvent::KeyRelease, Qt::Key_Up, false);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), 51.0);

    QTest::keyClick(&knob, Qt::Key_Left);
    QCOMPARE(knob.value(), 50.0);
    QCOMPARE(committed.count(), 2);
    QCOMPARE(committed.at(1).at(0).toDouble(), 50.0);
}

void tst_IndustrialKnob::heldKeyCommitsOnceAtTheRelease_data()
{
    QTest::addColumn<bool>("releasePairs");
    QTest::newRow("press and release pairs (xcb)") << true;
    QTest::newRow("presses only (Windows, macOS)") << false;
}

// #182: a held key steps at each auto-repeat and commits once, at the release that is not an auto-repeat
void tst_IndustrialKnob::heldKeyCommitsOnceAtTheRelease()
{
    QFETCH(bool, releasePairs);

    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(50.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);

    sendKey(knob, QEvent::KeyPress, Qt::Key_Up, false);
    for (int i = 0; i < 3; ++i) {
        if (releasePairs) {
            sendKey(knob, QEvent::KeyRelease, Qt::Key_Up, true);
        }
        sendKey(knob, QEvent::KeyPress, Qt::Key_Up, true);
    }
    QCOMPARE(knob.value(), 54.0);
    QCOMPARE(changed.count(), 4);
    QCOMPARE(committed.count(), 0);
    sendKey(knob, QEvent::KeyRelease, Qt::Key_Up, false);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), 54.0);
}

// #182: a key that changes nothing (at an end) commits nothing
void tst_IndustrialKnob::keyAtTheEndDoesNotCommit()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(100.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    for (int key : {Qt::Key_Up, Qt::Key_Right, Qt::Key_PageUp, Qt::Key_End}) {
        QTest::keyClick(&knob, static_cast<Qt::Key>(key));
    }
    QCOMPARE(knob.value(), 100.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
}

// #182: PageUp/PageDown move by pageStep(), Home/End go to the ends, each committed at its release
void tst_IndustrialKnob::pageHomeAndEndKeysCommit()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(2.0);
    knob.setValue(50.0);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    struct Step { Qt::Key key; double value; };
    int commits = 0;
    for (const Step step : {Step{Qt::Key_PageUp, 60.0}, Step{Qt::Key_PageDown, 50.0}, Step{Qt::Key_Home, 0.0},
                            Step{Qt::Key_End, 100.0}}) {
        QTest::keyClick(&knob, step.key);
        QCOMPARE(knob.value(), step.value);
        QCOMPARE(committed.count(), ++commits);
        QCOMPARE(committed.last().at(0).toDouble(), step.value);
    }
}

// #182: setValue() and the clamping of setRange() emit valueChanged() but never valueCommitted()
void tst_IndustrialKnob::setValueAndSetRangeNeverCommit()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    knob.setValue(70.0);
    knob.setRange(0.0, 50.0);
    QCOMPARE(knob.value(), 50.0);
    QVERIFY(changed.count() >= 2);
    QCOMPARE(committed.count(), 0);
}

void tst_IndustrialKnob::focusOutOrHideEndsTheAction_data()
{
    QTest::addColumn<bool>("drag");
    QTest::addColumn<bool>("hide");
    QTest::newRow("drag, focus out") << true << false;
    QTest::newRow("drag, hide") << true << true;
    QTest::newRow("held key, focus out") << false << false;
    QTest::newRow("held key, hide") << false << true;
}

// #182: a drag or a held key that loses the focus or the window ends there and commits once; the release that
// comes later, and the moves before it, do nothing
void tst_IndustrialKnob::focusOutOrHideEndsTheAction()
{
    QFETCH(bool, drag);
    QFETCH(bool, hide);

    IndustrialKnob knob;
    // A focused widget that is hidden also loses the focus, so the hide rows use a knob that never has it: there
    // only the hide can end the action
    if (hide) {
        knob.setFocusPolicy(Qt::NoFocus);
    }
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    knob.setValue(50.0);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);

    if (drag) {
        sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    } else {
        sendKey(knob, QEvent::KeyPress, Qt::Key_PageUp, false);
        sendKey(knob, QEvent::KeyPress, Qt::Key_PageUp, true);
    }
    const double moved = knob.value();
    QVERIFY(moved > 50.0);
    QCOMPARE(committed.count(), 0);

    if (hide) {
        knob.hide();
    } else {
        QFocusEvent focusOut(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
        QCoreApplication::sendEvent(&knob, &focusOut);
    }
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), moved);

    if (drag) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0));
        QCOMPARE(knob.value(), moved);
        sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    } else {
        sendKey(knob, QEvent::KeyRelease, Qt::Key_PageUp, false);
    }
    QCOMPARE(committed.count(), 1);
}

// #182: pageStep() is 5 x step() until it is set, keeps its value when step() changes, and follows step() again after
// resetPageStep(). Non-finite and non-positive values are ignored
void tst_IndustrialKnob::pageStepFollowsStepUntilSet()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    QCOMPARE(knob.pageStep(), 5.0);
    knob.setStep(2.0);
    QCOMPARE(knob.pageStep(), 10.0);

    QSignalSpy appearance(&knob, &IndustrialKnob::appearanceChanged);
    knob.setPageStep(3.0);
    QCOMPARE(knob.pageStep(), 3.0);
    QCOMPARE(appearance.count(), 1);
    knob.setPageStep(3.0);
    QCOMPARE(appearance.count(), 1);
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        knob.setPageStep(invalid);
        QCOMPARE(knob.pageStep(), 3.0);
    }
    QCOMPARE(appearance.count(), 1);

    knob.setValue(50.0);
    QTest::keyClick(&knob, Qt::Key_PageUp);
    QCOMPARE(knob.value(), 53.0);
    knob.setStep(4.0);
    QCOMPARE(knob.pageStep(), 3.0);

    const int before = appearance.count();
    knob.resetPageStep();
    QCOMPARE(knob.pageStep(), 20.0);
    QCOMPARE(appearance.count(), before + 1);
    QTest::keyClick(&knob, Qt::Key_PageDown);
    QCOMPARE(knob.value(), 33.0);
}

// #175: a press below the dial, between the two ends of the 270 degrees scale, changes nothing and starts no drag
void tst_IndustrialKnob::clickInTheGapChangesNothing()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);

    knob.setValue(50.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    for (double angle : {137.0, 150.0, 170.0, 190.0, 210.0, 223.0}) {
        sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, angle));
        QCOMPARE(knob.value(), 50.0);
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 120.0));
        QCOMPARE(knob.value(), 50.0);
        sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 120.0));
        QCOMPARE(knob.value(), 50.0);
    }
    QCOMPARE(changed.count(), 0);

    // The press that follows, on the scale, still works
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    QVERIFY(std::abs(knob.value() - 50.0) < 1.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    QVERIFY2(std::abs(knob.value() - 83.3) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
}

// #175: from either end, on both sides of the middle of the gap, the value stays at the end the drag reached
void tst_IndustrialKnob::dragIntoTheGapStopsAtTheNearestEnd()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);

    // #182: the drag is relative, so the value is set first and the press is at its angle
    knob.setValue(valueAtAngle(knob, 120.0));
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 120.0));
    QVERIFY(knob.value() > 90.0);
    for (double angle : {137.0, 150.0, 170.0, 190.0, 210.0, 223.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
        QVERIFY2(knob.value() == 100.0, qPrintable(QStringLiteral("angle %1: %2").arg(angle).arg(knob.value())));
    }
    // Back on the scale the way it came, the value follows the pointer again
    for (double angle : {170.0, 137.0, 120.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
    }
    QVERIFY2(std::abs(knob.value() - 94.4) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 120.0));

    knob.setValue(valueAtAngle(knob, -120.0));
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, -120.0));
    QVERIFY(knob.value() < 10.0);
    for (double angle : {-137.0, -150.0, -170.0, 170.0, 150.0, 137.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
        QVERIFY2(knob.value() == 0.0, qPrintable(QStringLiteral("angle %1: %2").arg(angle).arg(knob.value())));
    }
    for (double angle : {170.0, -150.0, -120.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
    }
    QVERIFY2(std::abs(knob.value() - 5.6) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, -120.0));
}

// #175: past the maximum, across the gap and on along the scale, the value stays at the maximum. In steps or with a
// single jump over the gap, as a fast mouse does. It moves again only when the pointer is back on the end it holds
void tst_IndustrialKnob::dragPastTheMaximumDoesNotReachZero()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);

    // #182: the drag is relative, so the value is set first and the press is at its angle
    knob.setValue(valueAtAngle(knob, 130.0));
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 130.0));
    const double start = knob.value();
    QVERIFY(start > 95.0);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    // In one jump over the gap
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, -120.0));
    QCOMPARE(knob.value(), 100.0);
    // On along the scale, then along the whole of it and back on the maximum
    for (double angle : {-90.0, -45.0, 0.0, 45.0, 90.0, 130.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
        QVERIFY2(knob.value() == 100.0, qPrintable(QStringLiteral("angle %1: %2").arg(angle).arg(knob.value())));
    }
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 134.0));
    QCOMPARE(knob.value(), 100.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 136.0));
    QCOMPARE(knob.value(), 100.0);
    // One full turn: the pointer is on the maximum again and the way back along the scale moves the value
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    QVERIFY2(std::abs(knob.value() - 83.3) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    for (const QList<QVariant> &signal : changed) {
        QVERIFY2(signal.at(0).toDouble() > 80.0, qPrintable(signal.at(0).toString()));
    }

    // The same past the minimum, the other way round
    knob.setValue(valueAtAngle(knob, -130.0));
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, -130.0));
    QVERIFY(knob.value() < 5.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 120.0));
    QCOMPARE(knob.value(), 0.0);
    for (double angle : {90.0, 45.0, 0.0, -45.0, -90.0, -130.0, -134.0, -136.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
        QVERIFY2(knob.value() == 0.0, qPrintable(QStringLiteral("angle %1: %2").arg(angle).arg(knob.value())));
    }
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, -90.0));
    QVERIFY2(std::abs(knob.value() - 16.7) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, -90.0));
}

// #170: the limits of minimumSizeHint(): not below the former fixed 80 x 90, not above sizeHint()
void tst_IndustrialKnob::minimumSizeHintLimits()
{
    IndustrialKnob knob;
    QCOMPARE(knob.minimumSizeHint(), QSize(80, 90));
    QCOMPARE(knob.sizeHint(), QSize(160, 180));
    for (double span : {180.0, 270.0, 360.0}) {
        for (double maximum : {1.0, 100.0, 1e7, 1e15, 1e30}) {
            for (int precision : {0, 6}) {
                IndustrialKnob other;
                other.setRange(0.0, maximum);
                other.setPrecision(precision);
                other.setSpanAngle(span);
                const QSize hint = other.minimumSizeHint();
                QVERIFY2(hint.width() >= 80 && hint.width() <= 160 && hint.height() == (hint.width() * 9 + 7) / 8,
                         qPrintable(QStringLiteral("span %1, 0-%2, precision %3: %4 x %5")
                                        .arg(span).arg(maximum).arg(precision).arg(hint.width()).arg(hint.height())));
            }
        }
    }
    // On a full turn the first and last labels lie on each other, at every size: the first one is enough
    IndustrialKnob turn;
    turn.setSpanAngle(360.0);
    QCOMPARE(turn.minimumSizeHint(), QSize(80, 90));
}

void tst_IndustrialKnob::minimumSizeKeepsTheEnds_data()
{
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("discreteSteps"); // 0: continuous mode
    struct Range { const char *name; double min; double max; };
    for (const Range &r : {Range{"0-100", 0.0, 100.0}, Range{"0-8000", 0.0, 8000.0}, Range{"-1000-1000", -1000.0, 1000.0},
                           Range{"0-100000", 0.0, 100000.0}, Range{"0-1000000", 0.0, 1000000.0},
                           Range{"0-10000000", 0.0, 10000000.0}}) {
        QTest::addRow("%s", r.name) << r.min << r.max << 0;
    }
    for (int steps : {5, 12, 41}) {
        QTest::addRow("%d discrete steps", steps) << 0.0 << 100.0 << steps;
    }
}

// #170: from minimumSizeHint() up the labels stay inside and the first and last ones are drawn, also where one
// side only grows
void tst_IndustrialKnob::minimumSizeKeepsTheEnds()
{
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, discreteSteps);

    IndustrialKnob knob;
    knob.setRange(minimum, maximum);
    if (discreteSteps > 0) {
        knob.setMode(IndustrialKnob::KnobMode::Discrete);
        knob.setDiscreteSteps(discreteSteps);
    }
    const QSize hint = knob.minimumSizeHint();
    for (const QSize &extra : {QSize(0, 0), QSize(1, 0), QSize(0, 1), QSize(5, 5), QSize(20, 0), QSize(0, 20), QSize(20, 23),
                               QSize(40, 45), QSize(80, 90), QSize(200, 0), QSize(0, 200)}) {
        const QSize size = hint + extra;
        QVERIFY2(sizeKeepsTheEnds(knob, size.width(), size.height()),
                 qPrintable(QStringLiteral("%1 x %2, minimum %3 x %4").arg(size.width()).arg(size.height()).arg(hint.width()).arg(hint.height())));
    }
}

// #170: a font too wide for 80 x 90 raises minimumSizeHint() to the smallest size that keeps the labels, and no further
void tst_IndustrialKnob::minimumSizeHintMakesRoomForAWideFont()
{
    int checked = 0;
    for (double maximum : {1e6, 1e7, 1e9, 1e12}) {
        for (int stretch : {100, 150, 200, 250, 300, 400}) {
            IndustrialKnob knob;
            knob.setRange(0.0, maximum);
            QFont wide = knob.font();
            wide.setStretch(stretch);
            knob.setFont(wide);

            const int hint = knob.minimumSizeHint().width();
            // Where the ends do not fit even at the ceiling only the first one is required
            if (hint <= 80 || hint >= 160 || !sizeKeepsTheEnds(knob, 160, 180)) {
                continue;
            }
            ++checked;
            QVERIFY2(sizeKeepsTheEnds(knob, hint, (hint * 9 + 7) / 8),
                     qPrintable(QStringLiteral("0-%1, stretch %2 at %3").arg(maximum).arg(stretch).arg(hint)));
            QVERIFY2(!sizeKeepsTheEnds(knob, hint - 1, ((hint - 1) * 9 + 7) / 8),
                     qPrintable(QStringLiteral("0-%1, stretch %2: %3 is enough, the minimum is %4").arg(maximum).arg(stretch).arg(hint - 1).arg(hint)));
        }
    }
    QVERIFY2(checked > 0, "no font gives a size between the floor and the ceiling");
}

// #170: a label too wide for 80 x 90 raises minimumSizeHint() also where the ends are not the limit: on a full turn the
// only label drawn is the first one, at the top, and the dial cannot help
void tst_IndustrialKnob::minimumSizeHintMakesRoomForALabelAtTheTop()
{
    int checked = 0;
    for (int stretch : {100, 150, 200, 300}) {
        IndustrialKnob knob;
        knob.setRange(-9e18, 9e18);
        knob.setStartAngle(0.0);
        knob.setSpanAngle(360.0);
        QFont wide = knob.font();
        wide.setStretch(stretch);
        knob.setFont(wide);

        const int hint = knob.minimumSizeHint().width();
        const auto inside = [&](int width) {
            const int height = (width * 9 + 7) / 8;
            return internal::knobLabelsInside(drawnInk(scaleLabelsAt(knob, drawnGeometryOf(knob, width, height))), width, height);
        };
        if (hint <= 80 || hint >= 160) {
            continue;
        }
        ++checked;
        QVERIFY2(inside(hint) && !inside(hint - 1), qPrintable(QStringLiteral("stretch %1 at %2").arg(stretch).arg(hint)));
    }
    QVERIFY2(checked > 0, "no stretched font gives a size between the floor and the ceiling");
}

void tst_IndustrialKnob::minimumSizeHintFollowsConfiguration_data()
{
    using Change = std::function<void(IndustrialKnob &)>;
    QTest::addColumn<Change>("prepare");
    QTest::addColumn<Change>("change");
    QTest::addColumn<bool>("hintChanges");

    const Change nothing = [](IndustrialKnob &) {};
    const Change longRange = [](IndustrialKnob &k) { k.setRange(0.0, 1e12); };
    QTest::addRow("range") << nothing << Change([](IndustrialKnob &k) { k.setRange(0.0, 1e12); }) << true;
    QTest::addRow("maximum") << nothing << Change([](IndustrialKnob &k) { k.setMaximum(1e12); }) << true;
    QTest::addRow("precision") << Change([](IndustrialKnob &k) { k.setRange(0.0, 123456789.5); })
                               << Change([](IndustrialKnob &k) { k.setPrecision(1); }) << true;
    QTest::addRow("mode") << longRange << Change([](IndustrialKnob &k) { k.setMode(IndustrialKnob::KnobMode::Discrete); }) << true;
    QTest::addRow("span angle") << longRange << Change([](IndustrialKnob &k) { k.setSpanAngle(360.0); }) << true;
    QTest::addRow("start angle") << longRange << Change([](IndustrialKnob &k) { k.setStartAngle(-90.0); }) << false;
    QTest::addRow("major ticks") << longRange << Change([](IndustrialKnob &k) { k.setMajorTicks(4); }) << false;
    QTest::addRow("discrete steps") << Change([](IndustrialKnob &k) { k.setMode(IndustrialKnob::KnobMode::Discrete); })
                                    << Change([](IndustrialKnob &k) { k.setDiscreteSteps(100); }) << false;
    QTest::addRow("font") << longRange << Change([](IndustrialKnob &k) {
        QFont wide = k.font();
        wide.setStretch(200);
        k.setFont(wide);
    }) << false; // Without fonts no property of the font changes the metrics at a pixel size
}

// #170: minimumSizeHint() is that of a new knob with the same configuration after every setter it depends on, and the
// layout that holds the knob sees it without being invalidated by hand (updateGeometry())
void tst_IndustrialKnob::minimumSizeHintFollowsConfiguration()
{
    using Change = std::function<void(IndustrialKnob &)>;
    QFETCH(Change, prepare);
    QFETCH(Change, change);
    QFETCH(bool, hintChanges);

    QWidget parent;
    auto *layout = new QVBoxLayout(&parent);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *knob = new IndustrialKnob(&parent);
    prepare(*knob);
    layout->addWidget(knob);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));

    const int before = knob->minimumSizeHint().width();
    QCOMPARE(layout->minimumSize().width(), before);

    // Also where the value stays the same: a setter that does not call updateGeometry() posts no request
    QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
    LayoutRequests requests;
    parent.installEventFilter(&requests);
    change(*knob);
    QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
    parent.removeEventFilter(&requests);
    QVERIFY(requests.count > 0);
    const int after = knob->minimumSizeHint().width();

    IndustrialKnob fresh;
    prepare(fresh);
    change(fresh);
    QCOMPARE(after, fresh.minimumSizeHint().width());
    if (hintChanges) {
        QVERIFY2(after != before, qPrintable(QStringLiteral("still %1").arg(after)));
    }
    QCOMPARE(layout->minimumSize().width(), after);
}

// #184: configurations for the checks of the text-metrics table of minimumSizeHint(): ranges, precisions, the
// discrete mode, and fonts that differ from the application font in more than family, weight and style, at the
// same pixel size
static void addMinimumTableRows()
{
    using Setup = std::function<void(IndustrialKnob &)>;
    QTest::addColumn<Setup>("setup");
    const QFont app = QApplication::font();
    QTest::addRow("default") << Setup([](IndustrialKnob &) {});
    QTest::addRow("0 to 1e12, precision 3, 20 ticks") << Setup([](IndustrialKnob &k) {
        k.setRange(0.0, 1e12);
        k.setPrecision(3);
        k.setMajorTicks(20);
    });
    QTest::addRow("-1e12 to 1e12, precision 0") << Setup([](IndustrialKnob &k) {
        k.setRange(-1e12, 1e12);
        k.setPrecision(0);
    });
    // Above the floor of 80 px in all 8 configurations, also with letter spacing (minimumTableNotKeptAcrossComputations)
    QTest::addRow("-1e6 to 1e6, precision 0, letter spacing") << Setup([app](IndustrialKnob &k) {
        k.setRange(-1e6, 1e6);
        k.setPrecision(0);
        k.setFont(TextMetricsTest::spacedFont(app));
    });
    QTest::addRow("discrete, 12 steps") << Setup([](IndustrialKnob &k) {
        k.setMode(IndustrialKnob::KnobMode::Discrete);
        k.setDiscreteSteps(12);
    });
    QTest::addRow("letter spacing") << Setup([app](IndustrialKnob &k) { k.setFont(TextMetricsTest::spacedFont(app)); });
    QTest::addRow("stretch") << Setup([app](IndustrialKnob &k) {
        QFont f = app;
        f.setStretch(QFont::ExtraExpanded);
        k.setFont(f);
    });
    QTest::addRow("capitals") << Setup([app](IndustrialKnob &k) {
        QFont f = app;
        f.setCapitalization(QFont::AllUppercase);
        k.setFont(f);
    });
    QTest::addRow("no kerning") << Setup([app](IndustrialKnob &k) {
        QFont f = app;
        f.setKerning(false);
        k.setFont(f);
    });
    QTest::addRow("other family, italic") << Setup([app](IndustrialKnob &k) { k.setFont(FontTest::otherFont(app)); });
    QTest::addRow("doubled font") << Setup([](IndustrialKnob &k) { k.setFont(FontTest::doubledFont()); });
}

void tst_IndustrialKnob::minimumWithTableEqualsReference_data()
{
    addMinimumTableRows();
}

// #184: minimumSizeHint() with its table (each label measured once in the width and radius searches) is the same
// as the reference that measures every label at every size tried, as before the table
void tst_IndustrialKnob::minimumWithTableEqualsReference()
{
    using Setup = std::function<void(IndustrialKnob &)>;
    using Access = internal::IndustrialKnobTestAccess;
    QFETCH(Setup, setup);
    IndustrialKnob withTable;
    setup(withTable);
    IndustrialKnob reference;
    setup(reference);
    Access::setMinimumWithoutTable(reference, true);
    QCOMPARE(withTable.minimumSizeHint(), reference.minimumSizeHint());
    // The table was used, and the reference did not use it
    QVERIFY(!Access::lastMinimumMeasures(withTable).empty());
    QVERIFY(Access::lastMinimumMeasures(reference).empty());
}

void tst_IndustrialKnob::minimumMeasuresEachTextOnce_data()
{
    addMinimumTableRows();
}

// #184: one computation of minimumSizeHint() measures each label at each font size once
void tst_IndustrialKnob::minimumMeasuresEachTextOnce()
{
    using Setup = std::function<void(IndustrialKnob &)>;
    QFETCH(Setup, setup);
    IndustrialKnob knob;
    setup(knob);
    (void)knob.minimumSizeHint();
    const auto log = internal::IndustrialKnobTestAccess::lastMinimumMeasures(knob);
    QVERIFY(!log.empty());
    const QString repeated = TextMetricsTest::repeatedMeasure(log);
    QVERIFY2(repeated.isEmpty(), qPrintable(repeated));
}

void tst_IndustrialKnob::minimumFormatsEachLabelOnce_data()
{
    addMinimumTableRows();
}

// #184: one computation of minimumSizeHint() formats each label text once, not at every width and radius tried
void tst_IndustrialKnob::minimumFormatsEachLabelOnce()
{
    using Setup = std::function<void(IndustrialKnob &)>;
    using Access = internal::IndustrialKnobTestAccess;
    QFETCH(Setup, setup);
    IndustrialKnob knob;
    setup(knob);
    (void)knob.minimumSizeHint();
    const int ticks = knob.mode() == IndustrialKnob::KnobMode::Discrete ? std::max(1, knob.discreteSteps() - 1)
                                                                          : std::max(1, knob.majorTicks());
    QCOMPARE(Access::lastMinimumLabelFormats(knob), static_cast<quint64>(ticks) + 1);
    // The search laid out sizes from the table
    QVERIFY(!Access::lastMinimumMeasures(knob).empty());
}

// #184: the table lives for one computation. After a font change at the same pixel sizes, minimumSizeHint() is
// that of the reference with the new font, not one measured with the labels of the old font. The change is one of
// letter spacing: a change of family (the test font of tests/fonts) leaves the minimum of this knob at the floor
// at QT_SCALE_FACTOR 2 and without installed fonts, where it would check nothing
void tst_IndustrialKnob::minimumTableNotKeptAcrossComputations()
{
    // Labels that decide the size: the default knob stops at the floor of 80 px, which no font change moves
    const auto configure = [](IndustrialKnob &k) {
        k.setRange(-1e6, 1e6);
        k.setPrecision(0);
    };
    IndustrialKnob knob;
    configure(knob);
    const QSize first = knob.minimumSizeHint();
    const QFont other = TextMetricsTest::spacedFont(QApplication::font());
    knob.setFont(other);
    const QSize second = knob.minimumSizeHint();
    IndustrialKnob reference;
    configure(reference);
    reference.setFont(other);
    internal::IndustrialKnobTestAccess::setMinimumWithoutTable(reference, true);
    const QSize expected = reference.minimumSizeHint();
    QVERIFY2(expected != first, qPrintable(QStringLiteral("the font change leaves the minimum at %1: it checks nothing")
                                               .arg(first.width())));
    QCOMPARE(second, expected);
}

void tst_IndustrialKnob::dialFollowsTheDrawnLabels_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("maximum");
    QTest::addRow("160x180, 0-10000000") << 160 << 180 << 10000000.0;
    QTest::addRow("220x240, 0-10000000") << 220 << 240 << 10000000.0;
    QTest::addRow("300x330, 0-10000000") << 300 << 330 << 10000000.0;
    QTest::addRow("160x180, 0-1000000") << 160 << 180 << 1000000.0;
    QTest::addRow("300x330, 0-100000") << 300 << 330 << 100000.0;
}

// #170: the dial is drawn at the largest radius at which the labels selected at that radius, and not the ones left
// out, stay inside. Taken from the pixels of the scale: the ticks end 0.8 px past the scale radius (round cap of the
// 1.6 px pen), and 1.5 px covers the rounding to a pixel and of the centre measured, as in the clicks above
void tst_IndustrialKnob::dialFollowsTheDrawnLabels()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, maximum);

    IndustrialKnob knob;
    knob.setRange(0.0, maximum);
    knob.resize(width, height);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));

    const internal::KnobScaleGeometry expected = drawnGeometryOf(knob, width, height);
    QVERIFY(expected.radius < internal::knobScaleGeometry(width, height).radius);
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY2(std::abs(dial.radius - (expected.scaleRadius + 0.8)) <= 1.5,
             qPrintable(QStringLiteral("scale drawn at %1, expected %2 + 0.8").arg(dial.radius).arg(expected.scaleRadius)));
    // The grip is drawn on every paint, not with the static scale: 0.65 of the radius, plus half of its 1.2 px pen
    QVERIFY2(std::abs(dial.grip - (expected.radius * 0.65 + 0.6)) <= 1.0,
             qPrintable(QStringLiteral("grip drawn at %1, expected %2 + 0.6").arg(dial.grip).arg(expected.radius * 0.65)));
}

// #170: the labels and the radius depend on the precision, so the static scale has to follow it. On a knob that is
// shown: grab() sends a hidden widget a resize event, which drops the cache anyway
void tst_IndustrialKnob::precisionChangesTheLabelsDrawn()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 1.0);
    knob.setValueDisplayVisible(false);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const QImage before = FontTest::render(knob);
    knob.setPrecision(2);

    IndustrialKnob fresh;
    fresh.setRange(0.0, 1.0);
    fresh.setValueDisplayVisible(false);
    fresh.setPrecision(2);
    fresh.resize(220, 240);
    fresh.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fresh));
    QVERIFY(FontTest::render(fresh) != before);
    QCOMPARE(FontTest::render(knob), FontTest::render(fresh));
}

// #170: wherever a label falls on the dial, its pixels stay inside the ink measured once for its text and font size,
// on a pixmap as the static scale. Every pixel with any ink counts, with the rectangle centred on a device pixel
void tst_IndustrialKnob::labelInkStaysInsideItsMeasure()
{
    IndustrialKnob knob;
    const qreal dpr = knob.devicePixelRatioF();
    for (int pixelSize : {7, 8, 9, 10, 11}) {
        QFont font = knob.font();
        font.setPixelSize(pixelSize);
        font.setBold(true);
        const QFontMetricsF fm(font);
        for (const QString &text : {QStringLiteral("0"), QStringLiteral("12"), QStringLiteral("20"), QStringLiteral("100"), QStringLiteral("8000"),
                                    QStringLiteral("-1000"), QStringLiteral("100000")}) {
            const QRectF rect = fm.boundingRect(text);
            const internal::TextInkBox ink = internal::measureCenteredTextInk(text, font, rect.width(), rect.height(), dpr);
            for (int i = 0; i < 60; ++i) {
                const double x = std::round((60.0 + i * 1.37) * dpr) / dpr;
                const double y = std::round((40.0 + i * 0.83) * dpr) / dpr;
                QPixmap pixmap(QSize(220, 160) * dpr);
                pixmap.setDevicePixelRatio(dpr);
                pixmap.fill(Qt::transparent);
                {
                    QPainter painter(&pixmap);
                    painter.setRenderHint(QPainter::Antialiasing, true);
                    painter.setRenderHint(QPainter::TextAntialiasing, true);
                    painter.setFont(font);
                    painter.setPen(Qt::white);
                    painter.drawText(QRectF(x - rect.width() * 0.5, y - rect.height() * 0.5, rect.width(), rect.height()),
                                     Qt::AlignCenter | Qt::TextDontClip, text);
                }
                const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QRect drawn;
                for (int py = 0; py < image.height(); ++py) {
                    const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(py));
                    for (int px = 0; px < image.width(); ++px) {
                        if (qAlpha(line[px]) != 0) {
                            drawn |= QRect(px, py, 1, 1);
                        }
                    }
                }
                const double slack = 1e-6;
                QVERIFY2(drawn.left() / dpr - x >= ink.left - slack && (drawn.right() + 1) / dpr - x <= ink.right + slack
                             && drawn.top() / dpr - y >= ink.top - slack && (drawn.bottom() + 1) / dpr - y <= ink.bottom + slack,
                         qPrintable(QStringLiteral("%1 at %2 px, centre %3, %4").arg(text).arg(pixelSize).arg(x).arg(y)));
            }
        }
    }
}

void tst_IndustrialKnob::dprChangeOfAVisibleKnob_data()
{
    QTest::addColumn<double>("target");
    for (double target : {1.25, 1.5, 2.0}) {
        QTest::addRow("to DPR %g", target) << target;
    }
}

// #170: a window moved to a monitor of another DPR is repainted. From that repaint the scale and the ink follow the new
// DPR, as in a new knob, and the layout that holds the knob gets the new minimum, once, without being told by hand
void tst_IndustrialKnob::dprChangeOfAVisibleKnob()
{
    QFETCH(double, target);

    int differs = 0;
    for (double maximum : {1e7, 1e8, 1e9, 1e10, 1e11, 1e12}) {
        QWidget parent;
        auto *layout = new QVBoxLayout(&parent);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *knob = new FakeDprKnob(&parent);
        knob->setFocusPolicy(Qt::NoFocus); // The last window shown takes the focus, and its knob draws the focus ring
        knob->setRange(0.0, maximum);
        layout->addWidget(knob);
        parent.resize(200, 220);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));

        const QSize before = knob->minimumSizeHint();
        QCOMPARE(layout->minimumSize().width(), before.width());
        knob->repaint();
        QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);

        LayoutRequests requests;
        parent.installEventFilter(&requests);
        knob->fakeDpr = target;
        QCOMPARE(knob->devicePixelRatioF(), target);
        knob->repaint();
        QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
        QVERIFY2(requests.count > 0, qPrintable(QStringLiteral("0-%1: no layout request").arg(maximum)));

        // The same parent and size, at the new DPR from the start
        QWidget freshParent;
        auto *freshLayout = new QVBoxLayout(&freshParent);
        freshLayout->setContentsMargins(0, 0, 0, 0);
        auto *fresh = new FakeDprKnob(&freshParent);
        fresh->fakeDpr = target;
        fresh->setFocusPolicy(Qt::NoFocus);
        fresh->setRange(0.0, maximum);
        freshLayout->addWidget(fresh);
        freshParent.resize(parent.size());
        freshParent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&freshParent));
        QCOMPARE(fresh->size(), knob->size());

        QCOMPARE(knob->minimumSizeHint(), fresh->minimumSizeHint());
        QCOMPARE(layout->minimumSize().width(), fresh->minimumSizeHint().width());
        differs += fresh->minimumSizeHint() != before ? 1 : 0;
        const QImage drawn = FontTest::render(*knob);
        const QImage expected = FontTest::render(*fresh);
        QVERIFY2(FontTest::differingPixelCount(drawn, expected) == 0,
                 qPrintable(QStringLiteral("0-%1: %2 pixels differ, %3 x %4 and %5 x %6")
                                .arg(maximum).arg(FontTest::differingPixelCount(drawn, expected))
                                .arg(drawn.width()).arg(drawn.height()).arg(expected.width()).arg(expected.height())));

        // Once: the next repaints at this DPR do not disturb the layout
        requests.count = 0;
        knob->repaint();
        knob->repaint();
        QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
        QCOMPARE(requests.count, 0);
        parent.removeEventFilter(&requests);
    }
    QVERIFY2(differs > 0, "the minimum is the same at both DPRs for every range");

    // Asked for before the repaint, or on a knob that is not shown: the minimum of the DPR of that moment
    for (double maximum : {1e7, 1e8, 1e9, 1e10, 1e11, 1e12}) {
        FakeDprKnob knob;
        knob.setRange(0.0, maximum);
        QVERIFY(!knob.minimumSizeHint().isEmpty());
        knob.fakeDpr = target;
        FakeDprKnob fresh;
        fresh.fakeDpr = target;
        fresh.setRange(0.0, maximum);
        QCOMPARE(knob.minimumSizeHint(), fresh.minimumSizeHint());
    }
}

// #177: the ink of the labels is measured on a raster when the scale is built, and kept. A repaint, a colour or a value
// measures nothing; a new font or a new DPR measures the labels again
void tst_IndustrialKnob::repaintDoesNotMeasureTheLabelsAgain()
{
    using Access = internal::IndustrialKnobTestAccess;
    FakeDprKnob knob;
    knob.setFocusPolicy(Qt::NoFocus);
    knob.setRange(0.0, 8000.0);
    knob.resize(160, 180);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    knob.repaint();
    const quint64 shown = Access::labelInkMeasureCount(knob);
    QVERIFY2(shown > 0, "no label measured on the first paint");

    for (int i = 0; i < 100; ++i) {
        knob.repaint();
    }
    QCOMPARE(Access::labelInkMeasureCount(knob), shown);

    const QColor colours[] = {Qt::red, Qt::green, Qt::blue};
    for (const QColor &colour : colours) {
        knob.setKnobColor(colour);
        knob.setPointerColor(colour);
        knob.setScaleColor(colour);
        knob.setTrackColor(colour);
        knob.setTextColor(colour);
        knob.repaint();
    }
    for (int i = 0; i <= 80; ++i) {
        knob.setValue(i * 100.0);
        knob.repaint();
    }
    QCOMPARE(Access::labelInkMeasureCount(knob), shown);

    knob.setFont(FontTest::otherFont(knob.font()));
    knob.repaint();
    const quint64 afterFont = Access::labelInkMeasureCount(knob);
    QVERIFY2(afterFont > shown, "a new font did not measure the labels");
    knob.repaint();
    QCOMPARE(Access::labelInkMeasureCount(knob), afterFont);

    knob.fakeDpr = knob.devicePixelRatioF() == 2.0 ? 1.5 : 2.0;
    knob.repaint();
    const quint64 afterDpr = Access::labelInkMeasureCount(knob);
    QVERIFY2(afterDpr > afterFont, "a new DPR did not measure the labels");
    knob.repaint();
    QCOMPARE(Access::labelInkMeasureCount(knob), afterDpr);
}

QTEST_MAIN(tst_IndustrialKnob)
void tst_IndustrialKnob::scaleLabelsKeepMinimumGap_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");
    QTest::addColumn<int>("discreteSteps"); // 0: continuous mode

    // The configurations measured for #60, default 270-degree arc
    struct Range { const char *name; double min; double max; };
    const Range ranges[] = {{"0-100000", 0.0, 100000.0}, {"0-10000000", 0.0, 10000000.0}, {"0-100", 0.0, 100.0},
                            {"-1000-1000", -1000.0, 1000.0}, {"0-8000", 0.0, 8000.0}};
    const QSize sizes[] = {{80, 90}, {100, 120}, {160, 180}, {220, 240}, {300, 340}};
    for (const QSize &size : sizes) {
        for (const Range &r : ranges) {
            QTest::addRow("%dx%d, %s", size.width(), size.height(), r.name)
                << size.width() << size.height() << r.min << r.max << 0;
        }
        for (int steps : {5, 11, 21, 41}) {
            QTest::addRow("%dx%d, %d discrete steps", size.width(), size.height(), steps)
                << size.width() << size.height() << 0.0 << 100.0 << steps;
        }
    }
}

// #60(b): the labels the selection of #50 chooses, and no others, are drawn, and each keeps the
// minimum gap. Label pixels are those that change with textColor while the value display is
// hidden. They are grouped into clusters as in tst_RadialGauge::scaleLabelsKeepMinimumGap(): two
// labels closer than the minimum merge into one cluster, larger than any label drawn alone.
// Without fonts every glyph is a box far from its neighbours, hence a cluster of its own: all the
// ink must then lie inside the selected labels, and every selected label must have ink.
void tst_IndustrialKnob::scaleLabelsKeepMinimumGap()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, minimum);
    QFETCH(double, maximum);
    QFETCH(int, discreteSteps);

    IndustrialKnob knob;
    knob.setRange(minimum, maximum);
    if (discreteSteps > 0) {
        knob.setMode(IndustrialKnob::KnobMode::Discrete);
        knob.setDiscreteSteps(discreteSteps);
    }
    knob.setValueDisplayVisible(false);
    knob.setValue(minimum);
    knob.resize(width, height);
    knob.setTextColor(Qt::white);
    const QImage white = FontTest::render(knob);
    knob.setTextColor(Qt::red);
    const QImage red = FontTest::render(knob);
    const qreal dpr = knob.grab().devicePixelRatio();

    const ScaleLabels labels = scaleLabelsOf(knob);
    const int last = static_cast<int>(labels.boxes.size()) - 1;
    // #170: both ends of the scale from minimumSizeHint() up
    if (width >= knob.minimumSizeHint().width() && height >= knob.minimumSizeHint().height()) {
        QVERIFY2(std::find(labels.selection.drawn.begin(), labels.selection.drawn.end(), 0) != labels.selection.drawn.end(),
                 "the first label is not selected");
        QVERIFY2(std::find(labels.selection.drawn.begin(), labels.selection.drawn.end(), last) != labels.selection.drawn.end(),
                 "the last label is not selected");
    }

    QVector<QPoint> ink;
    for (int y = 0; y < white.height(); ++y) {
        for (int x = 0; x < white.width(); ++x) {
            if (white.pixel(x, y) != red.pixel(x, y)) {
                ink.append(QPoint(x, y));
            }
        }
    }
    const double threshold = (labels.minGap - 1.0) * dpr;
    const QVector<QRect> clusters = LabelInk::inkClusters(ink, threshold);

    QSize largest;
    for (int i : labels.selection.drawn) {
        largest = largest.expandedTo(LabelInk::inkBoxAlone(labels.texts[static_cast<size_t>(i)], labels.font, dpr).size());
    }
    const int tolerance = static_cast<int>(std::ceil(dpr));

    // 1 px logical allowed for glyphs that exceed the font bounding rectangle the layout uses
    auto holds = [&](int label, const QRect &c) {
        const internal::LayoutRect &b = labels.boxes[static_cast<size_t>(label)];
        const QPointF centre = QRectF(c).center() / dpr;
        return centre.x() >= b.x - 1.0 && centre.x() <= b.right() + 1.0 && centre.y() >= b.y - 1.0 && centre.y() <= b.bottom() + 1.0;
    };
    std::set<int> drawn;
    for (const QRect &c : clusters) {
        QVERIFY2(c.width() <= largest.width() + tolerance && c.height() <= largest.height() + tolerance,
                 qPrintable(QStringLiteral("label ink %1x%2 at (%3, %4) is larger than the largest label alone %5x%6: "
                                           "labels closer than %7 device px (minimum gap %8 px, DPR %9)")
                                .arg(c.width()).arg(c.height()).arg(c.x()).arg(c.y())
                                .arg(largest.width()).arg(largest.height()).arg(threshold).arg(labels.minGap).arg(dpr)));
        bool inSelectedLabel = false;
        for (int i : labels.selection.drawn) {
            if (holds(i, c)) {
                drawn.insert(i);
                inSelectedLabel = true;
            }
        }
        QVERIFY2(inSelectedLabel, qPrintable(QStringLiteral("ink %1x%2 at (%3, %4) outside every selected label (step %5)")
                                                 .arg(c.width()).arg(c.height()).arg(c.x()).arg(c.y()).arg(labels.selection.step)));
    }
    for (int i : labels.selection.drawn) {
        QVERIFY2(drawn.count(i) == 1, qPrintable(QStringLiteral("label %1 (\"%2\") is selected but not drawn")
                                                     .arg(i).arg(labels.texts[static_cast<size_t>(i)])));
    }
}

// #182: in read-only every key, a drag and the wheel change nothing and emit nothing; keys and presses reach the
// parent, the wheel is ignored. The knob keeps the focus and setValue() still works. Off again, the same input acts
void tst_IndustrialKnob::readOnlyIgnoresTheOperatorInput()
{
    RecordingParent parent;
    parent.resize(300, 300);
    auto *knob = new IndustrialKnob(&parent);
    knob->setRange(0.0, 100.0);
    knob->setStep(1.0);
    knob->setWheelEnabled(true);
    knob->resize(220, 240);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    const DrawnDial dial = drawnDialOf(*knob);
    QVERIFY(dial.radius > 20.0);
    QVERIFY(focusOn(*knob));

    knob->setValue(50.0);
    QSignalSpy readOnly(knob, &IndustrialKnob::readOnlyChanged);
    QVERIFY(!knob->isReadOnly());
    knob->setReadOnly(true);
    knob->setReadOnly(true);
    QVERIFY(knob->isReadOnly());
    QCOMPARE(readOnly.count(), 1);
    QCOMPARE(readOnly.at(0).at(0).toBool(), true);

    QSignalSpy changed(knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(knob, &IndustrialKnob::valueCommitted);
    for (const Qt::Key key : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_PageUp, Qt::Key_PageDown,
                              Qt::Key_Home, Qt::Key_End}) {
        const int before = parent.keyPresses;
        QTest::keyClick(knob, key);
        QVERIFY2(parent.keyPresses == before + 1,
                 qPrintable(QStringLiteral("key %1 did not reach the parent").arg(QKeySequence(key).toString())));
    }
    QTest::mousePress(knob, Qt::LeftButton, Qt::NoModifier, dialPoint(dial, 0.0).toPoint());
    QCOMPARE(parent.mousePresses, 1);
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    sendMouse(*knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    QVERIFY(!sendWheel(*knob, 120));
    QVERIFY(!sendWheel(*knob, -240));
    QCOMPARE(knob->value(), 50.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
    QVERIFY(knob->hasFocus());

    knob->setValue(60.0);
    QCOMPARE(knob->value(), 60.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(committed.count(), 0);

    // Control: off again, the same input reaches the knob and acts
    knob->setReadOnly(false);
    QCOMPARE(readOnly.count(), 2);
    const int keysBefore = parent.keyPresses;
    QTest::keyClick(knob, Qt::Key_Up);
    QCOMPARE(knob->value(), 61.0);
    QCOMPARE(parent.keyPresses, keysBefore);
    QVERIFY(sendWheel(*knob, 120));
    QCOMPARE(knob->value(), 62.0);
    knob->setValue(50.0);
    QTest::mousePress(knob, Qt::LeftButton, Qt::NoModifier, dialPoint(dial, 0.0).toPoint());
    QCOMPARE(parent.mousePresses, 1);
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    sendMouse(*knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    QVERIFY2(std::abs(knob->value() - 83.3) < 1.0, qPrintable(QString::number(knob->value())));
}

void tst_IndustrialKnob::readOnlyEndsTheRunningAction_data()
{
    QTest::addColumn<bool>("drag");
    QTest::newRow("drag") << true;
    QTest::newRow("held key") << false;
}

// #182: read-only turned on during a drag or a held key ends it there and commits once; what follows does nothing
void tst_IndustrialKnob::readOnlyEndsTheRunningAction()
{
    QFETCH(bool, drag);

    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    knob.setValue(50.0);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);

    if (drag) {
        sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    } else {
        sendKey(knob, QEvent::KeyPress, Qt::Key_PageUp, false);
        sendKey(knob, QEvent::KeyPress, Qt::Key_PageUp, true);
    }
    const double moved = knob.value();
    QVERIFY(moved > 50.0);
    QCOMPARE(committed.count(), 0);

    knob.setReadOnly(true);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toDouble(), moved);

    if (drag) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0));
        sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    } else {
        sendKey(knob, QEvent::KeyPress, Qt::Key_PageUp, true);
        sendKey(knob, QEvent::KeyRelease, Qt::Key_PageUp, false);
    }
    QCOMPARE(knob.value(), moved);
    QCOMPARE(committed.count(), 1);
}

void tst_IndustrialKnob::readOnlyDrawsThePadlockInsideThePointer_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("value");
    struct Size { int w; int h; };
    for (const Size size : {Size{220, 240}, Size{160, 180}}) {
        // 63.15: the pointer towards an upper corner of the padlock (35.5 degrees)
        for (const double value : {0.0, 33.0, 50.0, 63.15, 100.0}) {
            QTest::addRow("%dx%d, value %g", size.w, size.h, value) << size.w << size.h << value;
        }
    }
}

// #182: read-only draws a padlock: a shape in textColor() that changes the lightness of the cap (not a tint), inside
// the inner end of the pointer as it is drawn, so no pixel of the pointer changes, at every angle of the pointer. The
// knob is not greyed out: nothing else changes, and turning read-only off gives the same pixels as before
void tst_IndustrialKnob::readOnlyDrawsThePadlockInsideThePointer()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, value);

    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValueDisplayVisible(false);
    knob.resize(width, height);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const QColor knobColor = knob.knobColor();
    const QColor scaleColor = knob.scaleColor();
    const DrawnDial dial = drawnDialOf(knob);
    knob.setKnobColor(knobColor);
    knob.setScaleColor(scaleColor);
    QVERIFY(dial.radius > 20.0);
    const qreal dpr = knob.devicePixelRatioF();

    knob.setValue(value);
    const QVector<QPoint> pointer = pointerPixels(knob);
    QVERIFY(!pointer.isEmpty());
    double pointerInner = std::numeric_limits<double>::max();
    for (const QPoint &p : pointer) {
        pointerInner = std::min(pointerInner, distanceOf(p, dial.center, dpr));
    }

    const QImage normal = renderKnob(knob);
    knob.setReadOnly(true);
    const QImage locked = renderKnob(knob);
    const QVector<QPoint> changed = differingPixels(normal, locked);

    int shape = 0;
    int ink = 0;
    const QRectF rect = internal::IndustrialKnobTestAccess::padlockRect(knob);
    const QRectF deviceRect(rect.x() * dpr - 1.0, rect.y() * dpr - 1.0, rect.width() * dpr + 2.0, rect.height() * dpr + 2.0);
    for (const QPoint &p : changed) {
        const double distance = distanceOf(p, dial.center, dpr);
        QVERIFY2(distance < pointerInner,
                 qPrintable(QStringLiteral("read-only changes pixel (%1, %2) at %3 px from the centre; the pointer starts at %4 px")
                                .arg(p.x()).arg(p.y()).arg(distance).arg(pointerInner)));
        QVERIFY2(deviceRect.contains(QPointF(p) + QPointF(0.5, 0.5)),
                 qPrintable(QStringLiteral("read-only changes pixel (%1, %2), outside the padlock").arg(p.x()).arg(p.y())));
        shape += std::abs(lightness(locked.pixel(p)) - lightness(normal.pixel(p))) >= 0.25 ? 1 : 0;
        ink += colourDistance(locked.pixel(p), knob.textColor()) <= 24 ? 1 : 0;
    }
    for (const QPoint &p : pointer) {
        QVERIFY2(locked.pixel(p) == normal.pixel(p),
                 qPrintable(QStringLiteral("the padlock covers the pointer at (%1, %2)").arg(p.x()).arg(p.y())));
    }
    const int area = static_cast<int>(std::round(dpr * dpr));
    QVERIFY2(shape >= 25 * area, qPrintable(QStringLiteral("%1 pixels change lightness by 0.25 or more").arg(shape)));
    QVERIFY2(ink >= 12 * area, qPrintable(QStringLiteral("%1 pixels in textColor").arg(ink)));

    knob.setReadOnly(false);
    QCOMPARE(renderKnob(knob), normal);
}

// #182: the limits read are those set, within the range; without limits, the range. valueLimitsChanged() follows
// what is read, setRange() included. Invalid limits are ignored, and setting limits never changes the value
void tst_IndustrialKnob::valueLimitsReadWithinTheRange()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(90.0);
    QCOMPARE(knob.valueLimitLow(), 0.0);
    QCOMPARE(knob.valueLimitHigh(), 100.0);
    QSignalSpy limits(&knob, &IndustrialKnob::valueLimitsChanged);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    const auto lastLimits = [&]() {
        return std::make_pair(limits.last().at(0).toDouble(), limits.last().at(1).toDouble());
    };

    knob.setValueLimits(20.0, 80.0);
    QCOMPARE(knob.valueLimitLow(), 20.0);
    QCOMPARE(knob.valueLimitHigh(), 80.0);
    QCOMPARE(limits.count(), 1);
    QCOMPARE(lastLimits(), std::make_pair(20.0, 80.0));
    QCOMPARE(knob.value(), 90.0);
    QCOMPARE(changed.count(), 0);
    knob.setValueLimits(20.0, 80.0);
    QCOMPARE(limits.count(), 1);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const auto &invalid : {std::make_pair(nan, 80.0), std::make_pair(20.0, inf), std::make_pair(-inf, 80.0),
                                std::make_pair(80.0, 20.0), std::make_pair(50.0, 50.0)}) {
        knob.setValueLimits(invalid.first, invalid.second);
        QCOMPARE(knob.valueLimitLow(), 20.0);
        QCOMPARE(knob.valueLimitHigh(), 80.0);
    }
    QCOMPARE(limits.count(), 1);

    knob.setRange(30.0, 70.0);
    QCOMPARE(knob.valueLimitLow(), 30.0);
    QCOMPARE(knob.valueLimitHigh(), 70.0);
    QCOMPARE(limits.count(), 2);
    QCOMPARE(lastLimits(), std::make_pair(30.0, 70.0));
    knob.setRange(0.0, 100.0); // The limits as set apply again in full
    QCOMPARE(knob.valueLimitLow(), 20.0);
    QCOMPARE(knob.valueLimitHigh(), 80.0);
    QCOMPARE(limits.count(), 3);
    knob.setRange(10.0, 100.0); // Moves the range, not the limits read
    QCOMPARE(limits.count(), 3);

    knob.setValueLimits(-50.0, 150.0);
    QCOMPARE(knob.valueLimitLow(), 10.0);
    QCOMPARE(knob.valueLimitHigh(), 100.0);
    QCOMPARE(limits.count(), 4);
    knob.setRange(0.0, 200.0);
    QCOMPARE(knob.valueLimitLow(), 0.0);
    QCOMPARE(knob.valueLimitHigh(), 150.0);
    QCOMPARE(limits.count(), 5);

    knob.clearValueLimits();
    QCOMPARE(knob.valueLimitLow(), 0.0);
    QCOMPARE(knob.valueLimitHigh(), 200.0);
    QCOMPARE(limits.count(), 6);
    knob.clearValueLimits();
    QCOMPARE(limits.count(), 6);
    knob.setRange(0.0, 100.0); // Without limits they are the range, and follow it
    QCOMPARE(knob.valueLimitHigh(), 100.0);
    QCOMPARE(limits.count(), 7);
    QCOMPARE(lastLimits(), std::make_pair(0.0, 100.0));
}

// #182: setValue() is bound by the range only: inside the scale and outside the limits, the value is kept as it is
// and drawn where it is, not at the limit
void tst_IndustrialKnob::setValueOutsideTheLimitsIsKept()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValueLimits(20.0, 80.0);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    knob.setValue(90.0);
    QCOMPARE(knob.value(), 90.0);
    knob.setValue(5.0);
    QCOMPARE(knob.value(), 5.0);
    knob.setValue(150.0);
    QCOMPARE(knob.value(), 100.0);
    knob.setValue(-1.0);
    QCOMPARE(knob.value(), 0.0);
    QCOMPARE(committed.count(), 0);

    knob.setValueDisplayVisible(false);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const QColor knobColor = knob.knobColor();
    const QColor scaleColor = knob.scaleColor();
    const DrawnDial dial = drawnDialOf(knob);
    knob.setKnobColor(knobColor);
    knob.setScaleColor(scaleColor);
    const qreal dpr = knob.devicePixelRatioF();
    for (const double value : {90.0, 5.0}) {
        knob.setValue(value);
        const QVector<QPoint> pointer = pointerPixels(knob);
        QVERIFY(!pointer.isEmpty());
        // The tip of the pointer: its pixel farthest from the centre
        QPoint tip = pointer.first();
        for (const QPoint &p : pointer) {
            if (distanceOf(p, dial.center, dpr) > distanceOf(tip, dial.center, dpr)) {
                tip = p;
            }
        }
        const double angle = angleOf(tip, dial.center, dpr);
        QVERIFY2(std::abs(angle - angleOfValue(knob, value)) < 3.0,
                 qPrintable(QStringLiteral("value %1: pointer at %2 degrees, expected %3")
                                .arg(value).arg(angle).arg(angleOfValue(knob, value))));
    }
}

// #182: keys and a drag never take the value outside the limits; Home/End go to the limits; a step that would
// cross a limit stops on it. Past a limit a drag holds the value there until the pointer comes back
void tst_IndustrialKnob::operatorInputStaysInsideTheLimits()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(1.0);
    knob.setValueLimits(20.0, 80.0);
    struct Case { double from; Qt::Key key; double to; };
    for (const Case c : {Case{79.5, Qt::Key_Up, 80.0}, Case{80.0, Qt::Key_Up, 80.0}, Case{78.0, Qt::Key_PageUp, 80.0},
                         Case{50.0, Qt::Key_End, 80.0}, Case{20.5, Qt::Key_Down, 20.0}, Case{22.0, Qt::Key_PageDown, 20.0},
                         Case{50.0, Qt::Key_Home, 20.0}, Case{50.0, Qt::Key_Up, 51.0}, Case{50.0, Qt::Key_Left, 49.0}}) {
        knob.setValue(c.from);
        QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
        QTest::keyClick(&knob, c.key);
        const QString what = QStringLiteral("%1 from %2").arg(QKeySequence(c.key).toString()).arg(c.from);
        QVERIFY2(knob.value() == c.to, qPrintable(what + QStringLiteral(": %1, expected %2").arg(knob.value()).arg(c.to)));
        QVERIFY2(committed.count() == (c.to != c.from ? 1 : 0), qPrintable(what + QStringLiteral(": commits")));
    }

    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    // From 50 (0 degrees) up: 66.7 at 45 degrees, the limit from 64.8 degrees on, back to 66.7
    knob.setValue(50.0);
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    QVERIFY2(std::abs(knob.value() - 66.7) < 1.0, qPrintable(QString::number(knob.value())));
    for (const double angle : {90.0, 120.0, 90.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
        QVERIFY2(knob.value() == 80.0, qPrintable(QStringLiteral("angle %1: %2").arg(angle).arg(knob.value())));
    }
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    QVERIFY2(std::abs(knob.value() - 66.7) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 45.0));
    QCOMPARE(committed.count(), 1);
    // And down to the low limit
    knob.setValue(50.0);
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    for (const double angle : {-45.0, -90.0, -120.0}) {
        sendMouse(knob, QEvent::MouseMove, dialPoint(dial, angle));
    }
    QCOMPARE(knob.value(), 20.0);
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, -120.0));
    QCOMPARE(committed.count(), 2);
    QCOMPARE(committed.last().at(0).toDouble(), 20.0);
}

// #182: from a value outside the limits (set with setValue()), input towards them is applied, input further out is
// ignored and commits nothing
void tst_IndustrialKnob::fromOutsideTheLimitsOnlyInwardInputApplies()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setStep(1.0);
    knob.setValueLimits(20.0, 80.0);
    struct Case { double from; Qt::Key key; double to; };
    for (const Case c : {Case{90.0, Qt::Key_Up, 90.0}, Case{90.0, Qt::Key_PageUp, 90.0}, Case{90.0, Qt::Key_End, 90.0},
                         Case{90.0, Qt::Key_Down, 89.0}, Case{90.0, Qt::Key_PageDown, 85.0}, Case{90.0, Qt::Key_Home, 20.0},
                         Case{81.0, Qt::Key_PageDown, 76.0},
                         Case{10.0, Qt::Key_Down, 10.0}, Case{10.0, Qt::Key_PageDown, 10.0}, Case{10.0, Qt::Key_Home, 10.0},
                         Case{10.0, Qt::Key_Up, 11.0}, Case{10.0, Qt::Key_End, 80.0}}) {
        knob.setValue(c.from);
        QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
        QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
        QTest::keyClick(&knob, c.key);
        const QString what = QStringLiteral("%1 from %2").arg(QKeySequence(c.key).toString()).arg(c.from);
        QVERIFY2(knob.value() == c.to, qPrintable(what + QStringLiteral(": %1, expected %2").arg(knob.value()).arg(c.to)));
        QVERIFY2(committed.count() == (c.to != c.from ? 1 : 0), qPrintable(what + QStringLiteral(": commits")));
        QVERIFY2(changed.count() == (c.to != c.from ? 1 : 0), qPrintable(what + QStringLiteral(": changes")));
    }

    knob.setStep(0.01);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    knob.setValue(90.0); // At 108 degrees
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 108.0));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 120.0)); // Further out: ignored
    QCOMPARE(knob.value(), 90.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0)); // Back past where it was: inward, applied
    QVERIFY2(std::abs(knob.value() - 83.3) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 0.0));
    QVERIFY2(std::abs(knob.value() - 50.0) < 1.0, qPrintable(QString::number(knob.value())));
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, 0.0));
    QCOMPARE(committed.count(), 1);
}

// #182: in Discrete mode the operator reaches only the steps inside the limits; setValue() keeps any step
void tst_IndustrialKnob::discreteModeReachesOnlyTheStepsInsideTheLimits()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5); // 0, 25, 50, 75, 100
    knob.setStep(25.0);
    knob.setValueLimits(10.0, 60.0); // Steps inside: 25 and 50
    struct Case { double from; Qt::Key key; double to; };
    for (const Case c : {Case{50.0, Qt::Key_End, 50.0}, Case{50.0, Qt::Key_Up, 50.0}, Case{50.0, Qt::Key_Home, 25.0},
                         Case{25.0, Qt::Key_Down, 25.0}, Case{100.0, Qt::Key_Home, 25.0}, Case{100.0, Qt::Key_Down, 75.0},
                         Case{100.0, Qt::Key_Up, 100.0}, Case{0.0, Qt::Key_Up, 25.0}, Case{0.0, Qt::Key_End, 50.0}}) {
        knob.setValue(c.from);
        QCOMPARE(knob.value(), c.from);
        QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);
        QTest::keyClick(&knob, c.key);
        const QString what = QStringLiteral("%1 from %2").arg(QKeySequence(c.key).toString()).arg(c.from);
        QVERIFY2(knob.value() == c.to, qPrintable(what + QStringLiteral(": %1, expected %2").arg(knob.value()).arg(c.to)));
        QVERIFY2(committed.count() == (c.to != c.from ? 1 : 0), qPrintable(what + QStringLiteral(": commits")));
    }

    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const DrawnDial dial = drawnDialOf(knob);
    QVERIFY(dial.radius > 20.0);
    knob.setValue(50.0);
    sendMouse(knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 90.0)); // 83.3: the step inside below it is 50
    QCOMPARE(knob.value(), 50.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, 0.0)); // Not a half turn in one move (#175: ambiguous)
    QCOMPARE(knob.value(), 50.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, -90.0)); // 16.7: nearest step 25
    QCOMPARE(knob.value(), 25.0);
    sendMouse(knob, QEvent::MouseMove, dialPoint(dial, -120.0)); // 5.6: the nearest step inside is 25
    QCOMPARE(knob.value(), 25.0);
    sendMouse(knob, QEvent::MouseButtonRelease, dialPoint(dial, -120.0));
}

void tst_IndustrialKnob::limitsAreHatchedWithStopMarks_data()
{
    QTest::addColumn<bool>("discrete");
    QTest::addColumn<double>("low");
    QTest::addColumn<double>("high");
    QTest::newRow("continuous, 20-80") << false << 20.0 << 80.0;
    QTest::newRow("continuous, high only (0-70)") << false << 0.0 << 70.0;
    QTest::newRow("continuous, low only (35-100)") << false << 35.0 << 100.0;
    QTest::newRow("discrete, 10-60") << true << 10.0 << 60.0;
}

// #182: the parts of the scale outside the limits are hatched (lines, not a fill) in the band of the ticks, and each
// limit inside the range has a radial stop mark that reaches further in than the ticks. Nothing is drawn inside the
// limits, and limits equal to the range, or cleared, draw nothing
void tst_IndustrialKnob::limitsAreHatchedWithStopMarks()
{
    QFETCH(bool, discrete);
    QFETCH(double, low);
    QFETCH(double, high);

    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    if (discrete) {
        knob.setMode(IndustrialKnob::KnobMode::Discrete);
        knob.setDiscreteSteps(5);
    }
    knob.setValue(50.0);
    knob.setTrackVisible(false);
    knob.setValueDisplayVisible(false);
    knob.resize(220, 240);
    knob.show();
    QVERIFY(QTest::qWaitForWindowExposed(&knob));
    const qreal dpr = knob.devicePixelRatioF();
    const internal::KnobScaleGeometry g = drawnGeometryOf(knob, knob.width(), knob.height());
    const QPointF center(g.centerX, g.centerY);
    const double tickLen = g.radius * 0.12; // As renderStaticScale()

    const QImage plain = renderKnob(knob);
    knob.setValueLimits(0.0, 100.0);
    QCOMPARE(renderKnob(knob), plain);
    knob.setValueLimits(low, high);
    const QImage limited = renderKnob(knob);

    const double lowAngle = angleOfValue(knob, low);
    const double highAngle = angleOfValue(knob, high);
    struct Count { int hatch{0}; int band{0}; int stop{0}; };
    Count below;
    Count above;
    for (int y = 0; y < plain.height(); ++y) {
        for (int x = 0; x < plain.width(); ++x) {
            const QPoint p(x, y);
            const double r = distanceOf(p, center, dpr);
            double a = angleOf(p, center, dpr);
            if (a - knob.startAngle() < 0.0) {
                a += 360.0;
            }
            const bool differs = plain.pixel(p) != limited.pixel(p);
            const double margin = 2.5 / r * 180.0 / M_PI; // 2.5 px across, in degrees
            const bool inBand = r >= g.scaleRadius - tickLen + 1.0 && r <= g.scaleRadius - 1.0;
            const bool onScale = a >= knob.startAngle() && a <= knob.startAngle() + knob.spanAngle();
            if (inBand && onScale && a < lowAngle - margin) {
                ++below.band;
                below.hatch += differs ? 1 : 0;
            }
            if (inBand && onScale && a > highAngle + margin) {
                ++above.band;
                above.hatch += differs ? 1 : 0;
            }
            if (!differs) {
                continue;
            }
            QVERIFY2(r >= g.scaleRadius - 1.5 * tickLen - 1.5 && r <= g.scaleRadius + 1.5,
                     qPrintable(QStringLiteral("pixel (%1, %2) at radius %3, outside the band %4-%5")
                                    .arg(x).arg(y).arg(r).arg(g.scaleRadius - 1.5 * tickLen).arg(g.scaleRadius)));
            QVERIFY2(a <= lowAngle + margin || a >= highAngle - margin,
                     qPrintable(QStringLiteral("pixel (%1, %2) at %3 degrees, inside the limits %4-%5")
                                    .arg(x).arg(y).arg(a).arg(lowAngle).arg(highAngle)));
            if (r < g.scaleRadius - 1.1 * tickLen) {
                (std::abs(a - lowAngle) <= margin ? below : above).stop += std::abs(a - lowAngle) <= margin
                                                                              || std::abs(a - highAngle) <= margin ? 1 : 0;
            }
        }
    }
    const int area = static_cast<int>(std::round(dpr * dpr));
    const auto checkSide = [&](const char *side, const Count &c, bool limited) {
        if (!limited) {
            QVERIFY2(c.hatch == 0 && c.stop == 0, qPrintable(QStringLiteral("%1: drawn without a limit").arg(QLatin1String(side))));
            return;
        }
        QVERIFY2(c.band > 0, side);
        const double fraction = static_cast<double>(c.hatch) / c.band;
        QVERIFY2(c.hatch >= 30 * area && fraction >= 0.1 && fraction <= 0.8,
                 qPrintable(QStringLiteral("%1: %2 of %3 band pixels hatched (%4)").arg(QLatin1String(side)).arg(c.hatch).arg(c.band).arg(fraction)));
        QVERIFY2(c.stop >= 4 * area, qPrintable(QStringLiteral("%1: %2 pixels of the stop mark").arg(QLatin1String(side)).arg(c.stop)));
    };
    checkSide("below the low limit", below, low > knob.minimum());
    checkSide("above the high limit", above, high < knob.maximum());

    knob.clearValueLimits();
    QCOMPARE(renderKnob(knob), plain);
}

// #182: the wheel is off by default: a wheel event changes nothing and is ignored, so it goes to the parent
void tst_IndustrialKnob::wheelIsOffByDefault()
{
    QWidget window;
    auto *knob = new IndustrialKnob(&window);
    knob->setRange(0.0, 100.0);
    knob->setValue(50.0);
    knob->resize(220, 240);
    QVERIFY(!knob->isWheelEnabled());
    QVERIFY(focusOn(*knob));
    QSignalSpy changed(knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(knob, &IndustrialKnob::valueCommitted);
    QVERIFY(!sendWheel(*knob, 120));
    QVERIFY(!sendWheel(*knob, -120));
    QCOMPARE(knob->value(), 50.0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(committed.count(), 0);
}

// #182: with the wheel on, it acts only while the knob has focus; each notch (120 units, summed over smaller
// events) is one step, committed at once. What is left of a notch is dropped at focus out. The limits hold
void tst_IndustrialKnob::wheelStepsOnlyWithFocus()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *knob = new IndustrialKnob(&window);
    auto *other = new QLineEdit(&window);
    layout->addWidget(knob);
    layout->addWidget(other);
    knob->setRange(0.0, 100.0);
    knob->setStep(1.0);
    knob->setValue(50.0);
    QSignalSpy appearance(knob, &IndustrialKnob::appearanceChanged);
    knob->setWheelEnabled(true);
    knob->setWheelEnabled(true);
    QVERIFY(knob->isWheelEnabled());
    QCOMPARE(appearance.count(), 1);

    QSignalSpy changed(knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(knob, &IndustrialKnob::valueCommitted);
    QVERIFY(focusOn(*other));
    QVERIFY(!sendWheel(*knob, 120));
    QCOMPARE(knob->value(), 50.0);
    QCOMPARE(changed.count(), 0);

    QVERIFY(focusOn(*knob));
    QVERIFY(sendWheel(*knob, 120));
    QCOMPARE(knob->value(), 51.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.last().at(0).toDouble(), 51.0);
    QVERIFY(sendWheel(*knob, -240)); // Two notches: two steps, two commits
    QCOMPARE(knob->value(), 49.0);
    QCOMPARE(committed.count(), 3);
    QCOMPARE(committed.at(1).at(0).toDouble(), 50.0);
    QCOMPARE(committed.at(2).at(0).toDouble(), 49.0);

    QVERIFY(sendWheel(*knob, 60)); // Half a notch, then the other half
    QCOMPARE(knob->value(), 49.0);
    QCOMPARE(committed.count(), 3);
    QVERIFY(sendWheel(*knob, 60));
    QCOMPARE(knob->value(), 50.0);
    QCOMPARE(committed.count(), 4);
    QVERIFY(sendWheel(*knob, 60)); // Half a notch, then back
    QVERIFY(sendWheel(*knob, -60));
    QCOMPARE(knob->value(), 50.0);
    QCOMPARE(committed.count(), 4);

    QVERIFY(sendWheel(*knob, 80)); // Dropped at focus out
    QVERIFY(focusOn(*other));
    QVERIFY(focusOn(*knob));
    QVERIFY(sendWheel(*knob, 80));
    QCOMPARE(knob->value(), 50.0);
    QVERIFY(sendWheel(*knob, 40));
    QCOMPARE(knob->value(), 51.0);
    QCOMPARE(committed.count(), 5);

    knob->setValueLimits(0.0, 52.0);
    QVERIFY(sendWheel(*knob, 360));
    QCOMPARE(knob->value(), 52.0);
    QCOMPARE(committed.count(), 6);
    QCOMPARE(committed.last().at(0).toDouble(), 52.0);
}

void tst_IndustrialKnob::focusRingIsASolidRingInTextColor_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::newRow("220x240") << 220 << 240;
    QTest::newRow("160x180") << 160 << 180;
}

// #183: with the focus the knob draws a solid ring in textColor() around the grip, 4 px out from it, in the gap before
// the band of the ticks: every pixel the pen covers in full is in textColor() (not dashed, not translucent), all
// around, and nothing else changes
void tst_IndustrialKnob::focusRingIsASolidRingInTextColor()
{
    QFETCH(int, width);
    QFETCH(int, height);

    QWidget window;
    // A dark background, far from the default textColor, as in the dark theme: on the light default background of
    // the window a gap or a translucent ring would look like textColor
    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, QColor(30, 33, 40));
    window.setPalette(palette);
    window.setAutoFillBackground(true);
    auto *knob = new IndustrialKnob(&window);
    auto *other = new QLineEdit(&window);
    knob->setRange(0.0, 100.0);
    knob->setValue(30.0);
    knob->setGeometry(0, 0, width, height);
    other->setGeometry(0, height + 10, width, 30);
    window.resize(width, height + 50);
    QVERIFY(focusOn(*other));
    const QImage plain = renderKnob(*knob);
    QVERIFY(focusOn(*knob));
    const QImage focused = renderKnob(*knob);

    const qreal dpr = knob->devicePixelRatioF();
    const internal::KnobScaleGeometry g = drawnGeometryOf(*knob, width, height);
    const QPointF center(g.centerX, g.centerY);
    const double grip = g.radius * 0.65;  // The knob, as paintEvent() draws it
    const double band = g.radius * 0.80;  // Inner end of the ticks: scaleRadius - tickLen
    const QVector<QPoint> changed = differingPixels(plain, focused);
    QVERIFY2(!changed.isEmpty(), "no focus indicator");
    for (const QPoint &p : changed) {
        const double r = distanceOf(p, center, dpr);
        QVERIFY2(r > grip + 0.5 && r < band - 0.5,
                 qPrintable(QStringLiteral("pixel (%1, %2) at radius %3, outside the gap %4-%5")
                                .arg(p.x()).arg(p.y()).arg(r).arg(grip).arg(band)));
    }
    // The pixels covered in full by the 2 px pen: the whole square of the pixel inside the annulus of the pen, from
    // ring - 1 to ring + 1 (its farthest corner within the outer edge, its nearest point outside the inner edge). They
    // must be in textColor; a pixel only partly covered mixes with the background, whatever the platform (#213)
    const double ring = grip + 4.0;
    const auto coveredInFull = [&](const QPoint &p) {
        const double left = p.x() / dpr - center.x();
        const double right = (p.x() + 1) / dpr - center.x();
        const double top = p.y() / dpr - center.y();
        const double bottom = (p.y() + 1) / dpr - center.y();
        const double farX = std::max(std::abs(left), std::abs(right));
        const double farY = std::max(std::abs(top), std::abs(bottom));
        const double nearX = (left <= 0.0 && right >= 0.0) ? 0.0 : std::min(std::abs(left), std::abs(right));
        const double nearY = (top <= 0.0 && bottom >= 0.0) ? 0.0 : std::min(std::abs(top), std::abs(bottom));
        return std::hypot(farX, farY) <= ring + 1.0 && std::hypot(nearX, nearY) >= ring - 1.0;
    };
    std::array<int, 72> perSector{}; // Pixels checked in each 5 degree sector
    int checked = 0;
    int off = 0;
    QString first;
    for (int y = 0; y < focused.height(); ++y) {
        for (int x = 0; x < focused.width(); ++x) {
            const QPoint p(x, y);
            if (!coveredInFull(p)) {
                continue;
            }
            // Without focus the ring is not there: what is under it must be far from textColor, or this test sees
            // nothing
            QVERIFY2(colourDistance(plain.pixel(p), knob->textColor()) > 200,
                     qPrintable(QStringLiteral("at (%1, %2) the background is too close to textColor").arg(x).arg(y)));
            ++checked;
            const double angle = std::fmod(angleOf(p, center, dpr) + 360.0, 360.0);
            ++perSector[std::min(71, static_cast<int>(angle / 5.0))];
            if (colourDistance(focused.pixel(p), knob->textColor()) > 48) {
                if (off++ == 0) {
                    const QRgb c = focused.pixel(p);
                    first = QStringLiteral("%1 degrees: rgb(%2, %3, %4)").arg(angle).arg(qRed(c)).arg(qGreen(c)).arg(qBlue(c));
                }
            }
        }
    }
    for (int i = 0; i < 72; ++i) {
        QVERIFY2(perSector[i] > 0, qPrintable(QStringLiteral("no pixel checked between %1 and %2 degrees").arg(i * 5).arg(i * 5 + 5)));
    }
    QVERIFY2(off == 0, qPrintable(QStringLiteral("%1 of %2 pixels of the ring are not textColor, the first at %3")
                                      .arg(off).arg(checked).arg(first)));
}

void tst_IndustrialKnob::focusTraversalReachesTheKnob_data()
{
    QTest::addColumn<bool>("readOnly");
    QTest::newRow("normal") << false;
    QTest::newRow("read-only") << true;
}

// #183: Tab and Shift+Tab bring the focus to the knob and take it away, also in read-only; a click gives it the focus;
// with the focus, the keys reach the knob through the real path (the focus widget of the window)
void tst_IndustrialKnob::focusTraversalReachesTheKnob()
{
    QFETCH(bool, readOnly);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *before = new QLineEdit(&window);
    auto *knob = new IndustrialKnob(&window);
    auto *after = new QLineEdit(&window);
    layout->addWidget(before);
    layout->addWidget(knob);
    layout->addWidget(after);
    knob->setRange(0.0, 100.0);
    knob->setValue(50.0);
    knob->setReadOnly(readOnly);
    knob->setMinimumSize(160, 180);
    QVERIFY(focusOn(*before));

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    QVERIFY2(knob->hasFocus(), "Tab did not reach the knob");
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    QVERIFY2(after->hasFocus(), "Tab did not leave the knob");
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backtab);
    QVERIFY2(knob->hasFocus(), "Shift+Tab did not reach the knob");
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backtab);
    QVERIFY2(before->hasFocus(), "Shift+Tab did not leave the knob");

    QTest::mouseClick(knob, Qt::LeftButton, Qt::NoModifier, QPoint(knob->width() / 2, knob->height() / 2));
    QVERIFY2(knob->hasFocus(), "a click did not give the focus");
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
    QCOMPARE(knob->value(), readOnly ? 50.0 : 51.0);
}

// #183: the accessible interface of the knob: role Dial, the name and description set by the application, the value
// text as the readout draws it (also with the display hidden), the value interface with value, range and step (in
// Discrete mode the step of a detent), and the read-only state
void tst_IndustrialKnob::accessibleInterfaceDescribesTheKnob()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 8000.0);
    knob.setPrecision(0);
    knob.setUnit(QStringLiteral("RPM"));
    knob.setStep(50.0);
    knob.setValue(2400.0);
    knob.setAccessibleName(QStringLiteral("Throttle"));
    knob.setAccessibleDescription(QStringLiteral("Target engine speed"));

    QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(&knob);
    QVERIFY(iface);
    QCOMPARE(iface->role(), QAccessible::Dial);
    QCOMPARE(iface->text(QAccessible::Name), QStringLiteral("Throttle"));
    QCOMPARE(iface->text(QAccessible::Description), QStringLiteral("Target engine speed"));
    QCOMPARE(iface->text(QAccessible::Value), QStringLiteral("2400 RPM"));
    QAccessibleValueInterface *value = iface->valueInterface();
    QVERIFY(value);
    QCOMPARE(value->currentValue().toDouble(), 2400.0);
    QCOMPARE(value->minimumValue().toDouble(), 0.0);
    QCOMPARE(value->maximumValue().toDouble(), 8000.0);
    QCOMPARE(value->minimumStepSize().toDouble(), 50.0);
    QVERIFY(!iface->state().readOnly);

    knob.setValueDisplayVisible(false);
    knob.setValue(1234.0);
    QCOMPARE(iface->text(QAccessible::Value), QStringLiteral("1234 RPM"));
    QCOMPARE(value->currentValue().toDouble(), 1234.0);

    knob.setReadOnly(true);
    QVERIFY(iface->state().readOnly);
    knob.setReadOnly(false);
    QVERIFY(!iface->state().readOnly);

    knob.setRange(0.0, 100.0);
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);
    knob.setValue(25.0);
    QCOMPARE(iface->text(QAccessible::Value), QStringLiteral("POS 2"));
    QCOMPARE(value->minimumStepSize().toDouble(), 25.0);
    QCOMPARE(value->maximumValue().toDouble(), 100.0);
}

// #183: without an accessible name the knob has none: the unit or anything else is not made into a name
void tst_IndustrialKnob::accessibleNameHasNoFallback()
{
    IndustrialKnob knob;
    knob.setUnit(QStringLiteral("bar"));
    QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(&knob);
    QVERIFY(iface);
    QCOMPARE(iface->role(), QAccessible::Dial);
    QCOMPARE(iface->text(QAccessible::Name), QString());
}

// #183: the knob is focusable, and focused while it has the focus
void tst_IndustrialKnob::accessibleFocusState()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *knob = new IndustrialKnob(&window);
    auto *other = new QLineEdit(&window);
    layout->addWidget(knob);
    layout->addWidget(other);
    QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(knob);
    QVERIFY(iface);
    QVERIFY(iface->state().focusable);
    QVERIFY(focusOn(*other));
    QVERIFY(!iface->state().focused);
    QVERIFY(focusOn(*knob));
    QVERIFY(iface->state().focused);
}

// #183: a value set by assistive technology is operator input: bound by the value limits, ignored in read-only, and
// committed (valueChanged() and valueCommitted()); a value that changes nothing, or that is not a finite number,
// commits nothing
void tst_IndustrialKnob::accessibleSetValueIsOperatorInput()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 8000.0);
    knob.setValue(2400.0);
    QAccessibleValueInterface *value = QAccessible::queryAccessibleInterface(&knob)->valueInterface();
    QVERIFY(value);
    QSignalSpy changed(&knob, &IndustrialKnob::valueChanged);
    QSignalSpy committed(&knob, &IndustrialKnob::valueCommitted);

    value->setCurrentValue(3000.0);
    QCOMPARE(knob.value(), 3000.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.last().at(0).toDouble(), 3000.0);

    value->setCurrentValue(3000.0);
    value->setCurrentValue(std::numeric_limits<double>::quiet_NaN());
    value->setCurrentValue(QStringLiteral("not a number"));
    QCOMPARE(knob.value(), 3000.0);
    QCOMPARE(committed.count(), 1);

    knob.setValueLimits(1000.0, 5000.0);
    value->setCurrentValue(7000.0);
    QCOMPARE(knob.value(), 5000.0);
    QCOMPARE(committed.count(), 2);
    QCOMPARE(committed.last().at(0).toDouble(), 5000.0);

    knob.setReadOnly(true);
    const int changes = changed.count();
    value->setCurrentValue(2000.0);
    QCOMPARE(knob.value(), 5000.0);
    QCOMPARE(changed.count(), changes);
    QCOMPARE(committed.count(), 2);
}

// #183: one value-change event at every change of the value, from every source (key, wheel, drag, assistive
// technology, setValue(), the clamping of setRange()), none when the value does not change; a range change sends one
// too; a state-change event when read-only changes
void tst_IndustrialKnob::accessibleEventsFollowTheValue()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *knob = new IndustrialKnob(&window);
    layout->addWidget(knob);
    knob->setRange(0.0, 100.0);
    knob->setStep(1.0);
    knob->setValue(50.0);
    knob->setMinimumSize(220, 240);
    knob->setWheelEnabled(true);
    QVERIFY(focusOn(*knob));
    const DrawnDial dial = drawnDialOf(*knob);
    QVERIFY(dial.radius > 20.0);

    AccessibleEvents events(*knob);
    using Ev = AccessibleEvents::Event;
    // The values of the events, which must all be value changes of the knob (-1 marks any other event)
    const auto values = [knob](const QList<Ev> &list) {
        QList<double> out;
        for (const Ev &e : list) {
            out.push_back(e.type == QAccessible::ValueChanged && e.object == knob ? e.value : -1.0);
        }
        return out;
    };

    knob->setValue(10.0);
    QCOMPARE(values(events.take()), QList<double>{10.0});
    knob->setValue(10.0);
    QCOMPARE(values(events.take()), QList<double>{});

    QTest::keyClick(knob, Qt::Key_Up);
    QCOMPARE(values(events.take()), QList<double>{11.0});
    QTest::keyClick(knob, Qt::Key_Home);
    QCOMPARE(values(events.take()), QList<double>{0.0});
    QTest::keyClick(knob, Qt::Key_Home); // Already there
    QCOMPARE(values(events.take()), QList<double>{});

    QVERIFY(sendWheel(*knob, 120));
    QCOMPARE(values(events.take()), QList<double>{1.0});

    knob->setValue(50.0);
    events.take();
    sendMouse(*knob, QEvent::MouseButtonPress, dialPoint(dial, 0.0));
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 45.0));
    sendMouse(*knob, QEvent::MouseMove, dialPoint(dial, 90.0));
    sendMouse(*knob, QEvent::MouseButtonRelease, dialPoint(dial, 90.0));
    const QList<double> dragged = values(events.take());
    QCOMPARE(dragged.size(), 2);
    QCOMPARE(dragged.last(), knob->value());

    QAccessible::queryAccessibleInterface(knob)->valueInterface()->setCurrentValue(40.0);
    QCOMPARE(values(events.take()), QList<double>{40.0});

    knob->setRange(0.0, 30.0); // Clamps the value
    QCOMPARE(values(events.take()), QList<double>{30.0});
    knob->setRange(0.0, 200.0); // Keeps the value; the range changed
    QCOMPARE(values(events.take()), QList<double>{30.0});

    knob->setReadOnly(true);
    const QList<Ev> state = events.take();
    QCOMPARE(state.size(), 1);
    QCOMPARE(state.first().type, QAccessible::StateChanged);
    QCOMPARE(state.first().object, static_cast<QObject *>(knob));
    QVERIFY(state.first().readOnly);
    knob->setReadOnly(true);
    QCOMPARE(events.take().size(), 0);
    knob->setReadOnly(false);
    QCOMPARE(events.take().size(), 1);
}

#include "tst_IndustrialKnob.moc"


