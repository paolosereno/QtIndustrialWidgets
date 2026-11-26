// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/Compass.h>

#include "FontTestUtils.h"
#include "ReadoutTestUtils.h"
#include "ScreenChangeTestUtils.h"
#include "internal/CompassTestAccess.h"

#include <memory>
#include <QtGui/QMouseEvent>
#include <QtGui/QFontDatabase>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>
#include <limits>

using namespace QtIndustrialWidgets;

// QSKIP(message) without its variadic part: AppleClang warns about an omitted variadic argument before C++20
// (-Wvariadic-macro-arguments-omitted, an error with warnings as errors in CI)
#define COMPASS_SKIP(message)                                                                                          \
    do {                                                                                                               \
        QTest::qSkip(message, __FILE__, __LINE__);                                                                     \
        return;                                                                                                        \
    } while (false)

namespace {

// #226: the wider font of the Compass tests: FontTest::otherFont() (another family, letter spacing 150 %) without its
// italic. Where the family has no italic face Qt synthesizes one, and draws rotated text with the slant in device
// space, not along the glyph (Qt 6.4.2): the rotated labels then leave the ink box that the layout measures, a known
// limitation (docs/widgets.md). A real face keeps the tests on what the layout is meant to measure
QFont compassOtherFont()
{
    QFont f = FontTest::otherFont(QApplication::font());
    f.setItalic(false);
    return f;
}

// Synthetic mouse event with explicit buttons, delivered synchronously. QTest::mouseMove() does not deliver a
// MouseMove event to an offscreen widget with Qt 6.2 (#79), so drags are sent as explicit events.
void sendMouse(QWidget &w, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, w.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&w, &e);
}


constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QImage renderCompass(Compass &c)
{
    return c.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// #151: a compass whose parts can be told apart in a render: ticks and labels pure green (textColor and
// cardinalColor), the north half of the needle and the "N" label pure blue (needleColor), the south half pure
// magenta (needleTailColor), the lubber line pure yellow. No heading bug, no digital readout. 300 x 300: the
// dial radius is 144 px (side / 2 - 6)
void prepareForDial(Compass &c, Compass::DisplayMode mode, double heading)
{
    c.setDisplayMode(mode);
    c.setDialColor(QColor(20, 24, 32));
    c.setBezelColor(QColor(48, 56, 70));
    c.setTextColor(QColor(0, 255, 0));
    c.setCardinalColor(QColor(0, 255, 0));
    c.setNeedleColor(QColor(0, 0, 255));
    c.setNeedleTailColor(QColor(255, 0, 255));
    c.setLubberColor(QColor(255, 255, 0));
    c.setHeadingBugVisible(false);
    c.setDigitalReadoutVisible(false);
    c.resize(300, 300);
    c.setHeading(heading);
}

bool isGreen(QRgb p) { return qGreen(p) > 150 && qGreen(p) - qRed(p) > 80 && qGreen(p) - qBlue(p) > 80; }
// The south half of the needle: magenta, red and blue alike (pure, darker or antialiased on the dial). Text
// antialiasing can fringe the red flag with pixels like (226, 80, 162): those are not the needle
bool isTail(QRgb p) { return qRed(p) > 150 && qBlue(p) > 150 && std::abs(qRed(p) - qBlue(p)) < 40 && qGreen(p) < 60; }
bool isLubber(QRgb p) { return qRed(p) > 180 && qGreen(p) > 180 && qBlue(p) < 90; }
// The needle and the "N" label at full strength; bluish() also takes them dimmed
bool isBlue(QRgb p) { return qBlue(p) > 150 && qBlue(p) - qRed(p) > 80 && qBlue(p) - qGreen(p) > 80; }
bool isBluish(QRgb p) { return qBlue(p) > 60 && qBlue(p) - qRed(p) > 30 && qBlue(p) - qGreen(p) > 30; }
// The HDG failure flag: a saturated red, a colour that prepareForDial() gives to no other part
bool isFlagRed(QRgb p) { return qRed(p) > 180 && qGreen(p) < 90 && qBlue(p) < 100; }

struct Dial {
    QPointF centre; // device px
    double radius;  // device px
};

// As in Compass.cpp: centre QPointF(rect().center()), radius min(w, h) / 2 - 6 (logical px), scaled to device px
Dial dialOf(const Compass &c, const QImage &img)
{
    const qreal dpr = img.devicePixelRatio();
    return {QPointF(c.rect().center()) * dpr, (std::min(c.width(), c.height()) / 2.0 - 6.0) * dpr};
}

// Pixels matching \a pred at a distance from the centre within [r0, r1] x radius
template <typename Pred>
QVector<QPoint> ringPixels(const QImage &img, const Dial &d, double r0, double r1, Pred pred)
{
    QVector<QPoint> out;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const double r = std::hypot(x + 0.5 - d.centre.x(), y + 0.5 - d.centre.y()) / d.radius;
            if (r >= r0 && r <= r1 && pred(img.pixel(x, y))) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

// Rows holding a run of flag-red pixels at least \a minRun px long: a solid flag gives many such rows; the
// strokes of a text or a needle along a radius do not
int flagRows(const QImage &img, int minRun)
{
    int rows = 0;
    for (int y = 0; y < img.height(); ++y) {
        int run = 0;
        int longest = 0;
        for (int x = 0; x < img.width(); ++x) {
            run = isFlagRed(img.pixel(x, y)) ? run + 1 : 0;
            longest = std::max(longest, run);
        }
        if (longest >= minRun) {
            ++rows;
        }
    }
    return rows;
}

// Empty when \a img shows a flag: at least 0.08 x radius rows with a red run of at least 0.25 x radius
QString flagError(const QImage &img, const Dial &d)
{
    const int rows = flagRows(img, static_cast<int>(std::ceil(d.radius * 0.25)));
    const int needed = static_cast<int>(std::ceil(d.radius * 0.08));
    return rows >= needed ? QString() : QStringLiteral("%1 flag rows, %2 needed").arg(rows).arg(needed);
}

// #226: the geometry of the card as Compass.cpp draws it, in device px. In HeadingUp the card pixmap (centre at
// width / 2, height / 2) is drawn at rect().center() - (width() / 2, height() / 2), integer division; the pod and the
// flag are drawn around rect().center()
struct Card {
    QPointF centre;    // of the card
    QPointF podCentre; // of the pod and the flag
    double radius;     // side / 2 - 6
    double textR;      // radius - 3 - 0.22 radius: the centre of the ring labels
    double majorTickR; // radius - 3 - 0.12 radius: the inner end of the major ticks, a circle
    double dpr;
};

Card cardOf(const Compass &c, qreal dpr)
{
    const double r = std::min(c.width(), c.height()) / 2.0 - 6.0;
    const QPointF origin = QPointF(c.rect().center()) - QPointF(c.width() / 2, c.height() / 2);
    return {(origin + QPointF(c.width() / 2.0, c.height() / 2.0)) * dpr, QPointF(c.rect().center()) * dpr,
            r * dpr, (r - 3.0 - r * 0.22) * dpr, (r - 3.0 - r * 0.12) * dpr, dpr};
}

// The ring labels: N, NE, E, … at their angle (degrees clockwise from 12 o'clock), and the numbers 030 … 330
const std::vector<int> &ringLabelAngles()
{
    static const std::vector<int> angles{0, 30, 45, 60, 90, 120, 135, 150, 180, 210, 225, 240, 270, 300, 315, 330};
    return angles;
}

// The ink of the card: the pixels that change when textColor, cardinalColor and needleColor (the "N") change. A
// HeadingUp compass at heading 0 (card unrotated), with no lubber line, heading bug or readout
QVector<QPoint> cardInk(Compass &c)
{
    const QSize size = c.size(); // prepareForDial() resizes to 300 x 300
    prepareForDial(c, Compass::DisplayMode::HeadingUp, 0.0);
    c.resize(size);
    c.setLubberLineVisible(false);
    const QImage green = renderCompass(c);
    c.setTextColor(QColor(255, 0, 0));
    c.setCardinalColor(QColor(255, 0, 0));
    c.setNeedleColor(QColor(255, 0, 0));
    const QImage red = renderCompass(c);
    QVector<QPoint> ink;
    for (int y = 0; y < green.height(); ++y) {
        for (int x = 0; x < green.width(); ++x) {
            if (green.pixel(x, y) != red.pixel(x, y)) ink.push_back(QPoint(x, y));
        }
    }
    return ink;
}

// Nearest and farthest distance from \a centre of the device pixel \a p (a square)
std::pair<double, double> pixelDistances(const QPoint &p, const QPointF &centre)
{
    const double nx = std::clamp(centre.x(), double(p.x()), p.x() + 1.0) - centre.x();
    const double ny = std::clamp(centre.y(), double(p.y()), p.y() + 1.0) - centre.y();
    double far = 0.0;
    for (int dx = 0; dx <= 1; ++dx) {
        for (int dy = 0; dy <= 1; ++dy) far = std::max(far, std::hypot(p.x() + dx - centre.x(), p.y() + dy - centre.y()));
    }
    return {std::hypot(nx, ny), far};
}

// Smallest and largest angle (degrees clockwise from 12 o'clock, relative to \a reference, in (-180, 180]) of the
// corners of the device pixel \a p
std::pair<double, double> pixelAngles(const QPoint &p, const QPointF &centre, double reference)
{
    // Relative to the angle of the pixel centre first, so that a pixel across +-180 is not taken for the whole circle
    const auto angleOf = [&](double x, double y) { return std::atan2(x - centre.x(), centre.y() - y) * 180.0 / M_PI; };
    const double middle = angleOf(p.x() + 0.5, p.y() + 0.5);
    double lo = 1e9;
    double hi = -1e9;
    for (int dx = 0; dx <= 1; ++dx) {
        for (int dy = 0; dy <= 1; ++dy) {
            const double rel = std::remainder(angleOf(p.x() + dx, p.y() + dy) - middle, 360.0);
            lo = std::min(lo, rel);
            hi = std::max(hi, rel);
        }
    }
    const double shift = std::remainder(middle - reference, 360.0);
    return {shift + lo, shift + hi};
}

// The first point of (from, to) that no interval covers, or NaN
double firstUncovered(std::vector<std::pair<double, double>> intervals, double from, double to)
{
    std::sort(intervals.begin(), intervals.end());
    double reach = from;
    for (const auto &[lo, hi] : intervals) {
        if (hi <= reach) continue;
        if (lo > reach && reach < to) return reach + std::min(lo - reach, to - reach) * 0.5;
        reach = std::max(reach, hi);
        if (reach >= to) return std::numeric_limits<double>::quiet_NaN();
    }
    return reach < to ? (reach + to) * 0.5 : std::numeric_limits<double>::quiet_NaN();
}

// #226: the ring labels are clear of each other and of the major tick circle, in the pixels. Empty, or the failure
QString ringLabelsError(Compass &c)
{
    const QVector<QPoint> ink = cardInk(c);
    const Card card = cardOf(c, c.devicePixelRatioF());
    // A circle with no ink between the centre of the labels and the tick circle
    std::vector<std::pair<double, double>> radii;
    for (const QPoint &p : ink) radii.push_back(pixelDistances(p, card.centre));
    const double gap = firstUncovered(radii, card.textR, card.majorTickR);
    if (!std::isfinite(gap)) {
        return QStringLiteral("no circle without ink between the labels (radius %1 device px) and the tick circle (%2)")
            .arg(card.textR, 0, 'f', 1).arg(card.majorTickR, 0, 'f', 1);
    }
    // Between two neighbouring labels, a ray with no ink in the ring of the labels (from 0.45 radius to that circle)
    const std::vector<int> &angles = ringLabelAngles();
    for (size_t i = 0; i < angles.size(); ++i) {
        const double a = angles[i];
        const double b = i + 1 < angles.size() ? angles[i + 1] : 360.0;
        const double mid = (a + b) * 0.5;
        std::vector<std::pair<double, double>> spans;
        for (const QPoint &p : ink) {
            const auto [near, far] = pixelDistances(p, card.centre);
            if (far < card.radius * 0.45 || near > gap) continue;
            spans.push_back(pixelAngles(p, card.centre, mid));
        }
        if (!std::isfinite(firstUncovered(spans, a - mid, b - mid))) {
            return QStringLiteral("the labels at %1 and %2 degrees touch: no ray without ink between them").arg(a).arg(b);
        }
    }
    return QString();
}

// The pod as Compass.cpp draws it, in device px, inside its 1.5 px border
QRectF podInterior(const Compass &c, const Card &card)
{
    const double r = card.radius / card.dpr;
    const double podW = std::clamp(r * 0.52, 48.0, 110.0);
    const double podH = std::clamp(r * 0.26, 24.0, 50.0);
    const QRectF pod(card.podCentre / card.dpr - QPointF(podW / 2.0, podH / 2.0), QSizeF(podW, podH));
    Q_UNUSED(c);
    return QRectF(pod.topLeft() * card.dpr, pod.size() * card.dpr).adjusted(0.75 * card.dpr, 0.75 * card.dpr,
                                                                             -0.75 * card.dpr, -0.75 * card.dpr);
}

// The heading text of the readout that is widest in the pod font, of the 360 headings
double widestHeading(const Compass &c, const Card &card)
{
    const double podH = std::clamp(card.radius / card.dpr * 0.26, 24.0, 50.0);
    QFont f = c.font();
    f.setBold(true);
    f.setPointSizeF(std::clamp(podH * 0.48, 8.0, 20.0));
    const QFontMetricsF fm(f, const_cast<Compass *>(&c));
    int widest = 0;
    double width = -1.0;
    for (int h = 0; h < 360; ++h) {
        const double w = fm.horizontalAdvance(QStringLiteral("%1°").arg(h, 3, 10, QLatin1Char('0')));
        if (w > width) {
            width = w;
            widest = h;
        }
    }
    return widest;
}

// The readout ink (readoutColor) at \a heading: the pixels that change from white to red with readoutColor. Drawn in
// readoutColor at any coverage, such a pixel keeps its red channel (255 in both colours) and changes green or blue;
// a pixel of anything else that differs between the two renders changes its red channel too (where no font is
// installed, Qt can leave a box glyph of the card out of the first render of a widget, #226)
QRect readoutInk(Compass &c, double heading)
{
    const QSize size = c.size(); // prepareForDial() resizes to 300 x 300
    prepareForDial(c, Compass::DisplayMode::HeadingUp, heading);
    c.resize(size);
    c.setLubberLineVisible(false);
    c.setDigitalReadoutVisible(true);
    c.setReadoutColor(QColor(255, 255, 255));
    const QImage white = renderCompass(c);
    c.setReadoutColor(QColor(255, 0, 0));
    const QImage red = renderCompass(c);
    QRect box;
    for (int y = 0; y < white.height(); ++y) {
        for (int x = 0; x < white.width(); ++x) {
            const QRgb w = white.pixel(x, y);
            const QRgb r = red.pixel(x, y);
            if (w != r && qRed(w) == qRed(r)) box |= QRect(x, y, 1, 1);
        }
    }
    return box;
}

// #226: the grid of sides of the minimum-size tests, as offsets from minimumSizeHint(): every side just above the
// minimum, then wider steps up to the sizes where the card fonts stop growing
const std::vector<int> &sideGrid()
{
    static const std::vector<int> offsets{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 13, 17, 21, 26, 34, 42, 55, 70, 89, 110, 144, 180, 233, 300};
    return offsets;
}

// WCAG 2 contrast ratio of two opaque colours
double contrastRatio(const QColor &a, const QColor &b)
{
    const auto luminance = [](const QColor &c) {
        const auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
    };
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The device pixel (x, y) as a square in widget coordinates
QRectF pixelSquare(int x, int y, double dpr)
{
    return QRectF(x / dpr, y / dpr, 1.0 / dpr, 1.0 / dpr);
}

// #226: "HDG" alone on the colour of the flag, painted as Compass paints it
class FlagTextReference : public QWidget
{
public:
    explicit FlagTextReference(const CompassFlag &flag) : m_flag(flag) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.fillRect(rect(), m_flag.fill);
        p.setFont(m_flag.textFont);
        p.setPen(m_flag.text);
        p.drawText(m_flag.textOrigin, QStringLiteral("HDG"));
    }

private:
    CompassFlag m_flag;
};

QImage renderWidget(QWidget &w)
{
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// #226: whether the label glyphs of \a font are drawn as glyphs. Where no font is installed (or the font lacks them)
// Qt draws each missing glyph as a box, and draws those boxes unrotated in a rotated frame: there the rotated labels
// are checked on the layout geometry only (CompassTestAccess), not on their pixels
bool labelGlyphsDrawn(const QFont &font)
{
    const QFontMetricsF fm(font);
    const QString chars = QStringLiteral("NESW0123456789");
    return std::all_of(chars.cbegin(), chars.cend(), [&](QChar ch) { return fm.inFont(ch); });
}

// #226: the fonts of the minimum-size tests
void addMinimumRows()
{
    QTest::addColumn<QString>("fontName");
    QTest::addColumn<int>("offset");
    for (const char *font : {"application", "test font", "other font"}) {
        for (int offset : sideGrid()) {
            QTest::newRow(qPrintable(QStringLiteral("%1, minimum + %2").arg(QLatin1String(font)).arg(offset)))
                << QString::fromLatin1(font) << offset;
        }
    }
}

// The compass of a minimum-size row: the font set, resized to its minimumSizeHint() + \a offset (square)
bool setUpMinimumRow(Compass &c, const QString &fontName, int offset, QString *error)
{
    if (fontName == QLatin1String("test font")) {
        const QFont f = ReadoutTest::readoutTestFont(error);
        if (!error->isEmpty()) return false;
        c.setFont(f);
    } else if (fontName == QLatin1String("other font")) {
        c.setFont(compassOtherFont());
    }
    const QSize minimum = c.minimumSizeHint();
    const int side = std::max(minimum.width(), minimum.height()) + offset;
    c.resize(side, side);
    return true;
}
} // namespace

class tst_Compass : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void textSizeIgnoresWidgetFontSize();
    void initTestCase();
    void init();
    void defaultValues();
    void headingNormalization();
    void targetHeadingAndBug();
    void courseDeviationCalculation();
    void displayModeSwitch();
    void colorSetters();
    void mouseInteractionBug();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
    void cleanupTestCase();
    void invalidHeadingIsNaN();
    void northUpInvalidHidesNeedleAndShowsFlag();
    void headingUpInvalidShowsDimmedCardAtNorthWithoutLubber();
    void invalidReadoutShowsDashes_data();
    void invalidReadoutShowsDashes();
    void bugDragOnInvalidHeadingUpUsesTheCardAtNorth();
    void flagVisibleAtMinimumSize_data();
    void flagVisibleAtMinimumSize();
    void valueStateOnlyValidOrInvalid();
    void valueStateSignalOrder();
    void headingUpInvalidDimsTheBug();
    void northUpLubberAtTwelveOClock_data();
    void northUpLubberAtTwelveOClock();
    void minimumSizeIsNotFixed();
    void ringLabelsClearFromMinimumUp_data();
    void ringLabelsClearFromMinimumUp();
    void readoutFitsThePodFromMinimumUp_data();
    void readoutFitsThePodFromMinimumUp();
    void flagClearOfRingAndPodFromMinimumUp_data();
    void flagClearOfRingAndPodFromMinimumUp();
    void ringLabelsClearOnTheLayout_data();
    void ringLabelsClearOnTheLayout();
    void layoutThinsInOrder_data();
    void layoutThinsInOrder();
    void labelsDrawnAsLaidOut_data();
    void labelsDrawnAsLaidOut();
    void labelsDrawnAtTheWidgetDpi_data();
    void labelsDrawnAtTheWidgetDpi();
    void minimumFollowsTheFont();
    void minimumFollowsTheDpiBeforeARepaint_data();
    void minimumFollowsTheDpiBeforeARepaint();
    void minimumFollowsTheScreen_data();
    void minimumFollowsTheScreen();
};

void tst_Compass::initTestCase()
{
}

// #226: each test (and each row) starts without the application fonts of the earlier ones. Where no font is installed
// the test font would otherwise stand for every family, and it has no letters: N, E, S, W would not be drawn
void tst_Compass::init()
{
    QFontDatabase::removeAllApplicationFonts();
}

void tst_Compass::cleanupTestCase()
{
}

void tst_Compass::defaultValues()
{
    Compass compass;
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(compass.targetHeading(), 0.0);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);
    QVERIFY(compass.isHeadingBugVisible());
    QVERIFY(compass.isHeadingBugInteractive());
    QVERIFY(compass.isLubberLineVisible());
    QVERIFY(compass.isDigitalReadoutVisible());
    QCOMPARE(compass.courseDeviation(), 0.0);
    QVERIFY(compass.minimumSizeHint().width() > 0);
    QVERIFY(compass.sizeHint().width() >= compass.minimumSizeHint().width());
}

