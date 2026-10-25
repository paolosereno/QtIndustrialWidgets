// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialSwitch.h>

#include "FontTestUtils.h"
#include "internal/IndustrialSwitchLayout.h"
#include <QtGui/QImage>
#include <QtGui/QPixmap>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace QtIndustrialWidgets;

namespace {

// #44: the switch configurations of the gallery sizes
struct SwitchConfig {
    IndustrialSwitch::SwitchType type{IndustrialSwitch::SwitchType::ToggleLever};
    Qt::Orientation orientation{Qt::Vertical};
    QSize size;
    int positions{2};
    int sizeMode{0}; // 0: size, 1: sizeHint() of the guarded switch, 2: its minimumSizeHint()
};

struct SwitchTexts {
    QString title = QStringLiteral("EMERGENCY");
    QString on = QStringLiteral("ARMED");
    QString off = QStringLiteral("SAFE");
    QString center = QStringLiteral("AUTO");
};

std::unique_ptr<IndustrialSwitch> makeSwitch(const SwitchConfig &c, bool guard, bool open, const SwitchTexts &t = {})
{
    auto sw = std::make_unique<IndustrialSwitch>();
    sw->setAnimated(false);
    sw->setSwitchType(c.type);
    sw->setOrientation(c.orientation);
    sw->setPositionCount(c.positions);
    sw->setHasLed(true);
    sw->setHasSafetyGuard(guard);
    if (guard) {
        sw->setGuardOpen(open);
    }
    sw->setLabel(t.title);
    sw->setLabelOn(t.on);
    sw->setLabelOff(t.off);
    sw->setLabelCenter(c.positions == 3 ? t.center : QString());
    sw->setChecked(true); // LED lit, with its glow
    if (c.sizeMode != 0) {
        // the size hint, or the minimum size hint, of the same switch with a guard
        auto guarded = makeSwitch(SwitchConfig{c.type, c.orientation, QSize(100, 100), c.positions}, true, false, t);
        sw->resize(c.sizeMode == 1 ? guarded->sizeHint() : guarded->minimumSizeHint());
    } else {
        sw->resize(c.size);
    }
    return sw;
}

// #44: from minimumSizeHint() up, no label is clipped or covered; below it, only "no label covered"
// is guaranteed. The tests decide per switch by comparing with its minimumSizeHint().
bool atLeastMinimum(const IndustrialSwitch &sw)
{
    const QSize min = sw.minimumSizeHint();
    return sw.width() >= min.width() && sw.height() >= min.height();
}

void noteReducedCriterion(const IndustrialSwitch &sw)
{
    const QSize min = sw.minimumSizeHint();
    qInfo("%dx%d is below minimumSizeHint() %dx%d: only \"no label covered\" applies", sw.width(), sw.height(), min.width(), min.height());
}

// The switch rendered at the screen device pixel ratio (QT_SCALE_FACTOR in ctest)
QImage grabImage(IndustrialSwitch &sw)
{
    return sw.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// Device pixels that differ between two renders of the same size, sorted
std::vector<QPoint> differingPixels(const QImage &a, const QImage &b)
{
    std::vector<QPoint> out;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

int commonPixels(const std::vector<QPoint> &a, const std::vector<QPoint> &b)
{
    auto less = [](const QPoint &p, const QPoint &q) { return p.y() != q.y() ? p.y() < q.y() : p.x() < q.x(); };
    int n = 0;
    for (const QPoint &p : a) {
        n += std::binary_search(b.begin(), b.end(), p, less) ? 1 : 0;
    }
    return n;
}

// The pixels of one element are isolated by changing only its public colour: the layout does not
// change between the two renders (#44: labels move with the guard and the switch size depends on
// the texts, so renders with and without labels or guard cannot be compared pixel by pixel).
std::vector<QPoint> textInk(IndustrialSwitch &sw)
{
    const QColor original = sw.textColor();
    sw.setTextColor(Qt::white);
    const QImage a = grabImage(sw);
    sw.setTextColor(Qt::red);
    const QImage b = grabImage(sw);
    sw.setTextColor(original);
    return differingPixels(a, b);
}

std::vector<QPoint> guardPixels(IndustrialSwitch &sw)
{
    const QColor original = sw.guardColor();
    const QImage a = grabImage(sw);
    sw.setGuardColor(Qt::blue);
    const QImage b = grabImage(sw);
    sw.setGuardColor(original);
    return differingPixels(a, b);
}

std::vector<QPoint> ledPixels(IndustrialSwitch &sw)
{
    const QColor original = sw.ledColor();
    const QImage a = grabImage(sw);
    sw.setLedColor(Qt::blue);
    const QImage b = grabImage(sw);
    sw.setLedColor(original);
    return differingPixels(a, b);
}

QRect boundingBox(const std::vector<QPoint> &pixels)
{
    QRect box;
    for (const QPoint &p : pixels) {
        box |= QRect(p, QSize(1, 1));
    }
    return box;
}

double linearChannel(int c)
{
    const double v = c / 255.0;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

// WCAG 2.x relative luminance
double luminance(QRgb p)
{
    return 0.2126 * linearChannel(qRed(p)) + 0.7152 * linearChannel(qGreen(p)) + 0.0722 * linearChannel(qBlue(p));
}

void addSwitchRows()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("orientation");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("positions");
    QTest::addColumn<int>("sizeMode");
    using T = IndustrialSwitch::SwitchType;
    for (T type : {T::ToggleLever, T::Rocker}) {
        for (int positions : {2, 3}) {
            for (const QSize &size : {QSize(120, 190), QSize(180, 220)}) {
                QTest::addRow("%s V %dx%d %d-pos", type == T::Rocker ? "rocker" : "toggle", size.width(), size.height(), positions)
                    << static_cast<int>(type) << static_cast<int>(Qt::Vertical) << size << positions << 0;
            }
            for (const QSize &size : {QSize(180, 120), QSize(220, 120)}) {
                QTest::addRow("%s H %dx%d %d-pos", type == T::Rocker ? "rocker" : "toggle", size.width(), size.height(), positions)
                    << static_cast<int>(type) << static_cast<int>(Qt::Horizontal) << size << positions << 0;
            }
            // At minimumSizeHint() of the switch with a guard
            for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
                QTest::addRow("%s %s minimumSizeHint %d-pos", type == T::Rocker ? "rocker" : "toggle", o == Qt::Vertical ? "V" : "H", positions)
                    << static_cast<int>(type) << static_cast<int>(o) << QSize() << positions << 2;
            }
        }
        // At sizeHint() of the switch with a guard and 3 positions
        for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
            QTest::addRow("%s %s sizeHint 3-pos", type == T::Rocker ? "rocker" : "toggle", o == Qt::Vertical ? "V" : "H")
                << static_cast<int>(type) << static_cast<int>(o) << QSize() << 3 << 1;
        }
    }
}

SwitchConfig fetchConfig()
{
    QFETCH(int, type);
    QFETCH(int, orientation);
    QFETCH(QSize, size);
    QFETCH(int, positions);
    QFETCH(int, sizeMode);
    return {static_cast<IndustrialSwitch::SwitchType>(type), static_cast<Qt::Orientation>(orientation), size, positions, sizeMode};
}

} // namespace

