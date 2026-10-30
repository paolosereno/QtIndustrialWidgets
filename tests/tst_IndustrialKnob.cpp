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

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
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

ScaleLabels scaleLabelsOf(const IndustrialKnob &knob)
{
    return scaleLabelsAt(knob, internal::knobScaleGeometry(knob.width(), knob.height(), [&](const internal::KnobScaleGeometry &g) {
        const ScaleLabels at = scaleLabelsAt(knob, g);
        std::vector<internal::LayoutRect> drawn;
        for (int i : at.selection.drawn) {
            drawn.push_back(at.boxes[static_cast<size_t>(i)]);
        }
        return drawn;
    }));
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
    for (const QPoint &p : scale) {
        dial.radius = std::max(dial.radius, std::hypot((p.x() + 0.5) / dpr - dial.center.x(), (p.y() + 0.5) / dpr - dial.center.y()));
    }
    return dial;
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
    QVERIFY2(onEdge == 0, qPrintable(QStringLiteral("%1 pixels of label ink on the edge of the widget").arg(onEdge)));
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
    QVERIFY2(std::find(labels.selection.drawn.begin(), labels.selection.drawn.end(), 0) != labels.selection.drawn.end(),
             "the first label is not selected");
    QVERIFY2(std::find(labels.selection.drawn.begin(), labels.selection.drawn.end(), last) != labels.selection.drawn.end(),
             "the last label is not selected");

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