void tst_Compass::headingNormalization()
{
    Compass compass;
    QSignalSpy spy(&compass, &Compass::headingChanged);

    // Standard heading within [0, 360)
    compass.setHeading(45.0);
    QCOMPARE(compass.heading(), 45.0);
    QCOMPARE(spy.count(), 1);

    // 360.0 wraps to 0.0
    compass.setHeading(360.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2);

    // Multiple revolutions
    compass.setHeading(720.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2); // No change from 0.0, no new emission

    compass.setHeading(450.0);
    QCOMPARE(compass.heading(), 90.0);
    QCOMPARE(spy.count(), 3);

    // Negative angle wraps properly
    compass.setHeading(-90.0);
    QCOMPARE(compass.heading(), 270.0);
    QCOMPARE(spy.count(), 4);

    compass.setHeading(-370.0);
    QCOMPARE(compass.heading(), 350.0);
    QCOMPARE(spy.count(), 5);

    // Static helper
    QCOMPARE(Compass::normalizeDegrees(0.0), 0.0);
    QCOMPARE(Compass::normalizeDegrees(360.0), 0.0);
    QCOMPARE(Compass::normalizeDegrees(-45.0), 315.0);
}

void tst_Compass::targetHeadingAndBug()
{
    Compass compass;
    QSignalSpy spyTarget(&compass, &Compass::targetHeadingChanged);
    QSignalSpy spyApp(&compass, &Compass::appearanceChanged);

    compass.setTargetHeading(120.0);
    QCOMPARE(compass.targetHeading(), 120.0);
    QCOMPARE(spyTarget.count(), 1);

    compass.setTargetHeading(-45.0);
    QCOMPARE(compass.targetHeading(), 315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Setting same value does not emit
    compass.setTargetHeading(315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Bug visibility and interaction
    compass.setHeadingBugVisible(false);
    QVERIFY(!compass.isHeadingBugVisible());
    QVERIFY(spyApp.count() >= 1);

    compass.setHeadingBugInteractive(false);
    QVERIFY(!compass.isHeadingBugInteractive());

    compass.setLubberLineVisible(false);
    QVERIFY(!compass.isLubberLineVisible());

    compass.setDigitalReadoutVisible(false);
    QVERIFY(!compass.isDigitalReadoutVisible());
}

void tst_Compass::courseDeviationCalculation()
{
    Compass compass;

    // On course
    compass.setHeading(45.0);
    compass.setTargetHeading(45.0);
    QCOMPARE(compass.courseDeviation(), 0.0);

    // 10 degrees to starboard (+10°)
    compass.setHeading(50.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), 10.0);

    // 10 degrees to port (-10°)
    compass.setHeading(30.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), -10.0);

    // Crossing 0° / 360° north meridian
    compass.setHeading(10.0);
    compass.setTargetHeading(350.0);
    QCOMPARE(compass.courseDeviation(), 20.0);

    compass.setHeading(350.0);
    compass.setTargetHeading(10.0);
    QCOMPARE(compass.courseDeviation(), -20.0);
}

void tst_Compass::displayModeSwitch()
{
    Compass compass;
    QSignalSpy spyMode(&compass, &Compass::displayModeChanged);

    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    // Re-setting same mode does not emit duplicate
    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    compass.setDisplayMode(Compass::DisplayMode::HeadingUp);
    QCOMPARE(compass.displayMode(), Compass::DisplayMode::HeadingUp);
    QCOMPARE(spyMode.count(), 2);
}

void tst_Compass::colorSetters()
{
    Compass compass;
    QSignalSpy spy(&compass, &Compass::appearanceChanged);

    compass.setDialColor(QColor(10, 10, 10));
    QCOMPARE(compass.dialColor(), QColor(10, 10, 10));

    compass.setBezelColor(QColor(40, 50, 60));
    QCOMPARE(compass.bezelColor(), QColor(40, 50, 60));

    compass.setTextColor(QColor(255, 255, 255));
    QCOMPARE(compass.textColor(), QColor(255, 255, 255));

    compass.setCardinalColor(QColor(0, 200, 255));
    QCOMPARE(compass.cardinalColor(), QColor(0, 200, 255));

    compass.setNeedleColor(QColor(255, 0, 0));
    QCOMPARE(compass.needleColor(), QColor(255, 0, 0));

    compass.setNeedleTailColor(QColor(200, 200, 200));
    QCOMPARE(compass.needleTailColor(), QColor(200, 200, 200));

    compass.setBugColor(QColor(255, 128, 0));
    QCOMPARE(compass.bugColor(), QColor(255, 128, 0));

    compass.setLubberColor(QColor(255, 255, 0));
    QCOMPARE(compass.lubberColor(), QColor(255, 255, 0));

    QVERIFY(spy.count() >= 8);
}