class tst_IndustrialSwitch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fontChangeRefreshesCache();
    void defaultValues();
    void twoPositionToggle();
    void threePositionSwitch();
    void safetyGuardBehavior();
    void switchStylesAndOrientations();
    void signalEmission();
    void keyboardInteraction();
    void closedGuardBlocksUserInput_data();
    void closedGuardBlocksUserInput();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void ledDoesNotCoverLabels_data() { addSwitchRows(); }
    void ledDoesNotCoverLabels();
    void guardDoesNotCoverLabels_data() { addSwitchRows(); }
    void guardDoesNotCoverLabels();
    void guardStaysInsideWidget_data() { addSwitchRows(); }
    void guardStaysInsideWidget();
    void closedGuardLabelsReadable_data() { addSwitchRows(); }
    void closedGuardLabelsReadable();
    void labelsNotClipped_data() { addSwitchRows(); }
    void labelsNotClipped();
    void labelInkBeyondAdvanceStaysInPlate();
    void widerFontFullCriterionAtMinimumSize_data();
    void widerFontFullCriterionAtMinimumSize();
    void sizeHintKeepsGuardedThreePositionSwitchReadable_data();
    void sizeHintKeepsGuardedThreePositionSwitchReadable();
};

void tst_IndustrialSwitch::defaultValues()
{
    IndustrialSwitch sw;
    QCOMPARE(sw.positionCount(), 2);
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
    QCOMPARE(sw.switchType(), IndustrialSwitch::SwitchType::ToggleLever);
    QCOMPARE(sw.hasSafetyGuard(), false);
}

