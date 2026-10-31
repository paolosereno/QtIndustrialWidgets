// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialKnob.h>

#include "FontTestUtils.h"
#include "LabelInkTestUtils.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"

#include <memory>
#include <QtGui/QPixmap>

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
        out.texts.push_back(text);
        out.boxes.push_back(internal::knobLabelBox(geometry, knob.startAngle() + frac * knob.spanAngle(),
                                                   rect.width(), rect.height()));
    }
    out.minGap = internal::scaleLabelMinGap(fm.height());
    out.selection = internal::layoutRadialScaleLabels(out.boxes, out.minGap);
    return out;
}

std::vector<internal::LayoutRect> drawnBoxes(const ScaleLabels &labels)
{
    std::vector<internal::LayoutRect> drawn;
    for (int i : labels.selection.drawn) {
        drawn.push_back(labels.boxes[static_cast<size_t>(i)]);
    }
    return drawn;
}

internal::KnobScaleGeometry drawnGeometryOf(const IndustrialKnob &knob, int width, int height)
{
    return internal::knobScaleGeometry(width, height, [&](const internal::KnobScaleGeometry &g) {
        return drawnBoxes(scaleLabelsAt(knob, g));
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
           && internal::knobLabelsInside(drawnBoxes(labels), width, height);
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

} // namespace

class tst_IndustrialKnob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
    void clickOnTheDrawnDialSelectsItsValue_data();
    void clickOnTheDrawnDialSelectsItsValue();
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
    void dialFollowsTheDrawnLabels_data();
    void dialFollowsTheDrawnLabels();
    void precisionChangesTheLabelsDrawn();
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

    // Continuous mode: NaN ignored
    knob.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(knob.value(), 50.0);

    // +Inf and -Inf clamped
    knob.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(knob.value(), 100.0);
    knob.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(knob.value(), 0.0);

    // Discrete mode: NaN must not cause undefined behavior / crash
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);
    knob.setValue(50.0);
    knob.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(knob.value(), 50.0);

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
    int onEdge = 0;
    for (const QPoint &p : ink) {
        onEdge += (p.x() == 0 || p.y() == 0 || p.x() == first.width() - 1 || p.y() == first.height() - 1) ? 1 : 0;
    }
    // DIAGNOSTIC of the macOS failure, not to be kept
    QString detail;
    if (onEdge > 0) {
        const ScaleLabels labels = scaleLabelsOf(knob);
        const qreal dpr = knob.devicePixelRatioF();
        for (int i : labels.selection.drawn) {
            const internal::LayoutRect &box = labels.boxes[static_cast<size_t>(i)];
            QImage image(QSize(400, 80) * dpr, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(dpr);
            image.fill(Qt::black);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setFont(labels.font);
            painter.setPen(Qt::white);
            const QRectF at(100.0 + (box.x - std::floor(box.x)), 30.0 + (box.y - std::floor(box.y)), box.w, box.h);
            painter.drawText(at, Qt::AlignCenter | Qt::TextDontClip, labels.texts[static_cast<size_t>(i)]);
            painter.end();
            QRect inkBox;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (qRed(image.pixel(x, y)) > 0) {
                        inkBox |= QRect(x, y, 1, 1);
                    }
                }
            }
            detail += QStringLiteral(" [%1: box %2..%3 x %4..%5 ink out L %6 R %7 T %8 B %9]")
                          .arg(labels.texts[static_cast<size_t>(i)]).arg(box.x).arg(box.right()).arg(box.y).arg(box.bottom())
                          .arg(at.left() - inkBox.left() / dpr).arg((inkBox.right() + 1) / dpr - at.right())
                          .arg(at.top() - inkBox.top() / dpr).arg((inkBox.bottom() + 1) / dpr - at.bottom());
        }
    }
    QVERIFY2(onEdge == 0, qPrintable(QStringLiteral("%1 pixels of label ink on the edge of the widget, DPR %2:%3").arg(onEdge).arg(knob.devicePixelRatioF()).arg(detail)));
}

void tst_IndustrialKnob::clickOnTheDrawnDialSelectsItsValue_data()
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

// #170: the mouse reads the angle around the dial that is drawn. The point clicked is taken from the pixels of the
// dial, not from its formulas: on its rim, at the angle of the value. 1.5 px across the radius covers the rounding of
// the click to a pixel and of the centre measured
void tst_IndustrialKnob::clickOnTheDrawnDialSelectsItsValue()
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
    for (double angle : {-90.0, -45.0, 0.0, 45.0, 90.0}) {
        const double rad = angle * M_PI / 180.0;
        const QPointF point(dial.center.x() + dial.radius * std::sin(rad), dial.center.y() - dial.radius * std::cos(rad));
        const double expected = maximum * (angle - knob.startAngle()) / knob.spanAngle();
        QTest::mouseClick(&knob, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QVERIFY2(std::abs(knob.value() - expected) <= tolerance,
                 qPrintable(QStringLiteral("angle %1: value %2, expected %3 +/- %4 (dial at %5, %6, radius %7)")
                                .arg(angle).arg(knob.value()).arg(expected).arg(tolerance)
                                .arg(dial.center.x()).arg(dial.center.y()).arg(dial.radius)));
    }
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
    for (int stretch : {150, 200, 250, 300, 400}) {
        IndustrialKnob knob;
        knob.setRange(0.0, 1000000.0);
        QFont wide = knob.font();
        wide.setStretch(stretch);
        knob.setFont(wide);

        const int hint = knob.minimumSizeHint().width();
        // Where the ends do not fit even at the ceiling only the first one is required
        if (hint <= 80 || hint >= 160 || !sizeKeepsTheEnds(knob, 160, 180)) {
            continue;
        }
        ++checked;
        QVERIFY2(sizeKeepsTheEnds(knob, hint, (hint * 9 + 7) / 8), qPrintable(QStringLiteral("stretch %1 at %2").arg(stretch).arg(hint)));
        QVERIFY2(!sizeKeepsTheEnds(knob, hint - 1, ((hint - 1) * 9 + 7) / 8),
                 qPrintable(QStringLiteral("stretch %1: %2 is enough, the minimum is %3").arg(stretch).arg(hint - 1).arg(hint)));
    }
    QVERIFY2(checked > 0, "no stretched font gives a size between the floor and the ceiling");
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
            return internal::knobLabelsInside(drawnBoxes(scaleLabelsAt(knob, drawnGeometryOf(knob, width, height))), width, height);
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

#include "tst_IndustrialKnob.moc"