void tst_Compass::mouseInteractionBug()
{
    Compass compass;
    compass.resize(300, 300);
    compass.show();
    QVERIFY(QTest::qWaitForWindowExposed(&compass));

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    compass.setHeadingBugInteractive(true);

    // Center is (150, 150).
    // Click at 3 o'clock: (250, 150) -> angle = 90° (East)
    QPoint eastPoint(250, 150);
    QTest::mousePress(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);
    QTest::mouseRelease(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);

    // Allow slight float tolerance around 90°
    QVERIFY(std::abs(compass.targetHeading() - 90.0) < 3.0);

    // Drag to 6 o'clock: (150, 250) -> angle = 180° (South)
    QPoint southPoint(150, 250);
    sendMouse(compass, QEvent::MouseButtonPress, eastPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(compass, QEvent::MouseMove, southPoint, Qt::NoButton, Qt::LeftButton);
    sendMouse(compass, QEvent::MouseButtonRelease, southPoint, Qt::LeftButton, Qt::NoButton);

    QVERIFY(std::abs(compass.targetHeading() - 180.0) < 3.0);
}

void tst_Compass::renderOffscreen()
{
    Compass compass;
    compass.resize(250, 250);
    compass.setHeading(45.0);
    compass.setTargetHeading(90.0);

    // Render HeadingUp mode
    compass.setDisplayMode(Compass::DisplayMode::HeadingUp);
    QPixmap pixmap1(250, 250);
    pixmap1.fill(Qt::transparent);
    compass.render(&pixmap1);
    QVERIFY(!pixmap1.isNull());

    // Render NorthUp mode
    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QPixmap pixmap2(250, 250);
    pixmap2.fill(Qt::transparent);
    compass.render(&pixmap2);
    QVERIFY(!pixmap2.isNull());
}

void tst_Compass::extremeResizeNoCrash()
{
    Compass compass;

    const QList<QSize> extremeSizes = {
        QSize(0, 0),
        QSize(1, 1),
        QSize(5, 5),
        QSize(10, 10),
        QSize(20, 20),
        QSize(100, 100),
        QSize(800, 800),
        QSize(1920, 1080),
        QSize(3840, 2160)
    };

    for (const auto &sz : extremeSizes) {
        compass.resize(sz);
        if (sz.width() > 0 && sz.height() > 0) {
            QPixmap pixmap(sz);
            compass.render(&pixmap);
        }
    }
}

void tst_Compass::nanAndInfinityResilience()
{
    Compass compass;
    compass.setHeading(90.0);
    QCOMPARE(compass.heading(), 90.0);

    // #151: NaN and ±Inf have no direction: the heading is invalid (NaN), never the last value or 0°
    compass.setHeading(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(std::isnan(compass.heading()));
    compass.setHeading(90.0);
    compass.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY(std::isnan(compass.heading()));
    compass.setHeading(90.0);
    compass.setHeading(-std::numeric_limits<double>::infinity());
    QVERIFY(std::isnan(compass.heading()));

    // Target heading NaN/Inf ignored
    compass.setTargetHeading(45.0);
    compass.setTargetHeading(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(compass.targetHeading(), 45.0);
    compass.setTargetHeading(std::numeric_limits<double>::infinity());
    QCOMPARE(compass.targetHeading(), 45.0);

    // normalizeDegrees static helper test
    QCOMPARE(Compass::normalizeDegrees(std::numeric_limits<double>::quiet_NaN()), 0.0);
    QCOMPARE(Compass::normalizeDegrees(std::numeric_limits<double>::infinity()), 0.0);

    // Offscreen render in both display modes
    compass.resize(200, 200);
    QPixmap pix(compass.size());
    compass.render(&pix);
    QVERIFY(!pix.isNull());

    compass.setDisplayMode(Compass::DisplayMode::NorthUp);
    QPixmap pixNorth(compass.size());
    compass.render(&pixNorth);
    QVERIFY(!pixNorth.isNull());
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_Compass::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<Compass>();
        if (font) g->setFont(*font);
        g->setDigitalReadoutVisible(false);
        g->resize(300, 300);
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

// #151: NaN and ±Inf have no direction: heading() and courseDeviation() are NaN, headingChanged(NaN) is emitted
// once (a repeated invalid input emits nothing), and the next finite heading is emitted
void tst_Compass::invalidHeadingIsNaN()
{
    Compass c;
    c.setHeading(90.0);
    c.setTargetHeading(45.0);
    QSignalSpy spy(&c, &Compass::headingChanged);

    c.setHeading(kNaN);
    QVERIFY2(std::isnan(c.heading()), qPrintable(QStringLiteral("heading() = %1").arg(c.heading())));
    QVERIFY2(std::isnan(c.courseDeviation()), qPrintable(QStringLiteral("courseDeviation() = %1").arg(c.courseDeviation())));
    QCOMPARE(spy.count(), 1);
    QVERIFY(std::isnan(spy.at(0).at(0).toDouble()));

    c.setHeading(kNaN);
    c.setHeading(std::numeric_limits<double>::infinity());
    c.setHeading(-std::numeric_limits<double>::infinity());
    QCOMPARE(spy.count(), 1);
    QVERIFY(std::isnan(c.heading()));

    c.setHeading(30.0);
    QCOMPARE(c.heading(), 30.0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toDouble(), 30.0);

    // +Inf is invalid, not 0° (North)
    c.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY2(std::isnan(c.heading()), qPrintable(QStringLiteral("heading() = %1").arg(c.heading())));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(c.targetHeading(), 45.0);
}

// #151: North-up, an invalid heading draws no needle and shows the HDG failure flag
void tst_Compass::northUpInvalidHidesNeedleAndShowsFlag()
{
    Compass c;
    prepareForDial(c, Compass::DisplayMode::NorthUp, 90.0);
    const QImage valid = renderCompass(c);
    const Dial d = dialOf(c, valid);
    QVERIFY(!ringPixels(valid, d, 0.0, 1.0, isTail).isEmpty());
    QVERIFY(!ringPixels(valid, d, 0.15, 0.6, isBlue).isEmpty());
    QVERIFY2(!flagError(valid, d).isEmpty(), "a valid heading shows the flag");

    c.setHeading(kNaN);
    const QImage invalid = renderCompass(c);
    QCOMPARE(ringPixels(invalid, d, 0.0, 1.0, isTail).size(), 0);
    QCOMPARE(ringPixels(invalid, d, 0.15, 0.6, isBlue).size(), 0);
    const QString flag = flagError(invalid, d);
    QVERIFY2(flag.isEmpty(), qPrintable(flag));
}

// #151: heading-up, an invalid heading draws the card at 0° with dimmed colours and no lubber line, and shows
// the HDG failure flag, so that the card is not read as a heading of 000
void tst_Compass::headingUpInvalidShowsDimmedCardAtNorthWithoutLubber()
{
    Compass reference;
    prepareForDial(reference, Compass::DisplayMode::HeadingUp, 0.0);
    const QImage north = renderCompass(reference);
    const Dial d = dialOf(reference, north);
    const int brightLabels = ringPixels(north, d, 0.6, 1.0, isGreen).size();
    QVERIFY(brightLabels > 0);

    Compass c;
    prepareForDial(c, Compass::DisplayMode::HeadingUp, 47.0);
    QVERIFY(!ringPixels(renderCompass(c), d, 0.0, 1.2, isLubber).isEmpty());
    c.setHeading(kNaN);
    const QImage invalid = renderCompass(c);

    // The "N" label (needle colour, dimmed) at the top of the dial: the card is drawn at 0°
    const QVector<QPoint> n = ringPixels(invalid, d, 0.6, 1.0, isBluish);
    QVERIFY(!n.isEmpty());
    QPointF sum;
    for (const QPoint &p : n) {
        sum += QPointF(p) + QPointF(0.5, 0.5);
    }
    const QPointF nCentre = sum / n.size();
    QVERIFY2(std::abs(nCentre.x() - d.centre.x()) < 0.06 * d.radius && nCentre.y() < d.centre.y() - 0.5 * d.radius,
             qPrintable(QStringLiteral("N at (%1, %2), centre (%3, %4)")
                            .arg(nCentre.x()).arg(nCentre.y()).arg(d.centre.x()).arg(d.centre.y())));

    // Dimmed: the ticks and labels lose their full colour
    const int dimLabels = ringPixels(invalid, d, 0.6, 1.0, isGreen).size();
    QVERIFY2(dimLabels < brightLabels / 10, qPrintable(QStringLiteral("%1 full-colour label pixels, %2 on a valid card")
                                                         .arg(dimLabels).arg(brightLabels)));
    QCOMPARE(ringPixels(invalid, d, 0.0, 1.2, isLubber).size(), 0);
    const QString flag = flagError(invalid, d);
    QVERIFY2(flag.isEmpty(), qPrintable(flag));
}

// #151: an invalid heading reads "---" (then the degree sign), not the last heading. The test font has no
// glyph for "°": only the first three ink groups are checked
void tst_Compass::invalidReadoutShowsDashes_data()
{
    QTest::addColumn<Compass::DisplayMode>("mode");
    QTest::newRow("heading-up") << Compass::DisplayMode::HeadingUp;
    QTest::newRow("north-up") << Compass::DisplayMode::NorthUp;
}

void tst_Compass::invalidReadoutShowsDashes()
{
    QFETCH(Compass::DisplayMode, mode);
    QString error;
    const QFont font = ReadoutTest::readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    Compass c;
    c.setFont(font);
    c.setDisplayMode(mode);
    c.setHeadingBugVisible(false);
    c.resize(300, 300);
    c.setHeading(90.0);

    // The display: where the render changes when it is hidden
    c.setDigitalReadoutVisible(false);
    const QImage without = renderCompass(c);
    c.setDigitalReadoutVisible(true);
    const QImage with = renderCompass(c);
    QRect box;
    for (int y = 0; y < with.height(); ++y) {
        for (int x = 0; x < with.width(); ++x) {
            if (with.pixel(x, y) != without.pixel(x, y)) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    QVERIFY(!box.isEmpty());
    box.adjust(3, 3, -3, -3);

    c.setHeading(kNaN);
    const QVector<ReadoutTest::InkGroup> groups = ReadoutTest::readoutInkGroups(renderCompass(c), box);
    QVERIFY2(groups.size() >= 3, qPrintable(QStringLiteral("%1 ink groups").arg(groups.size())));
    const QString dashes = ReadoutTest::dashesError(groups.mid(0, 3));
    QVERIFY2(dashes.isEmpty(), qPrintable(dashes));
}

// #151: heading-up with an invalid heading, the card is at 0°: a click on the dial sets the target heading
// read on that card (3 o'clock is 090°)
void tst_Compass::bugDragOnInvalidHeadingUpUsesTheCardAtNorth()
{
    Compass c;
    c.setDisplayMode(Compass::DisplayMode::HeadingUp);
    c.resize(300, 300);
    c.setHeading(47.0);
    c.setHeading(kNaN);
    const QPointF centre(c.rect().center());
    const double radius = 300 / 2.0 - 6.0;
    const QPointF east = centre + QPointF(radius * 0.8, 0.0);
    sendMouse(c, QEvent::MouseButtonPress, east, Qt::LeftButton, Qt::LeftButton);
    sendMouse(c, QEvent::MouseButtonRelease, east, Qt::LeftButton, Qt::NoButton);
    QVERIFY2(std::abs(c.targetHeading() - 90.0) < 1.0, qPrintable(QStringLiteral("targetHeading() = %1").arg(c.targetHeading())));
}

// #151, #226: the HDG flag stays a solid, readable flag from minimumSizeHint() up, on the geometry the widget lays out
// (test hook): (a) the flag is inside the widget and nothing is drawn over it; (b) every pixel wholly inside the flag
// (inside its border and rounded corners) and outside the ink box of "HDG" has the colour of the flag; (c) "HDG" is
// at least 7 pt, with a contrast of at least 4.5:1 on the flag; (d) the flag is at least 12 logical px high
void tst_Compass::flagVisibleAtMinimumSize_data()
{
    QTest::addColumn<Compass::DisplayMode>("mode");
    QTest::addColumn<QString>("fontName");
    QTest::addColumn<int>("offset");
    for (const auto &[name, mode] : {std::pair<const char *, Compass::DisplayMode>{"heading-up", Compass::DisplayMode::HeadingUp},
                                     std::pair<const char *, Compass::DisplayMode>{"north-up", Compass::DisplayMode::NorthUp}}) {
        for (const char *font : {"application", "other font"}) {
            for (int offset : sideGrid()) {
                QTest::newRow(qPrintable(QStringLiteral("%1, %2, minimum + %3").arg(QLatin1String(name), QLatin1String(font)).arg(offset)))
                    << mode << QString::fromLatin1(font) << offset;
            }
        }
    }
}

void tst_Compass::flagVisibleAtMinimumSize()
{
    QFETCH(Compass::DisplayMode, mode);
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    const QSize size = c.size(); // prepareForDial() resizes to 300 x 300
    prepareForDial(c, mode, 0.0);
    c.resize(size);
    c.setDigitalReadoutVisible(true);
    c.setHeading(kNaN);
    const QImage img = renderCompass(c);
    const CompassFlag flag = internal::CompassTestAccess::flag(c);
    const double dpr = c.devicePixelRatioF();
    const QString where = QStringLiteral("%1x%2, flag (%3, %4) %5x%6: ")
                              .arg(c.width()).arg(c.height()).arg(flag.rect.x()).arg(flag.rect.y())
                              .arg(flag.rect.width()).arg(flag.rect.height());

    // (d)
    QVERIFY2(flag.rect.height() >= 12.0, qPrintable(where + QStringLiteral("lower than 12 px")));

    // (a) Inside the widget with its border, and the pod (the only part drawn after the flag) leaves it as it is
    const QRectF outer = flag.rect.adjusted(-0.5, -0.5, 0.5, 0.5);
    QVERIFY2(QRectF(c.rect()).contains(outer), qPrintable(where + QStringLiteral("not inside the widget")));
    c.setDigitalReadoutVisible(false);
    const QImage withoutPod = renderCompass(c);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (pixelSquare(x, y, dpr).intersects(outer) && img.pixel(x, y) != withoutPod.pixel(x, y)) {
                QFAIL(qPrintable(where + QStringLiteral("device pixel (%1, %2) is drawn over").arg(x).arg(y)));
            }
        }
    }

    // (b) Inside the 1 px border, away from the corners of radius 2, outside the text. The ink box of the text is
    // that of "HDG" drawn as the widget draws it (#167): same font, origin, colours and DPR, painted by a widget and
    // grabbed, so that the text gets the same antialiasing (subpixel at DPR 1)
    FlagTextReference referenceWidget(flag);
    referenceWidget.resize(c.size());
    const QImage reference = renderWidget(referenceWidget);
    QRect textInk;
    for (int y = 0; y < reference.height(); ++y) {
        for (int x = 0; x < reference.width(); ++x) {
            if (pixelSquare(x, y, dpr).intersects(flag.rect) && QColor(reference.pixel(x, y)) != flag.fill) {
                textInk |= QRect(x, y, 1, 1);
            }
        }
    }
    const QRectF inner = flag.rect.adjusted(0.5, 0.5, -0.5, -0.5);

    // (e) The ink of "HDG" inside the flag, within its border: every pixel of the text in the reference (anywhere in
    // the widget) inside the border of the flag
    for (int y = 0; y < reference.height(); ++y) {
        for (int x = 0; x < reference.width(); ++x) {
            const QRectF px = pixelSquare(x, y, dpr);
            if (!QRectF(c.rect()).contains(px)) continue; // edge pixels at a fractional DPR: not painted by the fill
            if (QColor(reference.pixel(x, y)) != flag.fill && !inner.contains(px)) {
                QFAIL(qPrintable(where + QStringLiteral("device pixel (%1, %2) of \"HDG\" is not inside the border")
                                             .arg(x).arg(y)));
            }
        }
    }

    const double corner = 2.5;
    const QRectF corners[] = {QRectF(flag.rect.topLeft(), QSizeF(corner, corner)),
                              QRectF(flag.rect.topRight() - QPointF(corner, 0.0), QSizeF(corner, corner)),
                              QRectF(flag.rect.bottomLeft() - QPointF(0.0, corner), QSizeF(corner, corner)),
                              QRectF(flag.rect.bottomRight() - QPointF(corner, corner), QSizeF(corner, corner))};
    int checked = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRectF px = pixelSquare(x, y, dpr);
            if (!inner.contains(px) || textInk.contains(x, y)) continue;
            if (std::any_of(std::begin(corners), std::end(corners), [&](const QRectF &r) { return px.intersects(r); })) continue;
            ++checked;
            if (QColor(img.pixel(x, y)) != flag.fill) {
                QFAIL(qPrintable(where + QStringLiteral("device pixel (%1, %2) is %3, not the flag colour %4")
                                             .arg(x).arg(y).arg(QColor(img.pixel(x, y)).name(), flag.fill.name())));
            }
        }
    }
    QVERIFY2(checked > 0, qPrintable(where + QStringLiteral("no pixel of the flag colour to check")));

    // (c)
    const int sevenPt = static_cast<int>(std::ceil(7.0 * c.logicalDpiY() / 72.0));
    QVERIFY2(flag.textPixelSize >= sevenPt,
             qPrintable(where + QStringLiteral("\"HDG\" at %1 px, 7 pt is %2 px").arg(flag.textPixelSize).arg(sevenPt)));
    QVERIFY2(contrastRatio(flag.text, flag.fill) >= 4.5,
             qPrintable(where + QStringLiteral("contrast %1:1").arg(contrastRatio(flag.text, flag.fill), 0, 'f', 2)));
}

// #151: a heading has no range: every finite angle is Valid, NaN and ±Inf are Invalid; never Over/Underrange
void tst_Compass::valueStateOnlyValidOrInvalid()
{
    const double inf = std::numeric_limits<double>::infinity();
    Compass c;
    QCOMPARE(c.valueState(), ValueState::Valid);
    struct Step {
        double input;
        ValueState state;
    };
    const Step steps[] = {
        {720.5, ValueState::Valid}, {-30.0, ValueState::Valid}, {kNaN, ValueState::Invalid}, {1e9, ValueState::Valid},
        {inf, ValueState::Invalid}, {359.9, ValueState::Valid}, {-inf, ValueState::Invalid}, {0.0, ValueState::Valid},
    };
    for (const Step &step : steps) {
        c.setHeading(step.input);
        QVERIFY2(c.valueState() == step.state, qPrintable(QStringLiteral("input %1").arg(step.input)));
        QCOMPARE(std::isnan(c.heading()), step.state == ValueState::Invalid);
    }
    QCOMPARE(c.property("valueState").value<ValueState>(), ValueState::Valid);
}

void tst_Compass::valueStateSignalOrder()
{
    Compass c;
    c.setHeading(90.0);
    QStringList events;
    connect(&c, &Compass::headingChanged, &c, [&events](double h) { events << QStringLiteral("heading %1").arg(h); });
    connect(&c, &Compass::valueStateChanged, &c,
            [&events](ValueState st) { events << QStringLiteral("state %1").arg(static_cast<int>(st)); });

    c.setHeading(kNaN);
    QCOMPARE(events, QStringList({QStringLiteral("heading nan"), QStringLiteral("state 3")}));
    events.clear();
    c.setHeading(std::numeric_limits<double>::infinity());
    QVERIFY(events.isEmpty());

    // Back to the heading held before: still a change, since heading() was NaN
    c.setHeading(90.0);
    QCOMPARE(events, QStringList({QStringLiteral("heading 90"), QStringLiteral("state 0")}));
    events.clear();
    c.setHeading(450.0);
    QVERIFY(events.isEmpty());
}

// #151: heading-up, the position of the bug from 12 o'clock reads as a course deviation, unknown while the heading
// is invalid: the bug is still drawn (the target is kept) but dimmed with the card. North-up the bug is an
// absolute bearing and stays at full colour
void tst_Compass::headingUpInvalidDimsTheBug()
{
    const auto bright = [](QRgb p) { return qGreen(p) > 150 && qBlue(p) > 150 && qRed(p) < 90; };
    const auto dim = [](QRgb p) { return qGreen(p) > 60 && qBlue(p) > 60 && qRed(p) < 50 && std::abs(qGreen(p) - qBlue(p)) < 30; };
    for (const Compass::DisplayMode mode : {Compass::DisplayMode::HeadingUp, Compass::DisplayMode::NorthUp}) {
        const QString where = mode == Compass::DisplayMode::HeadingUp ? QStringLiteral("heading-up") : QStringLiteral("north-up");
        Compass c;
        prepareForDial(c, mode, 47.0);
        c.setBugColor(QColor(0, 255, 255));
        c.setTargetHeading(75.0);
        c.setHeadingBugVisible(true);
        const QImage valid = renderCompass(c);
        const Dial d = dialOf(c, valid);
        QVERIFY2(!ringPixels(valid, d, 0.8, 1.05, bright).isEmpty(), qPrintable(where));

        c.setHeading(kNaN);
        const QImage invalid = renderCompass(c);
        const int brightBug = ringPixels(invalid, d, 0.8, 1.05, bright).size();
        if (mode == Compass::DisplayMode::HeadingUp) {
            QVERIFY2(brightBug == 0, qPrintable(QStringLiteral("%1: %2 full-colour bug pixels").arg(where).arg(brightBug)));
            QVERIFY2(!ringPixels(invalid, d, 0.8, 1.05, dim).isEmpty(), qPrintable(where + QStringLiteral(": no bug")));
        } else {
            QVERIFY2(brightBug > 0, qPrintable(where));
        }
    }
}

void tst_Compass::northUpLubberAtTwelveOClock_data()
{
    QTest::addColumn<double>("heading");
    QTest::newRow("047") << 47.0;
    QTest::newRow("200") << 200.0;
    // The card stays at north, so the lubber line is kept for an invalid heading (unlike heading-up)
    QTest::newRow("invalid") << kNaN;
}

// #165: north-up draws the lubber line at 12 o'clock, at the top of the dial, as heading-up does; before, the
// painter was translated to the centre twice and the lubber line landed at the right edge of the widget
void tst_Compass::northUpLubberAtTwelveOClock()
{
    QFETCH(double, heading);
    Compass c;
    prepareForDial(c, Compass::DisplayMode::NorthUp, heading);
    const QImage img = renderCompass(c);
    const Dial d = dialOf(c, img);

    // Every lubber pixel of the widget, wherever it is
    const QVector<QPoint> lubber = ringPixels(img, d, 0.0, 10.0, isLubber);
    QVERIFY(!lubber.isEmpty());
    QPointF sum;
    for (const QPoint &p : lubber) {
        const QPointF q = QPointF(p) + QPointF(0.5, 0.5) - d.centre;
        QVERIFY2(std::abs(q.x()) < 0.1 * d.radius && q.y() < -0.8 * d.radius,
                 qPrintable(QStringLiteral("lubber pixel at (%1, %2) from the centre, radius %3")
                                .arg(q.x()).arg(q.y()).arg(d.radius)));
        sum += q;
    }
    const QPointF centroid = sum / lubber.size();
    QVERIFY2(std::abs(centroid.x()) < 0.03 * d.radius && std::abs(centroid.y() + d.radius) < 0.1 * d.radius,
             qPrintable(QStringLiteral("lubber centroid at (%1, %2) from the centre, radius %3")
                            .arg(centroid.x()).arg(centroid.y()).arg(d.radius)));

    c.setLubberLineVisible(false);
    QCOMPARE(ringPixels(renderCompass(c), d, 0.0, 10.0, isLubber).size(), 0);
}

// #226: no fixed minimum: the constructor no longer calls setMinimumSize(), so a layout can go down to
// minimumSizeHint(), computed from the font
void tst_Compass::minimumSizeIsNotFixed()
{
    Compass c;
    QCOMPARE(c.minimumSize(), QSize(0, 0));
}

// #226: from minimumSizeHint() up, the ring labels (N, NE, 030, …) are clear of each other and of the tick circle
void tst_Compass::ringLabelsClearFromMinimumUp_data() { addMinimumRows(); }

void tst_Compass::ringLabelsClearFromMinimumUp()
{
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    if (!labelGlyphsDrawn(c.font())) {
        COMPASS_SKIP("no glyphs for the labels in this font: checked on the layout by ringLabelsClearOnTheLayout (#226)");
    }
    const QString ring = ringLabelsError(c);
    QVERIFY2(ring.isEmpty(), qPrintable(QStringLiteral("%1x%2: ").arg(c.width()).arg(c.height()) + ring));
}

// #226: from minimumSizeHint() up, the readout is inside its pod: the widest of the 360 headings and "---°"
void tst_Compass::readoutFitsThePodFromMinimumUp_data() { addMinimumRows(); }

void tst_Compass::readoutFitsThePodFromMinimumUp()
{
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    const Card card = cardOf(c, c.devicePixelRatioF());
    const QRectF pod = podInterior(c, card);
    for (const double heading : {widestHeading(c, card), kNaN}) {
        const QRect ink = readoutInk(c, heading);
        QVERIFY2(!ink.isEmpty(), "no readout ink");
        QVERIFY2(ink.left() >= pod.left() && ink.right() + 1 <= pod.right() && ink.top() >= pod.top()
                     && ink.bottom() + 1 <= pod.bottom(),
                 qPrintable(QStringLiteral("%1x%2, heading %3: readout ink x %4-%5, y %6-%7 device px, pod inside its "
                                           "border x %8-%9, y %10-%11")
                                .arg(c.width()).arg(c.height()).arg(heading).arg(ink.left()).arg(ink.right() + 1)
                                .arg(ink.top()).arg(ink.bottom() + 1).arg(pod.left(), 0, 'f', 1)
                                .arg(pod.right(), 0, 'f', 1).arg(pod.top(), 0, 'f', 1).arg(pod.bottom(), 0, 'f', 1)));
        // Centred to within 1 device px: the margins of the ink to the left and right of the pod differ by 2 device px
        // at most (approved by the maintainer, #226). Qt draws the text on the widget with subpixel antialiasing and
        // with its glyphs on whole device px; the widget measures the ink on an image, in grey levels
        const double left = ink.left() - pod.left();
        const double right = pod.right() - (ink.right() + 1);
        QVERIFY2(std::abs(left - right) <= 2.0 + 1e-9, // 1e-9: the rounding of the fractional pod edges
                 qPrintable(QStringLiteral("%1x%2, heading %3: readout margins %4 (left) and %5 (right) device px")
                                .arg(c.width()).arg(c.height()).arg(heading).arg(left, 0, 'f', 2)
                                .arg(right, 0, 'f', 2)));
    }
}

// #226: from minimumSizeHint() up, the HDG flag of an invalid heading (#151) touches neither the ring labels nor
// the pod: no label ink within 1 device px of the flag; 1 device px at least between the flag and the pod, and 3 px
// between their borders
void tst_Compass::flagClearOfRingAndPodFromMinimumUp_data() { addMinimumRows(); }

void tst_Compass::flagClearOfRingAndPodFromMinimumUp()
{
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    const QVector<QPoint> ink = cardInk(c);
    const Card card = cardOf(c, c.devicePixelRatioF());

    const QSize size = c.size(); // prepareForDial() resizes to 300 x 300
    prepareForDial(c, Compass::DisplayMode::HeadingUp, 0.0);
    c.resize(size);
    c.setLubberLineVisible(false);
    c.setDigitalReadoutVisible(true);
    c.setHeading(kNaN);
    const QImage invalid = renderCompass(c);
    QRect flag;
    for (int y = 0; y < invalid.height(); ++y) {
        for (int x = 0; x < invalid.width(); ++x) {
            if (isFlagRed(invalid.pixel(x, y))) flag |= QRect(x, y, 1, 1);
        }
    }
    QVERIFY2(!flag.isEmpty(), "no flag");
    const QRectF pod = podInterior(c, card).adjusted(-0.75 * card.dpr, -0.75 * card.dpr, 0.75 * card.dpr,
                                                     0.75 * card.dpr);
    QVERIFY2(flag.bottom() + 1 + 1 <= pod.top() - 0.75 * card.dpr,
             qPrintable(QStringLiteral("%1x%2: the flag ends at y %3 device px, the pod border starts at %4")
                            .arg(c.width()).arg(c.height()).arg(flag.bottom() + 1)
                            .arg(pod.top() - 0.75 * card.dpr, 0, 'f', 1)));
    // And 3 px (docs/widgets.md) between the outer edges of their borders (the 1 px flag border and the 1.5 px pod
    // border, each centred on its outline), on the geometry the widget drew (CompassTestAccess)
    const QRectF flagRect = internal::CompassTestAccess::flag(c).rect;
    const double podOuterTop = pod.top() / card.dpr - 0.75;
    QVERIFY2(podOuterTop - (flagRect.bottom() + 0.5) >= 3.0,
             qPrintable(QStringLiteral("%1x%2: %3 px between the borders of the flag and of the pod")
                            .arg(c.width()).arg(c.height()).arg(podOuterTop - (flagRect.bottom() + 0.5), 0, 'f', 2)));

    if (!labelGlyphsDrawn(c.font())) {
        COMPASS_SKIP("no glyphs for the labels in this font: the labels against the flag are checked on the layout by "
              "ringLabelsClearOnTheLayout (#226)");
    }
    const QRect around = flag.adjusted(-1, -1, 1, 1);
    const auto touching = std::find_if(ink.cbegin(), ink.cend(), [&](const QPoint &p) {
        return around.contains(p) && pixelDistances(p, card.centre).second > card.radius * 0.45;
    });
    QVERIFY2(touching == ink.cend(),
             qPrintable(QStringLiteral("%1x%2: label ink at (%3, %4) within 1 device px of the flag (%5, %6)-(%7, %8)")
                            .arg(c.width()).arg(c.height()).arg(touching == ink.cend() ? 0 : touching->x())
                            .arg(touching == ink.cend() ? 0 : touching->y()).arg(flag.left()).arg(flag.top())
                            .arg(flag.right() + 1).arg(flag.bottom() + 1)));
}

// #226: the layout of \a c, read from the test hook after a render: it fits, every label ends 4 px inside the tick
// circle, and no label overlaps its neighbour, the flag or the pod (at any rotation of the card)
void checkLayoutClear(Compass &c)
{
    renderCompass(c);
    const CompassCardLayout layout = internal::CompassTestAccess::cardLayout(c);
    const QString where = QStringLiteral("%1x%2 at %3 dpi: ").arg(c.width()).arg(c.height()).arg(c.logicalDpiY());
    QVERIFY2(layout.fits, qPrintable(where + QStringLiteral("the layout does not fit")));
    QVERIFY2(!layout.labels.empty(), qPrintable(where + QStringLiteral("no labels")));
    const QPolygonF flag(layout.flag);
    const double podReach = std::hypot(layout.pod.width() / 2.0, layout.pod.height() / 2.0);
    for (size_t i = 0; i < layout.labels.size(); ++i) {
        const CompassCardLayout::Label &label = layout.labels[i];
        const CompassCardLayout::Label &next = layout.labels[(i + 1) % layout.labels.size()];
        double farthest = 0.0;
        double nearest = std::numeric_limits<double>::infinity();
        for (const QPointF &p : label.ink) {
            farthest = std::max(farthest, std::hypot(p.x(), p.y()));
            nearest = std::min(nearest, std::hypot(p.x(), p.y()));
        }
        QVERIFY2(layout.majorTickR - farthest >= 4.0,
                 qPrintable(where + QStringLiteral("the label at %1 degrees ends %2 px from the tick circle")
                                        .arg(label.angle).arg(layout.majorTickR - farthest, 0, 'f', 2)));
        QVERIFY2(layout.labels.size() == 1 || label.angle == next.angle || !label.ink.intersects(next.ink),
                 qPrintable(where + QStringLiteral("the labels at %1 and %2 degrees overlap").arg(label.angle).arg(next.angle)));
        QVERIFY2(!label.ink.intersects(flag),
                 qPrintable(where + QStringLiteral("the label at %1 degrees overlaps the flag").arg(label.angle)));
        // The corners are the nearest points only for a box seen from outside: enough here, the labels are far from
        // the centre, and a label across the pod would have a corner inside it
        QVERIFY2(nearest > podReach,
                 qPrintable(where + QStringLiteral("the label at %1 degrees reaches the pod at some rotation").arg(label.angle)));
    }
}

// #226: the layout of the card is checked against the geometry it reports (CompassTestAccess), in every
// configuration, also where no font is installed: from minimumSizeHint() up, no two neighbouring labels overlap, every
// label ends at least 4 px inside the circle of the major ticks, and no label overlaps the flag or (at any rotation of
// the card) the pod
void tst_Compass::ringLabelsClearOnTheLayout_data() { addMinimumRows(); }

void tst_Compass::ringLabelsClearOnTheLayout()
{
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    checkLayoutClear(c);
}

// #226: the order in which the card gives up its texts, on a range of sides (also below the minimum): the fonts first
// (never below 7 pt), then the numbers (every other one, then none), then the intercardinals; the cardinals always.
// From minimumSizeHint() up every layout fits
void tst_Compass::layoutThinsInOrder_data()
{
    QTest::addColumn<QString>("fontName");
    for (const char *font : {"application", "test font", "other font"}) QTest::newRow(font) << QString::fromLatin1(font);
}

void tst_Compass::layoutThinsInOrder()
{
    QFETCH(QString, fontName);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, 0, &error), qPrintable(error));
    const int minimum = c.minimumSizeHint().width();
    CompassCardLayout previous;
    for (int side = 60; side <= 600; ++side) {
        c.resize(side, side);
        renderCompass(c);
        const CompassCardLayout l = internal::CompassTestAccess::cardLayout(c);
        const QString where = QStringLiteral("side %1 (minimum %2): ").arg(side).arg(minimum);
        // Where the numbers come back, the fonts have just left 7 pt: the smallest is still at 7 pt. Measured (the
        // tightest bound, approved up to 0.5 pt, #226): 7.0000 pt with the three fonts, with and without fonts, at
        // DPR 1 and 2 (the cardinals at 10.1 to 11.6 pt: the numbers and the intercardinals bind)
        if (side > 60 && previous.numbers != CompassCardLayout::Numbers::All
            && l.numbers == CompassCardLayout::Numbers::All) {
            const double smallest = std::min({l.cardinalPt, l.intercardinalPt, l.numberPt});
            QVERIFY2(smallest <= 7.0, qPrintable(where + QStringLiteral("the numbers come back with the smallest font "
                                                                        "at %1 pt").arg(smallest, 0, 'f', 4)));
        }
        previous = l;
        QVERIFY2(l.cardinalPt >= 7.0 && l.intercardinalPt >= 7.0 && l.numberPt >= 7.0 && l.readoutPt >= 7.0,
                 qPrintable(where + QStringLiteral("a font below 7 pt: %1 %2 %3 %4").arg(l.cardinalPt).arg(l.intercardinalPt)
                                        .arg(l.numberPt).arg(l.readoutPt)));
        if (l.numbers != CompassCardLayout::Numbers::All) {
            QVERIFY2(l.cardinalPt == 7.0 && l.intercardinalPt == 7.0 && l.numberPt == 7.0,
                     qPrintable(where + QStringLiteral("numbers thinned before the fonts reached 7 pt")));
        }
        if (!l.intercardinals) {
            QVERIFY2(l.numbers == CompassCardLayout::Numbers::None,
                     qPrintable(where + QStringLiteral("intercardinals dropped before the numbers")));
        }
        std::vector<int> angles;
        for (const auto &label : l.labels) angles.push_back(label.angle);
        for (int cardinal : {0, 90, 180, 270}) {
            QVERIFY2(std::find(angles.begin(), angles.end(), cardinal) != angles.end(),
                     qPrintable(where + QStringLiteral("no label at %1 degrees").arg(cardinal)));
        }
        std::vector<int> expected{0, 90, 180, 270};
        if (l.intercardinals) expected.insert(expected.end(), {45, 135, 225, 315});
        if (l.numbers == CompassCardLayout::Numbers::All) expected.insert(expected.end(), {30, 60, 120, 150, 210, 240, 300, 330});
        if (l.numbers == CompassCardLayout::Numbers::EveryOther) expected.insert(expected.end(), {30, 120, 210, 300});
        std::sort(expected.begin(), expected.end());
        QVERIFY2(angles == expected, qPrintable(where + QStringLiteral("the labels drawn are not those of the layout")));
        if (side >= minimum) {
            QVERIFY2(l.fits, qPrintable(where + QStringLiteral("does not fit")));
        }
    }
}