void tst_IndustrialSwitch::twoPositionToggle()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    sw.setChecked(true);
    QCOMPARE(sw.position(), 1);
    QCOMPARE(sw.isChecked(), true);

    sw.toggle();
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
}

void tst_IndustrialSwitch::threePositionSwitch()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setPositionCount(3);

    QCOMPARE(sw.positionCount(), 3);
    sw.setPosition(2);
    QCOMPARE(sw.position(), 2);
    QCOMPARE(sw.isChecked(), true);

    sw.setPosition(1);
    QCOMPARE(sw.position(), 1);
    QCOMPARE(sw.isChecked(), false);
}

void tst_IndustrialSwitch::safetyGuardBehavior()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setHasSafetyGuard(true);

    QCOMPARE(sw.hasSafetyGuard(), true);
    QCOMPARE(sw.isGuardOpen(), false);

    QSignalSpy guardSpy(&sw, &IndustrialSwitch::guardToggled);
    sw.setGuardOpen(true);

    QCOMPARE(guardSpy.count(), 1);
    QCOMPARE(sw.isGuardOpen(), true);

    sw.setGuardOpen(false);
    QCOMPARE(sw.isGuardOpen(), false);
}

void tst_IndustrialSwitch::switchStylesAndOrientations()
{
    IndustrialSwitch sw;
    sw.setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    QCOMPARE(sw.switchType(), IndustrialSwitch::SwitchType::Rocker);

    sw.setOrientation(Qt::Horizontal);
    QCOMPARE(sw.orientation(), Qt::Horizontal);
}

void tst_IndustrialSwitch::signalEmission()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
    QSignalSpy toggleSpy(&sw, &IndustrialSwitch::toggled);

    sw.setPosition(1);
    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(toggleSpy.count(), 1);
}

void tst_IndustrialSwitch::keyboardInteraction()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 1);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 0);
}

// #142: with the safety guard closed, no key, click, drag or wheel step changes the position; the only
// effect of user input is opening the guard (click, Space, Return), as docs/widgets.md says
void tst_IndustrialSwitch::closedGuardBlocksUserInput_data()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<int>("positions");
    QTest::addColumn<int>("start");
    for (const Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
        const char *on = o == Qt::Vertical ? "vertical" : "horizontal";
        QTest::addRow("%s, 2 positions, at 0", on) << int(o) << 2 << 0;
        QTest::addRow("%s, 2 positions, at 1", on) << int(o) << 2 << 1;
        QTest::addRow("%s, 3 positions, at 1", on) << int(o) << 3 << 1;
    }
}

