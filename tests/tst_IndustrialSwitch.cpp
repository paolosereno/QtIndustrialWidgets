// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
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
    if (c.size.isEmpty()) {
        // "sizeHint" rows: the size hint of the same switch with a guard
        auto guarded = makeSwitch(SwitchConfig{c.type, c.orientation, QSize(100, 100), c.positions}, true, false, t);
        sw->resize(guarded->sizeHint());
    } else {
        sw->resize(c.size);
    }
    return sw;
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
    using T = IndustrialSwitch::SwitchType;
    for (T type : {T::ToggleLever, T::Rocker}) {
        for (int positions : {2, 3}) {
            for (const QSize &size : {QSize(120, 190), QSize(180, 220)}) {
                QTest::addRow("%s V %dx%d %d-pos", type == T::Rocker ? "rocker" : "toggle", size.width(), size.height(), positions)
                    << static_cast<int>(type) << static_cast<int>(Qt::Vertical) << size << positions;
            }
            for (const QSize &size : {QSize(180, 120), QSize(220, 120)}) {
                QTest::addRow("%s H %dx%d %d-pos", type == T::Rocker ? "rocker" : "toggle", size.width(), size.height(), positions)
                    << static_cast<int>(type) << static_cast<int>(Qt::Horizontal) << size << positions;
            }
        }
        // At sizeHint() of the switch with a guard and 3 positions
        for (Qt::Orientation o : {Qt::Vertical, Qt::Horizontal}) {
            QTest::addRow("%s %s sizeHint 3-pos", type == T::Rocker ? "rocker" : "toggle", o == Qt::Vertical ? "V" : "H")
                << static_cast<int>(type) << static_cast<int>(o) << QSize() << 3;
        }
    }
}

SwitchConfig fetchConfig()
{
    QFETCH(int, type);
    QFETCH(int, orientation);
    QFETCH(QSize, size);
    QFETCH(int, positions);
    return {static_cast<IndustrialSwitch::SwitchType>(type), static_cast<Qt::Orientation>(orientation), size, positions};
}

} // namespace

class tst_IndustrialSwitch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void twoPositionToggle();
    void threePositionSwitch();
    void safetyGuardBehavior();
    void switchStylesAndOrientations();
    void signalEmission();
    void keyboardInteraction();
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
}

QTEST_MAIN(tst_IndustrialSwitch)
#include "tst_IndustrialSwitch.moc"