// #226: the labels of \a c are drawn as laid out, read from their pixels (with glyphs for the labels): ink near the
// centre of each label of the layout, none where a number or an intercardinal was dropped, and the ink of each label
// laid out is its ink box: every pixel of it within 1 device px (antialiasing) of the box, and, in the frame of the
// label, the box filled to within kLabelFill of each of its sides
//
// kLabelFill, approved by the maintainer (#226): the ink box of the layout is the union of the tight boxes of the
// glyphs, which QFontMetricsF rounds out to whole px; measured, the drawn ink falls short of a side of it by at most
// 1.67 logical px with the application font, at the 4 DPRs and at 96, 120 and 144 dpi
constexpr double kLabelFill = 2.0;

void checkLabelsDrawnAsLaidOut(Compass &c)
{
    const QVector<QPoint> ink = cardInk(c);
    const CompassCardLayout layout = internal::CompassTestAccess::cardLayout(c);
    const Card card = cardOf(c, c.devicePixelRatioF());
    const QString where = QStringLiteral("%1x%2 at %3 dpi: ").arg(c.width()).arg(c.height()).arg(c.logicalDpiY());
    // The ink boxes of the labels laid out, in device px
    std::vector<QPolygonF> boxes;
    std::vector<QPainterPath> paths;
    for (const auto &l : layout.labels) {
        QPolygonF device;
        for (const QPointF &q : l.ink) device << card.centre + q * card.dpr;
        QPainterPath path;
        path.addPolygon(device);
        path.closeSubpath();
        boxes.push_back(device);
        paths.push_back(path);
    }
    const auto near = [](const QPainterPath &box, const QPoint &p) {
        return box.intersects(QRectF(p, QSizeF(1.0, 1.0)).adjusted(-1.0, -1.0, 1.0, 1.0));
    };
    // The ring of the labels: inside the circle of the major ticks and the inner ends of the major ticks (textColor;
    // a 1.8 px pen with square caps reaches 0.9 px inside the circle), by 1 device px of antialiasing. The labels end
    // 4 px inside
    const auto inRing = [&](const QPoint &p) {
        const auto [nearest, farthest] = pixelDistances(p, card.centre);
        return farthest >= card.radius * 0.45 && farthest <= card.majorTickR - 0.9 * card.dpr - 1.0;
    };
    for (const int angle : ringLabelAngles()) {
        const auto own = std::find_if(layout.labels.cbegin(), layout.labels.cend(),
                                      [&](const CompassCardLayout::Label &l) { return l.angle == angle; });
        const bool laidOut = own != layout.labels.cend();
        const size_t index = static_cast<size_t>(own - layout.labels.cbegin());
        // Within 5 degrees of the angle, the ink that no other label laid out explains
        bool inked = false;
        for (const QPoint &p : ink) {
            if (!inRing(p)) continue;
            const auto [lo, hi] = pixelAngles(p, card.centre, angle);
            if (hi <= -5.0 || lo >= 5.0) continue;
            bool other = false;
            for (size_t k = 0; k < paths.size(); ++k) {
                if ((!laidOut || k != index) && near(paths[k], p)) other = true;
            }
            if (other) continue;
            inked = true;
            if (laidOut) {
                QVERIFY2(near(paths[index], p),
                         qPrintable(where + QStringLiteral("device pixel (%1, %2) of the label at %3 degrees is "
                                                           "farther than 1 px from its ink box")
                                                .arg(p.x()).arg(p.y()).arg(angle)));
            }
        }
        QVERIFY2(inked == laidOut, qPrintable(where + QStringLiteral("the label at %1 degrees is %2 but %3")
                                                          .arg(angle)
                                                          .arg(laidOut ? QStringLiteral("laid out") : QStringLiteral("dropped"))
                                                          .arg(inked ? QStringLiteral("inked") : QStringLiteral("not inked"))));
        if (!laidOut) continue;
        // The box filled: in the frame of the label (rotated back to 0 degrees, around the centre of the card), the
        // centres of the ink pixels near the box reach each side of the box
        QTransform back;
        back.translate(card.centre.x(), card.centre.y());
        back.rotate(-angle);
        back.translate(-card.centre.x(), -card.centre.y());
        const QRectF box = back.map(boxes[index]).boundingRect();
        double left = 1e9, right = -1e9, top = 1e9, bottom = -1e9;
        for (const QPoint &p : ink) {
            if (!inRing(p) || !near(paths[index], p)) continue;
            const QPointF q = back.map(QPointF(p) + QPointF(0.5, 0.5));
            left = std::min(left, q.x());
            right = std::max(right, q.x());
            top = std::min(top, q.y());
            bottom = std::max(bottom, q.y());
        }
        const double deficit = std::max({left - box.left(), box.right() - right, top - box.top(), box.bottom() - bottom});
        QVERIFY2(deficit <= kLabelFill * card.dpr,
                 qPrintable(where + QStringLiteral("the label at %1 degrees falls %2 logical px short of a side of its "
                                                   "ink box")
                                        .arg(angle).arg(deficit / card.dpr, 0, 'f', 2)));
    }
}