void tst_IndustrialSwitch::closedGuardBlocksUserInput()
{
    QFETCH(int, orientation);
    QFETCH(int, positions);
    QFETCH(int, start);
    const auto o = static_cast<Qt::Orientation>(orientation);

    IndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setOrientation(o);
    sw.setPositionCount(positions);
    sw.resize(o == Qt::Vertical ? QSize(100, 160) : QSize(160, 100));
    sw.setHasSafetyGuard(true);

    // Puts the switch back at the start position with the guard closed
    const auto rearm = [&] {
        sw.setGuardOpen(false);
        sw.setPosition(start);
        QCOMPARE(sw.position(), start);
        QVERIFY(!sw.isGuardOpen());
    };

    // Keys: those that open the guard, and those that move an unguarded switch
    const QList<Qt::Key> openingKeys{Qt::Key_Space, Qt::Key_Return};
    const QList<Qt::Key> movingKeys{Qt::Key_Up, Qt::Key_Right, Qt::Key_Down, Qt::Key_Left};
    for (const Qt::Key key : openingKeys + movingKeys) {
        rearm();
        QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
        QTest::keyClick(&sw, key);
        const QString what = QStringLiteral("key ") + QKeySequence(key).toString();
        QVERIFY2(sw.position() == start && posSpy.isEmpty(),
                 qPrintable(what + QStringLiteral(" moved the switch to %1 with the guard closed").arg(sw.position())));
        QVERIFY2(sw.isGuardOpen() == openingKeys.contains(key),
                 qPrintable(what + (sw.isGuardOpen() ? QStringLiteral(" opened") : QStringLiteral(" did not open"))
                            + QStringLiteral(" the guard")));
    }

    // Control: with the guard open, the same moving keys do move the switch when the move is possible,
    // so the keys above did reach the widget
    for (const Qt::Key key : movingKeys) {
        const bool up = key == Qt::Key_Up || key == Qt::Key_Right;
        if ((up && start == positions - 1) || (!up && start == 0)) {
            continue;
        }
        rearm();
        sw.setGuardOpen(true);
        QTest::keyClick(&sw, key);
        QCOMPARE(sw.position(), up ? start + 1 : start - 1);
    }

    // Clicks over the whole widget: each one may only open the guard
    const QRect r = sw.rect();
    const QList<QPoint> points{r.center(), QPoint(r.center().x(), r.top() + 8), QPoint(r.center().x(), r.bottom() - 8),
                               QPoint(r.left() + 8, r.center().y()), QPoint(r.right() - 8, r.center().y())};
    for (const QPoint &pt : points) {
        rearm();
        QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
        QTest::mouseClick(&sw, Qt::LeftButton, Qt::NoModifier, pt);
        QVERIFY2(sw.position() == start && posSpy.isEmpty(),
                 qPrintable(QStringLiteral("a click at (%1, %2) moved the switch to %3 with the guard closed")
                                .arg(pt.x()).arg(pt.y()).arg(sw.position())));
        QVERIFY2(sw.isGuardOpen(), "a click did not open the guard");
    }

    // Drag across the switch with the guard closed (the press opens the guard; the drag must not move it)
    rearm();
    {
        QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
        const QPoint from = o == Qt::Vertical ? QPoint(r.center().x(), r.bottom() - 8) : QPoint(r.left() + 8, r.center().y());
        const QPoint to = o == Qt::Vertical ? QPoint(r.center().x(), r.top() + 8) : QPoint(r.right() - 8, r.center().y());
        QTest::mousePress(&sw, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(&sw, to);
        QTest::mouseRelease(&sw, Qt::LeftButton, Qt::NoModifier, to);
        QVERIFY2(sw.position() == start && posSpy.isEmpty(), "a drag moved the switch with the guard closed");
    }

    // Wheel steps in both directions
    for (const int delta : {120, -120}) {
        rearm();
        QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
        QWheelEvent wheel(QPointF(r.center()), sw.mapToGlobal(QPointF(r.center())), QPoint(), QPoint(0, delta),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&sw, &wheel);
        QVERIFY2(sw.position() == start && posSpy.isEmpty(),
                 qPrintable(QStringLiteral("a wheel step of %1 moved the switch with the guard closed").arg(delta)));
        QVERIFY(!sw.isGuardOpen());
    }
}

void tst_IndustrialSwitch::renderOffscreen()
{
    IndustrialSwitch sw;
    sw.setLabel(QStringLiteral("TEST"));
    sw.setHasSafetyGuard(true);
    sw.setChecked(true);
    sw.resize(80, 130);

    QPixmap pix(sw.size());
    sw.render(&pix);
    QVERIFY(!pix.isNull());

    // Rocker mode
    sw.setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    QPixmap pixRocker(sw.size());
    sw.render(&pixRocker);
    QVERIFY(!pixRocker.isNull());
}

void tst_IndustrialSwitch::extremeResizeNoCrash()
{
    IndustrialSwitch sw;
    sw.resize(2, 2);
    QPixmap pixSmall(sw.size());
    sw.render(&pixSmall);

    sw.resize(2560, 1440);
    QPixmap pixLarge(sw.size());
    sw.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

// #44: no pixel of the title or of a position label is painted over by the (lit) LED
void tst_IndustrialSwitch::ledDoesNotCoverLabels()
{
    const SwitchConfig c = fetchConfig();
    auto sw = makeSwitch(c, false, false);
    const std::vector<QPoint> text = textInk(*sw);
    const std::vector<QPoint> led = ledPixels(*sw);
    QVERIFY(!text.empty());
    QVERIFY(!led.empty());
    QCOMPARE(commonPixels(led, text), 0);
}

// #44: the closed guard leaves every label pixel exactly as with the guard open (the layout depends
// on the presence of the guard, not on its state), and the open guard paints over no label pixel
void tst_IndustrialSwitch::guardDoesNotCoverLabels()
{
    const SwitchConfig c = fetchConfig();
    auto closed = makeSwitch(c, true, false);
    auto open = makeSwitch(c, true, true);
    const std::vector<QPoint> closedText = textInk(*closed);
    const std::vector<QPoint> openText = textInk(*open);
    QVERIFY(!openText.empty());
    const int lost = static_cast<int>(openText.size()) - commonPixels(closedText, openText);
    QVERIFY2(closedText == openText, qPrintable(QStringLiteral("closed guard: %1 of %2 label pixels differ from the open-guard render")
                                                     .arg(lost).arg(openText.size())));
    const std::vector<QPoint> openGuard = guardPixels(*open);
    QVERIFY(!openGuard.empty());
    QCOMPARE(commonPixels(openGuard, openText), 0);
}

// #44: the guard, closed or open, is drawn inside the widget. QWidget::render() clips to the widget,
// so a guard that leaves it shows up as guard pixels on the outermost row or column of the widget.
void tst_IndustrialSwitch::guardStaysInsideWidget()
{
    const SwitchConfig c = fetchConfig();
    for (bool open : {false, true}) {
        auto sw = makeSwitch(c, true, open);
        if (!atLeastMinimum(*sw)) {
            noteReducedCriterion(*sw);
            return;
        }
        const std::vector<QPoint> guard = guardPixels(*sw);
        QVERIFY(!guard.empty());
        const QImage img = grabImage(*sw);
        int onBorder = 0;
        for (const QPoint &p : guard) {
            onBorder += (p.x() == 0 || p.y() == 0 || p.x() == img.width() - 1 || p.y() == img.height() - 1) ? 1 : 0;
        }
        QVERIFY2(onBorder == 0, qPrintable(QStringLiteral("%1 guard pixels on the widget border (guard %2)")
                                               .arg(onBorder).arg(open ? QStringLiteral("open") : QStringLiteral("closed"))));
    }
}

// #44: with the guard closed, every label reaches a WCAG contrast of at least 4.5:1 against what is
// behind it, measured on the half of its ink pixels that differ most from the background (glyph
// core) with the default colours. Each label is rendered alone; "behind" is the same render with a
// transparent text colour, so the layout is the same.
void tst_IndustrialSwitch::closedGuardLabelsReadable()
{
    const SwitchConfig c = fetchConfig();
    const SwitchTexts all;
    if (auto full = makeSwitch(c, true, false); !atLeastMinimum(*full)) {
        noteReducedCriterion(*full);
        return;
    }
    const QString labels[] = {all.title, all.on, all.off, all.center};
    QStringList report;
    double worst = 1e9;
    for (int i = 0; i < (c.positions == 3 ? 4 : 3); ++i) {
        SwitchTexts one;
        one.title = i == 0 ? labels[0] : QString();
        one.on = i == 1 ? labels[1] : QString();
        one.off = i == 2 ? labels[2] : QString();
        one.center = i == 3 ? labels[3] : QString();
        auto sw = makeSwitch(c, true, false, one);
        const QImage withText = grabImage(*sw);
        sw->setTextColor(QColor(0, 0, 0, 0));
        const QImage without = grabImage(*sw);
        struct Px { QPoint p; double d; };
        std::vector<Px> ink;
        for (const QPoint &p : differingPixels(withText, without)) {
            ink.push_back({p, std::abs(luminance(withText.pixel(p)) - luminance(without.pixel(p)))});
        }
        QVERIFY2(ink.size() > 10, qPrintable(QStringLiteral("label '%1' has no visible ink with the guard closed (fully hidden)").arg(labels[i])));
        std::sort(ink.begin(), ink.end(), [](const Px &a, const Px &b) { return a.d > b.d; });
        const size_t core = ink.size() / 2;
        double text = 0.0, back = 0.0;
        for (size_t k = 0; k < core; ++k) {
            text += luminance(withText.pixel(ink[k].p));
            back += luminance(without.pixel(ink[k].p));
        }
        text /= core;
        back /= core;
        const double ratio = (std::max(text, back) + 0.05) / (std::min(text, back) + 0.05);
        worst = std::min(worst, ratio);
        report << QStringLiteral("%1 %2:1").arg(labels[i]).arg(ratio, 0, 'f', 2);
    }
    QVERIFY2(worst >= 4.5, qPrintable(QStringLiteral("contrast below 4.5:1 with the guard closed: %1").arg(report.join(QStringLiteral(", ")))));
}

// #44: every label is drawn whole, inside the plate: its ink is as wide (within 1 device px) as the
// same label in a large switch of the same kind, where nothing can clip it. Rendered with and
// without a guard (open: see below).
void tst_IndustrialSwitch::labelsNotClipped()
{
    const SwitchConfig c = fetchConfig();
    SwitchConfig large = c;
    large.size = c.orientation == Qt::Vertical ? QSize(600, 800) : QSize(800, 400);
    const SwitchTexts all;
    const QString labels[] = {all.title, all.on, all.off, all.center};
    for (bool guard : {false, true}) {
        if (auto full = makeSwitch(c, guard, true); !atLeastMinimum(*full)) {
            noteReducedCriterion(*full);
            continue;
        }
        for (int i = 0; i < (c.positions == 3 ? 4 : 3); ++i) {
            SwitchTexts one;
            one.title = i == 0 ? labels[0] : QString();
            one.on = i == 1 ? labels[1] : QString();
            one.off = i == 2 ? labels[2] : QString();
            one.center = i == 3 ? labels[3] : QString();
            // With a guard, measured with the guard open: the layout depends on the presence of the
            // guard, not on its state, and the closed cover must not mix covering with clipping
            auto sw = makeSwitch(c, guard, true, one);
            auto reference = makeSwitch(large, guard, true, one);
            const QRect ink = boundingBox(textInk(*sw));
            const QRect full = boundingBox(textInk(*reference));
            QVERIFY(!full.isEmpty());
            QVERIFY2(std::abs(ink.width() - full.width()) <= 1,
                     qPrintable(QStringLiteral("label '%1' (guard %2) is %3 device px wide, %4 when nothing clips it")
                                    .arg(labels[i]).arg(guard).arg(ink.width()).arg(full.width())));
            const qreal dpr = grabImage(*sw).devicePixelRatio();
            const QRect plate = QRect(QPoint(0, 0), sw->size() * dpr).adjusted(qFloor(3 * dpr), qFloor(3 * dpr), -qFloor(3 * dpr), -qFloor(3 * dpr));
            QVERIFY2(plate.contains(ink), qPrintable(QStringLiteral("label '%1' ink outside the plate").arg(labels[i])));
        }
    }
}

// #44 (found by CI on Windows): with a font whose glyph ink extends past the advance width (bold
// serif), the layout draws the side labels so that their ink, measured here independently with
// QFontMetricsF::tightBoundingRect(), stays inside the space beside the guard: a right-aligned label
// ends its ink at the space's right edge, a left-aligned one starts it at the left edge.
void tst_IndustrialSwitch::labelInkBeyondAdvanceStaysInPlate()
{
    // Any font works for the check; DejaVu Serif bold has ink past the advance on both sides
    QFont serif = QApplication::font();
    serif.setFamily(QStringLiteral("DejaVu Serif"));
    const QFont labelFont = internal::switchLabelFont(serif);
    const QFontMetricsF fm(labelFont);
    auto inkLeft = [&](const QRectF &rect, Qt::Alignment align, const QString &text) {
        const double adv = fm.horizontalAdvance(text);
        const double x = (align & Qt::AlignRight) ? rect.right() - adv : (align & Qt::AlignHCenter) ? rect.center().x() - adv * 0.5 : rect.left();
        return x + fm.tightBoundingRect(text).left();
    };
    auto inkRight = [&](const QRectF &rect, Qt::Alignment align, const QString &text) {
        const double adv = fm.horizontalAdvance(text);
        const double x = (align & Qt::AlignRight) ? rect.right() - adv : (align & Qt::AlignHCenter) ? rect.center().x() - adv * 0.5 : rect.left();
        return x + fm.tightBoundingRect(text).right();
    };

    internal::SwitchLayoutInput in;
    in.font = serif;
    in.guard = true;
    in.title = QStringLiteral("EMERGENCY");
    in.on = QStringLiteral("ARMED");
    in.off = QStringLiteral("SAFE");
    in.center = QStringLiteral("AUTO");

    // Horizontal: OFF right-aligned left of the guard, ON left-aligned right of it
    in.orientation = Qt::Horizontal;
    in.positions = 2;
    in.size = QSize(260, 140);
    internal::SwitchLayout L = internal::layoutSwitch(in);
    QVERIFY(L.fits);
    const double offSpaceRight = L.guardOuter.left() - 2.0;
    const double onSpaceLeft = L.guardOuter.right() + 6.0;
    QVERIFY2(inkRight(L.offRect, L.offAlign, in.off) <= offSpaceRight + 1e-6,
             qPrintable(QStringLiteral("OFF ink ends at %1, its space at %2").arg(inkRight(L.offRect, L.offAlign, in.off)).arg(offSpaceRight)));
    QVERIFY2(inkLeft(L.onRect, L.onAlign, in.on) >= onSpaceLeft - 1e-6,
             qPrintable(QStringLiteral("ON ink starts at %1, its space at %2").arg(inkLeft(L.onRect, L.onAlign, in.on)).arg(onSpaceLeft)));

    // Vertical, 3 positions: centre label left-aligned right of the guard
    in.orientation = Qt::Vertical;
    in.positions = 3;
    in.size = QSize(260, 400);
    L = internal::layoutSwitch(in);
    QVERIFY(L.fits);
    const double centerSpaceLeft = L.guardOuter.right() + 4.0;
    QVERIFY2(inkLeft(L.centerRect, L.centerAlign, in.center) >= centerSpaceLeft - 1e-6,
             qPrintable(QStringLiteral("centre ink starts at %1, its space at %2").arg(inkLeft(L.centerRect, L.centerAlign, in.center)).arg(centerSpaceLeft)));
    QVERIFY2(inkRight(L.centerRect, L.centerAlign, in.center) <= L.plate.right() - 2.0 + 1e-6,
             qPrintable(QStringLiteral("centre ink ends at %1, past the plate").arg(inkRight(L.centerRect, L.centerAlign, in.center))));
}

void tst_IndustrialSwitch::widerFontFullCriterionAtMinimumSize_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("orientation");
    QTest::addColumn<int>("positions");
    using T = IndustrialSwitch::SwitchType;
    for (T type : {T::ToggleLever, T::Rocker}) {
        for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
            for (int positions : {2, 3}) {
                QTest::addRow("%s %s %d-pos", type == T::Rocker ? "rocker" : "toggle", o == Qt::Vertical ? "V" : "H", positions)
                    << static_cast<int>(type) << static_cast<int>(o) << positions;
            }
        }
    }
}