void tst_Compass::labelsDrawnAsLaidOut_data() { addMinimumRows(); }

void tst_Compass::labelsDrawnAsLaidOut()
{
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    Compass c;
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    if (!labelGlyphsDrawn(c.font())) {
        COMPASS_SKIP("no glyphs for the labels in this font: checked on the layout by layoutThinsInOrder (#226)");
    }
    checkLabelsDrawnAsLaidOut(c);
}

// #226, #177: on a screen of another logical DPI the card is laid out and drawn at the DPI of the widget, not at that
// of the card pixmap (the primary screen)
void tst_Compass::labelsDrawnAtTheWidgetDpi_data()
{
    QTest::addColumn<int>("dpi");
    QTest::addColumn<QString>("fontName");
    QTest::addColumn<int>("offset");
    for (int dpi : {120, 144}) {
        for (const char *font : {"application", "other font"}) {
            for (int offset : {0, 13, 55, 144}) {
                QTest::newRow(qPrintable(QStringLiteral("DPI %1, %2, minimum + %3").arg(dpi).arg(QLatin1String(font))
                                             .arg(offset)))
                    << dpi << QString::fromLatin1(font) << offset;
            }
        }
    }
}

void tst_Compass::labelsDrawnAtTheWidgetDpi()
{
    QFETCH(int, dpi);
    QFETCH(QString, fontName);
    QFETCH(int, offset);
    ScreenChange::FakeScreen<Compass> c;
    c.fakeDpr = qApp->devicePixelRatio(); // the DPR of the run, only the logical DPI changes
    c.fakeDpi = dpi;
    QCOMPARE(c.logicalDpiY(), dpi);
    QString error;
    QVERIFY2(setUpMinimumRow(c, fontName, offset, &error), qPrintable(error));
    checkLayoutClear(c); // the layout at this DPI, in every configuration
    if (QTest::currentTestFailed()) return;
    if (!labelGlyphsDrawn(c.font())) {
        COMPASS_SKIP("no glyphs for the labels in this font: their layout at this DPI is checked above (#226)");
    }
    checkLabelsDrawnAsLaidOut(c);
}