// #44: with a wider font whose ink extends past the advance (DejaVu Serif bold), the full criterion
// holds at minimumSizeHint(), with the guard: no label clipped (ink as wide as in a large switch
// with the same font) or outside the plate, and neither the lit LED nor the guard covers a label.
// On platforms without DejaVu Serif, Qt substitutes another font; on the Windows CI (offscreen)
// there is no real font at all (box glyphs), so the wider-font case is exercised on Linux.
void tst_IndustrialSwitch::widerFontFullCriterionAtMinimumSize()
{
    QFETCH(int, type);
    QFETCH(int, orientation);
    QFETCH(int, positions);
    QFont serif = QApplication::font();
    serif.setFamily(QStringLiteral("DejaVu Serif"));
    const SwitchConfig c{static_cast<IndustrialSwitch::SwitchType>(type), static_cast<Qt::Orientation>(orientation), QSize(100, 100), positions};
    const SwitchConfig large{c.type, c.orientation, c.orientation == Qt::Vertical ? QSize(600, 800) : QSize(800, 400), positions};

    auto atMinimum = [&](bool open, const SwitchTexts &t) {
        auto sw = makeSwitch(c, true, open, t);
        sw->setFont(serif);
        sw->resize(sw->minimumSizeHint());
        return sw;
    };

    // No label covered by the LED or the guard
    auto closed = atMinimum(false, {});
    auto open = atMinimum(true, {});
    const std::vector<QPoint> closedText = textInk(*closed);
    const std::vector<QPoint> openText = textInk(*open);
    QVERIFY(!openText.empty());
    QVERIFY2(closedText == openText, "the closed guard changes label pixels at minimumSizeHint()");
    QCOMPARE(commonPixels(guardPixels(*open), openText), 0);
    QCOMPARE(commonPixels(ledPixels(*open), openText), 0);

    // No label clipped or outside the plate
    const SwitchTexts all;
    const QString labels[] = {all.title, all.on, all.off, all.center};
    for (int i = 0; i < (positions == 3 ? 4 : 3); ++i) {
        SwitchTexts one;
        one.title = i == 0 ? labels[0] : QString();
        one.on = i == 1 ? labels[1] : QString();
        one.off = i == 2 ? labels[2] : QString();
        one.center = i == 3 ? labels[3] : QString();
        // the size is that of the switch with all its labels
        auto sw = makeSwitch(c, true, true, one);
        sw->setFont(serif);
        sw->resize(open->size());
        auto reference = makeSwitch(large, true, true, one);
        reference->setFont(serif);
        const QRect ink = boundingBox(textInk(*sw));
        const QRect full = boundingBox(textInk(*reference));
        QVERIFY(!full.isEmpty());
        QVERIFY2(std::abs(ink.width() - full.width()) <= 1,
                 qPrintable(QStringLiteral("label '%1' is %2 device px wide at minimumSizeHint %3x%4, %5 when nothing clips it")
                                .arg(labels[i]).arg(ink.width()).arg(sw->width()).arg(sw->height()).arg(full.width())));
        const qreal dpr = grabImage(*sw).devicePixelRatio();
        const QRect plate = QRect(QPoint(0, 0), sw->size() * dpr).adjusted(qFloor(3 * dpr), qFloor(3 * dpr), -qFloor(3 * dpr), -qFloor(3 * dpr));
        QVERIFY2(plate.contains(ink), qPrintable(QStringLiteral("label '%1' ink outside the plate at minimumSizeHint").arg(labels[i])));
    }
}