// #226: the minimum depends on the font family and style: wider glyphs need a larger compass. compassOtherFont()
// is wider (letter spacing 150 %, also where glyphs are boxes). A font change tells the layout
void tst_Compass::minimumFollowsTheFont()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *c = new Compass;
    layout->addWidget(c);
    ScreenChange::LayoutRequests requests;
    host.installEventFilter(&requests);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const QSize before = c->minimumSizeHint();
    QCoreApplication::processEvents();
    requests.count = 0;
    c->setFont(compassOtherFont());
    QCoreApplication::processEvents();
    const QSize after = c->minimumSizeHint();
    QVERIFY2(after.width() > before.width() && after.height() > before.height(),
             qPrintable(QStringLiteral("minimum %1x%2 with the application font, %3x%4 with a wider one")
                            .arg(before.width()).arg(before.height()).arg(after.width()).arg(after.height())));
    QVERIFY2(requests.count > 0, "the layout was not told");
    Compass fresh;
    fresh.setFont(compassOtherFont());
    QCOMPARE(after, fresh.minimumSizeHint());
}

// #226: the minimum follows the logical DPI of the screen (the fonts of the card are in points, #177); a DPR
// alone does not change it. A move to another screen gives the minimum and the pixels of a new compass there
// #226, #177: after a change of logical DPI, minimumSizeHint() is that of the new DPI at once, before any repaint
// (a layout can ask for it first)
void tst_Compass::minimumFollowsTheDpiBeforeARepaint_data()
{
    QTest::addColumn<int>("dpi");
    QTest::addColumn<QString>("fontName");
    for (int dpi : {120, 144}) {
        for (const char *font : {"application", "other font"}) {
            QTest::newRow(qPrintable(QStringLiteral("DPI %1, %2").arg(dpi).arg(QLatin1String(font))))
                << dpi << QString::fromLatin1(font);
        }
    }
}