void tst_IndustrialSwitch::sizeHintKeepsGuardedThreePositionSwitchReadable_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("orientation");
    using T = IndustrialSwitch::SwitchType;
    for (T type : {T::ToggleLever, T::Rocker}) {
        for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
            QTest::addRow("%s %s", type == T::Rocker ? "rocker" : "toggle", o == Qt::Vertical ? "V" : "H")
                << static_cast<int>(type) << static_cast<int>(o);
        }
    }
}

// #44: at sizeHint() a switch with a guard and 3 positions is laid out without scaling the switch
// down (scale 1: the size it has when nothing needs room) and everything fits in the plate
void tst_IndustrialSwitch::sizeHintKeepsGuardedThreePositionSwitchReadable()
{
    QFETCH(int, type);
    QFETCH(int, orientation);
    const SwitchConfig c{static_cast<IndustrialSwitch::SwitchType>(type), static_cast<Qt::Orientation>(orientation), QSize(100, 100), 3};
    auto sw = makeSwitch(c, true, false);
    const QSize hint = sw->sizeHint();

    internal::SwitchLayoutInput in;
    in.size = hint;
    in.orientation = c.orientation;
    in.positions = 3;
    in.guard = true;
    in.font = sw->font();
    in.title = sw->label();
    in.on = sw->labelOn();
    in.off = sw->labelOff();
    in.center = sw->labelCenter();
    const internal::SwitchLayout layout = internal::layoutSwitch(in);
    QVERIFY2(layout.fits, qPrintable(QStringLiteral("layout does not fit at sizeHint %1x%2").arg(hint.width()).arg(hint.height())));
    QVERIFY2(layout.scale >= 1.0, qPrintable(QStringLiteral("switch scaled to %1 at sizeHint %2x%3")
                                                 .arg(layout.scale).arg(hint.width()).arg(hint.height())));
    const QSize min = sw->minimumSizeHint();
    QVERIFY2(hint.width() >= min.width() && hint.height() >= min.height(),
             qPrintable(QStringLiteral("sizeHint %1x%2 below minimumSizeHint %3x%4").arg(hint.width()).arg(hint.height()).arg(min.width()).arg(min.height())));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_IndustrialSwitch::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialSwitch>();
        if (font) g->setFont(*font);
        g->setLabel(QStringLiteral("PUMP"));
        g->resize(180, 220);
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

QTEST_MAIN(tst_IndustrialSwitch)
#include "tst_IndustrialSwitch.moc"