void tst_Compass::minimumFollowsTheDpiBeforeARepaint()
{
    QFETCH(int, dpi);
    QFETCH(QString, fontName);
    const auto setUp = [&](ScreenChange::FakeScreen<Compass> &c) {
        c.fakeDpr = qApp->devicePixelRatio();
        if (fontName == QLatin1String("other font")) c.setFont(compassOtherFont());
    };
    ScreenChange::FakeScreen<Compass> c;
    setUp(c);
    const QSize before = c.minimumSizeHint();
    c.resize(before);
    renderCompass(c);
    c.fakeDpi = dpi;
    const QSize after = c.minimumSizeHint(); // no repaint in between
    ScreenChange::FakeScreen<Compass> fresh;
    setUp(fresh);
    fresh.fakeDpi = dpi;
    QCOMPARE(after, fresh.minimumSizeHint());
    QVERIFY2(after != before, qPrintable(QStringLiteral("the same minimum %1 at 96 and %2 dpi").arg(before.width()).arg(dpi)));
}

void tst_Compass::minimumFollowsTheScreen_data() { ScreenChange::addScreens(); }

void tst_Compass::minimumFollowsTheScreen()
{
    QFETCH(double, dpr);
    QFETCH(int, dpi);
    bool minimumChanged = false;
    ScreenChange::checkScreenChange<Compass>([](Compass &) {}, dpr, dpi, &minimumChanged);
    if (QTest::currentTestFailed()) return;
    QCOMPARE(minimumChanged, dpi != 96);
}

QTEST_MAIN(tst_Compass)
#include "tst_Compass.moc"


