// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #153: ValveIndicator. Status and opening (independent), the shape of each status and type, the opening bar and its
// readout (#151, #159, #167), the computed minimum size and the screen change of #177.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/ValveIndicator.h>

#include "internal/FontScale.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/ValveIndicatorLayout.h"
#include "internal/ValveIndicatorTestAccess.h"

#include "FontTestUtils.h"
#include "HatchTestUtils.h"
#include "ReadoutTestUtils.h"
#include "ScreenChangeTestUtils.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QVBoxLayout>

#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;
using ValveType = ValveIndicator::ValveType;
using ValveStatus = ValveIndicator::ValveStatus;

class tst_ValveIndicator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults();
    void statusAndOpeningAreIndependent();
    void openingStateAndRawOpening();
    void everyTypeAndStatusDiffersWithoutColour_data();
    void everyTypeAndStatusDiffersWithoutColour();
    void statusShapesAtTheirPoints_data();
    void statusShapesAtTheirPoints();
    void typeMarksAtTheirPoints_data();
    void typeMarksAtTheirPoints();
    void pipeStubsOnlyWhenAsked();
    void statusTags();
    void openingShownForControlOrWhenAsked();
    void openingBar_data();
    void openingBar();
    void readoutStates();
    void textsInsideTheWidgetFromTheMinimumUp_data();
    void textsInsideTheWidgetFromTheMinimumUp();
    void readoutNeverBelowSevenPoints_data();
    void readoutNeverBelowSevenPoints();
    void minimumSizeHintFollowsTheConfiguration();
    void screenChangeOfAVisibleValveIndicator_data();
    void screenChangeOfAVisibleValveIndicator();
    void nonFiniteInput();
};

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Colours that no other part of the drawing uses
const QColor kOpen(0, 128, 255);
const QColor kClosed(255, 0, 0);
const QColor kTransit(255, 128, 0);
const QColor kFault(255, 0, 255);
const QColor kUnknown(0, 255, 0); // pure green, for HatchTest
const QColor kOutline(0, 255, 255);
const QColor kText(255, 255, 0);
const QColor kBackground(0, 0, 0);
const QColor kPod(16, 20, 26); // the readout panel, as in ValveIndicator.cpp

const ValveType kTypes[] = {ValveType::Gate, ValveType::Ball, ValveType::Butterfly, ValveType::Control};
const ValveStatus kStatuses[] = {ValveStatus::Unknown, ValveStatus::Closed, ValveStatus::Open, ValveStatus::Transit,
                                 ValveStatus::Fault};

template <class E>
QString nameOf(E e)
{
    return QString::fromLatin1(QMetaEnum::fromType<E>().valueToKey(static_cast<int>(e)));
}

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

// The valve with the test colours on an opaque black background
void prepare(ValveIndicator &v, QSize size = QSize(200, 200))
{
    v.setOpenColor(kOpen);
    v.setClosedColor(kClosed);
    v.setTransitColor(kTransit);
    v.setFaultColor(kFault);
    v.setUnknownColor(kUnknown);
    v.setOutlineColor(kOutline);
    v.setTextColor(kText);
    QPalette pal = v.palette();
    pal.setColor(QPalette::Window, kBackground);
    v.setPalette(pal);
    v.setAutoFillBackground(true);
    v.resize(size);
}

QImage render(ValveIndicator &v)
{
    return v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

internal::ValveLayout layoutOf(const ValveIndicator &v)
{
    return internal::ValveIndicatorTestAccess::lastLayout(v);
}

// Pixel at a logical point
QRgb at(const QImage &img, QPointF p)
{
    const double dpr = img.devicePixelRatio();
    const int px = static_cast<int>(std::floor(p.x() * dpr));
    const int py = static_cast<int>(std::floor(p.y() * dpr));
    if (px < 0 || py < 0 || px >= img.width() || py >= img.height()) {
        return qRgba(0, 0, 0, 0);
    }
    return img.pixel(px, py);
}

// Pixel at a point of the symbol in unit coordinates (see ValveIndicatorLayout.h)
QRgb atUnit(const QImage &img, const ValveIndicator &v, QPointF unit)
{
    return at(img, layoutOf(v).transform.map(unit));
}

QString rgb(QRgb p)
{
    return QStringLiteral("rgb(%1, %2, %3)").arg(qRed(p)).arg(qGreen(p)).arg(qBlue(p));
}

// Device pixels of \a img in a logical rectangle that match a predicate
int countIn(const QImage &img, const QRectF &logical, const std::function<bool(QRgb)> &match)
{
    const double dpr = img.devicePixelRatio();
    const QRect r = QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect().intersected(img.rect());
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            n += match(img.pixel(x, y)) ? 1 : 0;
        }
    }
    return n;
}

// The pixels along a segment of the symbol (unit coordinates), one per device pixel of its length
QVector<QRgb> alongUnit(const QImage &img, const ValveIndicator &v, QPointF from, QPointF to)
{
    const QTransform &t = layoutOf(v).transform;
    const QPointF a = t.map(from);
    const QPointF b = t.map(to);
    const int n = std::max(2, static_cast<int>(QLineF(a, b).length() * img.devicePixelRatio()));
    QVector<QRgb> out;
    for (int i = 0; i <= n; ++i) {
        out.push_back(at(img, a + (b - a) * (double(i) / n)));
    }
    return out;
}

// Readout ink: the pixels that change when only the readout colour changes (#167)
QVector<QPoint> readoutInk(ValveIndicator &v)
{
    v.setReadoutColor(QColor(255, 0, 255, 0));
    const QImage without = render(v);
    v.setReadoutColor(QColor(255, 0, 255));
    const QImage with = render(v);
    v.setReadoutColor(QColor(240, 244, 248));
    QVector<QPoint> ink;
    for (int y = 0; y < with.height(); ++y) {
        for (int x = 0; x < with.width(); ++x) {
            if (with.pixel(x, y) != without.pixel(x, y)) {
                ink.push_back(QPoint(x, y));
            }
        }
    }
    return ink;
}

QRect boundsOf(const QVector<QPoint> &points)
{
    QRect r;
    for (const QPoint &p : points) {
        r |= QRect(p, QSize(1, 1));
    }
    return r;
}

// Ink box of a readout line drawn at 7 pt as ValveIndicator draws it (bold widget font, greyscale antialiasing, on
// the opaque panel colour, at the widget's DPI and DPR): the smallest box over the 64 x 64 sub-pixel positions of
// the raster engine (no slack, an opaque background)
QSize sevenPointLineInk(const QString &text, bool triangle, const QWidget &w, qreal dpr)
{
    static QHash<QString, QSize> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4|%5").arg(triangle).arg(text).arg(w.logicalDpiY()).arg(dpr)
                            .arg(w.font().toString());
    if (cache.contains(key)) {
        return cache.value(key);
    }
    QFont font = w.font();
    font.setBold(true);
    font.setPixelSize(internal::minimumReadoutPixelSize(w.logicalDpiY()));
    font.setStyleStrategy(QFont::StyleStrategy(font.styleStrategy() | QFont::NoSubpixelAntialias));
    const QFontMetricsF fm(font, &w);
    const double triW = triangle ? fm.capHeight() * 0.9 : 0.0;
    const double gap = (triangle && !text.isEmpty()) ? triW * 0.35 : 0.0;
    QSize smallest(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    const QSize area(static_cast<int>(std::ceil(triW + gap + fm.horizontalAdvance(text) + 8.0)),
                     static_cast<int>(std::ceil(fm.height() + 8.0)));
    for (int step = 0; step < 64 * 64; ++step) {
        const QPointF offset((step % 64) / 64.0 / dpr, (step / 64) / 64.0 / dpr);
        QImage img(area * dpr, QImage::Format_RGB32);
        img.setDevicePixelRatio(dpr);
        img.setDotsPerMeterX(qRound(w.logicalDpiX() / 0.0254));
        img.setDotsPerMeterY(qRound(w.logicalDpiY() / 0.0254));
        img.fill(kPod);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            const QPointF origin = QPointF(4.0, 4.0 + fm.ascent()) + offset;
            if (triangle) {
                const double midY = origin.y() - fm.capHeight() * 0.5;
                QPolygonF tri;
                tri << QPointF(origin.x(), midY + triW * 0.5) << QPointF(origin.x() + triW, midY + triW * 0.5)
                    << QPointF(origin.x() + triW * 0.5, midY - triW * 0.5);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 0, 255));
                p.drawPolygon(tri);
            }
            p.setFont(font);
            p.setPen(QColor(255, 0, 255));
            p.drawText(origin + QPointF(triW + gap, 0.0), text);
        }
        QRect box;
        for (int y = 0; y < img.height(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (row[x] != kPod.rgb()) {
                    box |= QRect(x, y, 1, 1);
                }
            }
        }
        smallest = QSize(std::min(smallest.width(), box.width()), std::min(smallest.height(), box.height()));
    }
    cache.insert(key, smallest);
    return smallest;
}

// Rows: every type, horizontal and vertical, with and without stubs
void addTypesAndOrientations()
{
    QTest::addColumn<ValveType>("type");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<bool>("stubs");
    for (ValveType type : kTypes) {
        for (bool vertical : {false, true}) {
            for (bool stubs : {false, true}) {
                QTest::addRow("%s%s%s", qPrintable(nameOf(type)), vertical ? ", vertical" : "", stubs ? ", stubs" : "")
                    << type << vertical << stubs;
            }
        }
    }
}

} // namespace

void tst_ValveIndicator::defaults()
{
    ValveIndicator v;
    QCOMPARE(v.valveType(), ValveType::Gate);
    QCOMPARE(v.status(), ValveStatus::Unknown);
    QVERIFY(std::isnan(v.opening()));
    QVERIFY(std::isnan(v.rawOpening()));
    QCOMPARE(v.openingState(), ValueState::Invalid);
    QCOMPARE(v.openingVisible(), false);
    QCOMPARE(v.precision(), 0);
    QCOMPARE(v.orientation(), Qt::Horizontal);
    QCOMPARE(v.pipeStubsVisible(), false);
    QCOMPARE(v.openColor(), QColor(46, 204, 113));
    QCOMPARE(v.closedColor(), QColor(231, 76, 60));
    QCOMPARE(v.transitColor(), QColor(241, 196, 15));
    QCOMPARE(v.faultColor(), QColor(217, 70, 239));
    QCOMPARE(v.unknownColor(), QColor(149, 165, 166));
    QCOMPARE(v.outlineColor(), QColor(200, 208, 218));
    QCOMPARE(v.textColor(), QColor(240, 244, 248));
    QCOMPARE(v.readoutColor(), QColor(240, 244, 248));
    QCOMPARE(v.property("status").value<ValveStatus>(), ValveStatus::Unknown);
    QCOMPARE(v.property("valveType").value<ValveType>(), ValveType::Gate);
    // A new valve is drawn as an unknown one: not as a closed or an open valve
    ValveIndicator fresh;
    prepare(fresh);
    const QImage a = render(fresh);
    for (ValveStatus s : {ValveStatus::Closed, ValveStatus::Open, ValveStatus::Transit, ValveStatus::Fault}) {
        ValveIndicator other;
        prepare(other);
        other.setStatus(s);
        QVERIFY2(a != render(other), qPrintable(nameOf(s)));
    }
    ValveIndicator unknown;
    prepare(unknown);
    unknown.setStatus(ValveStatus::Open);
    unknown.setStatus(ValveStatus::Unknown);
    QCOMPARE(a, render(unknown));
    // The setters and their signals
    QSignalSpy statusSpy(&v, &ValveIndicator::statusChanged);
    QSignalSpy appearanceSpy(&v, &ValveIndicator::appearanceChanged);
    v.setStatus(ValveStatus::Fault);
    v.setStatus(ValveStatus::Fault);
    QCOMPARE(statusSpy.size(), 1);
    QCOMPARE(statusSpy.first().first().value<ValveStatus>(), ValveStatus::Fault);
    v.setValveType(ValveType::Ball);
    v.setOrientation(Qt::Vertical);
    v.setPipeStubsVisible(true);
    v.setOpeningVisible(true);
    v.setPrecision(2);
    v.setPrecision(-3);
    QCOMPARE(v.precision(), 0);
    v.setPrecision(0);
    QCOMPARE(appearanceSpy.size(), 6);
    QVERIFY(v.setProperty("valveType", QVariant::fromValue(ValveType::Control)));
    QCOMPARE(v.valveType(), ValveType::Control);
}

// The widget never derives one from the other: a closed valve with a 40 % opening is shown as told
void tst_ValveIndicator::statusAndOpeningAreIndependent()
{
    ValveIndicator v;
    QSignalSpy statusSpy(&v, &ValveIndicator::statusChanged);
    QSignalSpy openingSpy(&v, &ValveIndicator::openingChanged);
    QSignalSpy stateSpy(&v, &ValveIndicator::openingStateChanged);
    v.setStatus(ValveStatus::Closed);
    QVERIFY(std::isnan(v.opening()));
    QCOMPARE(v.openingState(), ValueState::Invalid);
    QVERIFY(openingSpy.isEmpty() && stateSpy.isEmpty());
    for (double opening : {40.0, 0.0, 100.0, 150.0, kNaN}) {
        v.setOpening(opening);
        QCOMPARE(v.status(), ValveStatus::Closed);
    }
    QCOMPARE(statusSpy.size(), 1);
    v.setOpening(40.0);
    openingSpy.clear();
    stateSpy.clear();
    for (ValveStatus s : kStatuses) {
        v.setStatus(s);
        QCOMPARE(v.opening(), 40.0);
        QCOMPARE(v.rawOpening(), 40.0);
        QCOMPARE(v.openingState(), ValueState::Valid);
    }
    QVERIFY(openingSpy.isEmpty() && stateSpy.isEmpty());
    // And on the screen: the readout of a closed valve shows the 40 % it was told, as for an open one
    ValveIndicator closed;
    prepare(closed);
    closed.setValveType(ValveType::Control);
    closed.setStatus(ValveStatus::Closed);
    closed.setOpening(40.0);
    ValveIndicator open;
    prepare(open);
    open.setValveType(ValveType::Control);
    open.setStatus(ValveStatus::Open);
    open.setOpening(40.0);
    const QVector<QPoint> a = readoutInk(closed);
    QVERIFY(!a.isEmpty());
    QCOMPARE(a, readoutInk(open));
}

void tst_ValveIndicator::openingStateAndRawOpening()
{
    struct Case {
        double input;
        double opening;
        ValueState state;
    };
    const Case cases[] = {{42.5, 42.5, ValueState::Valid},       {0.0, 0.0, ValueState::Valid},
                          {100.0, 100.0, ValueState::Valid},     {130.0, 100.0, ValueState::Overrange},
                          {-5.0, 0.0, ValueState::Underrange},   {kInf, 100.0, ValueState::Overrange},
                          {-kInf, 0.0, ValueState::Underrange},  {kNaN, kNaN, ValueState::Invalid}};
    for (const Case &c : cases) {
        ValveIndicator v;
        v.setOpening(50.0);
        QSignalSpy openingSpy(&v, &ValveIndicator::openingChanged);
        QSignalSpy stateSpy(&v, &ValveIndicator::openingStateChanged);
        QStringList order;
        connect(&v, &ValveIndicator::openingChanged, &v, [&] { order << QStringLiteral("opening"); });
        connect(&v, &ValveIndicator::openingStateChanged, &v, [&] { order << QStringLiteral("state"); });
        v.setOpening(c.input);
        const QString where = QStringLiteral("input %1").arg(c.input);
        QVERIFY2(std::isnan(c.opening) ? std::isnan(v.opening()) : v.opening() == c.opening, qPrintable(where));
        QVERIFY2(std::isnan(c.input) ? std::isnan(v.rawOpening()) : v.rawOpening() == c.input, qPrintable(where));
        QCOMPARE(v.openingState(), c.state);
        QCOMPARE(v.property("openingState").value<ValueState>(), c.state);
        QCOMPARE(openingSpy.size(), c.input == 50.0 ? 0 : 1);
        QCOMPARE(stateSpy.size(), c.state == ValueState::Valid ? 0 : 1);
        if (stateSpy.size() == 1) {
            QCOMPARE(order, QStringList({QStringLiteral("opening"), QStringLiteral("state")}));
        }
    }
    // An out-of-range input after another one with the same clamped opening: no signal, the readout follows
    ValveIndicator v;
    prepare(v);
    v.setValveType(ValveType::Control);
    v.setOpening(130.0);
    const QVector<QPoint> first = readoutInk(v);
    QSignalSpy openingSpy(&v, &ValveIndicator::openingChanged);
    QSignalSpy stateSpy(&v, &ValveIndicator::openingStateChanged);
    v.setOpening(1700.0); // another number of digits: without installed fonts every digit is the same box
    QVERIFY(openingSpy.isEmpty() && stateSpy.isEmpty());
    QCOMPARE(v.rawOpening(), 1700.0);
    QVERIFY(readoutInk(v) != first);
}

void tst_ValveIndicator::everyTypeAndStatusDiffersWithoutColour_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("horizontal") << false;
    QTest::newRow("vertical") << true;
}

// #167: no status depends on colour. With the five status colours the same, every type x status (20 images, Unknown
// included) differs from every other in greyscale, on at least 1 % of the device pixels of the body box
void tst_ValveIndicator::everyTypeAndStatusDiffersWithoutColour()
{
    QFETCH(bool, vertical);
    const QColor same(150, 150, 150);
    struct Shot {
        QString name;
        QImage grey;
        double bodyArea;
    };
    QVector<Shot> shots;
    for (ValveType type : kTypes) {
        for (ValveStatus status : kStatuses) {
            ValveIndicator v;
            prepare(v, vertical ? QSize(160, 220) : QSize(220, 180));
            for (auto set : {&ValveIndicator::setOpenColor, &ValveIndicator::setClosedColor,
                             &ValveIndicator::setTransitColor, &ValveIndicator::setFaultColor,
                             &ValveIndicator::setUnknownColor}) {
                (v.*set)(same);
            }
            v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
            v.setValveType(type);
            v.setStatus(status);
            const QImage img = render(v);
            QImage grey(img.size(), QImage::Format_Grayscale8);
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    grey.setPixel(x, y, static_cast<uint>(qGray(img.pixel(x, y))));
                }
            }
            const QRectF body = layoutOf(v).transform.mapRect(internal::valveBodyPath(internal::ValveSymbol::Gate).boundingRect());
            const double dpr = img.devicePixelRatio();
            shots.push_back({nameOf(type) + QLatin1Char(' ') + nameOf(status), grey, body.width() * body.height() * dpr * dpr});
        }
    }
    for (int i = 0; i < shots.size(); ++i) {
        for (int j = i + 1; j < shots.size(); ++j) {
            int differ = 0;
            for (int y = 0; y < shots[i].grey.height(); ++y) {
                const uchar *a = shots[i].grey.constScanLine(y);
                const uchar *b = shots[j].grey.constScanLine(y);
                for (int x = 0; x < shots[i].grey.width(); ++x) {
                    differ += a[x] != b[x] ? 1 : 0;
                }
            }
            QVERIFY2(differ >= 0.01 * shots[i].bodyArea,
                     qPrintable(QStringLiteral("%1 and %2: %3 grey pixels differ, %4 needed")
                                    .arg(shots[i].name, shots[j].name).arg(differ).arg(0.01 * shots[i].bodyArea)));
        }
    }
}

void tst_ValveIndicator::statusShapesAtTheirPoints_data()
{
    addTypesAndOrientations();
}

// Each status at the points of the symbol that tell it (unit coordinates, see ValveIndicatorLayout.h):
// - Closed: both triangles filled;
// - Open: empty triangles, a solid outline (the downstream base of the body has no gap);
// - Transit: the upstream triangle filled, the downstream one empty, its base dashed (ink and gaps);
// - Fault: empty triangles, a cross over the body off its outline (at (±0.3, ±0.3), outside the triangles);
// - Unknown: the body hatched.
void tst_ValveIndicator::statusShapesAtTheirPoints()
{
    QFETCH(ValveType, type);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    const auto make = [&](ValveIndicator &v, ValveStatus status) {
        prepare(v, vertical ? QSize(160, 220) : QSize(220, 180));
        v.setValveType(type);
        v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
        v.setPipeStubsVisible(stubs);
        v.setStatus(status);
    };
    const QPointF upstream(-0.5, 0.0);
    const QPointF downstream(0.5, 0.0);
    const QPointF crossPoints[] = {QPointF(0.3, 0.3), QPointF(-0.3, 0.3), QPointF(0.3, -0.3), QPointF(-0.3, -0.3)};
    // The downstream base of the body, clear of the corners
    const auto base = [](const QImage &img, const ValveIndicator &v) {
        return alongUnit(img, v, QPointF(1.0, -0.45), QPointF(1.0, 0.45));
    };

    {
        ValveIndicator v;
        make(v, ValveStatus::Closed);
        const QImage img = render(v);
        for (QPointF p : {upstream, downstream}) {
            QVERIFY2(near(atUnit(img, v, p), kClosed), qPrintable(QStringLiteral("closed at (%1, %2): %3").arg(p.x()).arg(p.y()).arg(rgb(atUnit(img, v, p)))));
        }
    }
    {
        ValveIndicator v;
        make(v, ValveStatus::Open);
        const QImage img = render(v);
        for (QPointF p : {upstream, downstream}) {
            QVERIFY2(near(atUnit(img, v, p), kBackground), qPrintable(QStringLiteral("open at (%1, %2): %3").arg(p.x()).arg(p.y()).arg(rgb(atUnit(img, v, p)))));
        }
        for (QRgb p : base(img, v)) {
            QVERIFY2(near(p, kOpen), qPrintable(QStringLiteral("open outline: %1").arg(rgb(p))));
        }
        for (QPointF p : crossPoints) {
            QVERIFY2(near(atUnit(img, v, p), kBackground), qPrintable(QStringLiteral("open, (%1, %2): %3").arg(p.x()).arg(p.y()).arg(rgb(atUnit(img, v, p)))));
        }
    }
    {
        ValveIndicator v;
        make(v, ValveStatus::Transit);
        const QImage img = render(v);
        QVERIFY2(near(atUnit(img, v, upstream), kTransit), qPrintable(rgb(atUnit(img, v, upstream))));
        QVERIFY2(near(atUnit(img, v, downstream), kBackground), qPrintable(rgb(atUnit(img, v, downstream))));
        int ink = 0;
        int gap = 0;
        for (QRgb p : base(img, v)) {
            ink += near(p, kTransit) ? 1 : 0;
            gap += near(p, kBackground) ? 1 : 0;
        }
        QVERIFY2(ink >= 2 && gap >= 2, qPrintable(QStringLiteral("transit outline: %1 ink, %2 gap pixels").arg(ink).arg(gap)));
    }
    {
        ValveIndicator v;
        make(v, ValveStatus::Fault);
        const QImage img = render(v);
        for (QPointF p : {upstream, downstream}) {
            QVERIFY2(near(atUnit(img, v, p), kBackground), qPrintable(QStringLiteral("fault at (%1, %2): %3").arg(p.x()).arg(p.y()).arg(rgb(atUnit(img, v, p)))));
        }
        for (QPointF p : crossPoints) {
            QVERIFY2(near(atUnit(img, v, p), kFault), qPrintable(QStringLiteral("fault cross at (%1, %2): %3").arg(p.x()).arg(p.y()).arg(rgb(atUnit(img, v, p)))));
        }
    }
    {
        ValveIndicator v;
        make(v, ValveStatus::Unknown);
        const QImage img = render(v);
        // The inside of the body, 3 px away from its outline, without the marks of the type
        const QPainterPath body = layoutOf(v).transform.map(internal::valveBodyPath(static_cast<internal::ValveSymbol>(type)));
        const double dpr = img.devicePixelRatio();
        QVector<QPoint> area;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QPointF c((x + 0.5) / dpr, (y + 0.5) / dpr);
                bool inside = true;
                for (QPointF d : {QPointF(0, 0), QPointF(3, 0), QPointF(-3, 0), QPointF(0, 3), QPointF(0, -3)}) {
                    inside = inside && body.contains(c + d);
                }
                if (inside && !near(img.pixel(x, y), kOutline, 120)) {
                    area.push_back(QPoint(x, y));
                }
            }
        }
        // The pixels the hatch touches (far from the background, antialiased ones included, as in tst_TankDisplay),
        // painted pure green for HatchTest
        QImage mask(img.size(), QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                mask.setPixel(x, y, channelDistance(img.pixel(x, y), kBackground.rgb()) > 60 ? qRgb(0, 255, 0) : qRgb(0, 0, 0));
            }
        }
        QVERIFY(area.size() > 500);
        const QString error = HatchTest::hatchError(mask, area);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // In unknownColor(): pure green blended on black stays green at any coverage (a 1.2 px line covers no whole
        // pixel at DPR 1), so every pixel the hatch touches has red and blue below a quarter of its green
        int otherHue = 0;
        for (const QPoint &q : area) {
            const QRgb px = img.pixel(q);
            if (channelDistance(px, kBackground.rgb()) > 60 && std::max(qRed(px), qBlue(px)) * 4 > qGreen(px)) {
                ++otherHue;
            }
        }
        QVERIFY2(otherHue == 0, qPrintable(QStringLiteral("%1 hatch pixels not in unknownColor()").arg(otherHue)));
    }
}

void tst_ValveIndicator::typeMarksAtTheirPoints_data()
{
    addTypesAndOrientations();
}

// The marks of the type in outlineColor(): the disc of a butterfly valve (a line through the centre), the stem and
// the dome of a control valve; the ball of a ball valve in the status colour; nothing of these on a gate valve
void tst_ValveIndicator::typeMarksAtTheirPoints()
{
    QFETCH(ValveType, type);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    ValveIndicator v;
    prepare(v, vertical ? QSize(160, 220) : QSize(220, 180));
    v.setValveType(type);
    v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    v.setPipeStubsVisible(stubs);
    v.setStatus(ValveStatus::Open);
    const QImage img = render(v);
    const auto expect = [&](QPointF p, const QColor &c) {
        const QRgb got = atUnit(img, v, p);
        QVERIFY2(near(got, c), qPrintable(QStringLiteral("(%1, %2): %3, expected %4").arg(p.x()).arg(p.y()).arg(rgb(got))
                                              .arg(rgb(c.rgb()))));
    };
    // On the curve of the ball the 2 px pen does not cover the pixel under the point fully: a pixel of that colour
    // within 1 px of it
    const auto expectNear = [&](QPointF p, const QColor &c) {
        const QPointF centre = layoutOf(v).transform.map(p);
        bool found = false;
        for (double dx : {-1.0, 0.0, 1.0}) {
            for (double dy : {-1.0, 0.0, 1.0}) {
                found = found || near(at(img, centre + QPointF(dx, dy)), c);
            }
        }
        QVERIFY2(found, qPrintable(QStringLiteral("(%1, %2): no %3 pixel within 1 px").arg(p.x()).arg(p.y()).arg(rgb(c.rgb()))));
    };
    const bool butterfly = type == ValveType::Butterfly;
    const bool control = type == ValveType::Control;
    const bool ball = type == ValveType::Ball;
    expect(QPointF(0.0, 0.7), butterfly ? kOutline : kBackground);
    expect(QPointF(0.0, -0.7), butterfly || control ? kOutline : kBackground);
    expect(QPointF(0.0, -1.0), control ? kOutline : kBackground);
    // The dome: its top and its base, the inside left empty
    if (control) {
        expect(QPointF(0.0, -1.7), kOutline);
        expect(QPointF(-0.4, -1.1), kOutline);
        expect(QPointF(0.4, -1.1), kOutline);
        expect(QPointF(-0.3, -1.4), kBackground);
    }
    // The ball: its circle in the status colour, the inside empty (an open valve)
    if (ball) {
        expectNear(QPointF(0.0, -0.3), kOpen);
        expectNear(QPointF(0.0, 0.3), kOpen);
        expect(QPointF(0.0, 0.15), kBackground);
    } else {
        expect(QPointF(0.0, -0.3), butterfly || control ? kOutline : kBackground);
        expect(QPointF(0.0, 0.3), butterfly ? kOutline : kBackground);
    }
}

void tst_ValveIndicator::pipeStubsOnlyWhenAsked()
{
    for (bool vertical : {false, true}) {
        for (bool stubs : {false, true}) {
            ValveIndicator v;
            prepare(v);
            v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
            v.setPipeStubsVisible(stubs);
            v.setStatus(ValveStatus::Open);
            const QImage img = render(v);
            for (double x : {-1.3, 1.3}) {
                const QRgb got = atUnit(img, v, QPointF(x, 0.0));
                QVERIFY2(near(got, stubs ? kOutline : kBackground),
                         qPrintable(QStringLiteral("%1, stubs %2, x %3: %4").arg(vertical).arg(stubs).arg(x).arg(rgb(got))));
            }
        }
    }
}

// "?" for Unknown and "FAULT" for Fault, in textColor() in the row of the tag; nothing there for the other statuses
void tst_ValveIndicator::statusTags()
{
    int unknownInk = 0;
    int faultInk = 0;
    for (ValveStatus s : kStatuses) {
        ValveIndicator v;
        prepare(v);
        v.setStatus(s);
        const QImage img = render(v);
        const QRectF tag = layoutOf(v).tag;
        const int ink = countIn(img, tag, [](QRgb p) { return near(p, kText, 90); });
        const int anything = countIn(img, tag, [](QRgb p) { return !near(p, kBackground, 0); });
        if (s == ValveStatus::Unknown) {
            unknownInk = ink;
        } else if (s == ValveStatus::Fault) {
            faultInk = ink;
        } else {
            QVERIFY2(anything == 0, qPrintable(nameOf(s) + QStringLiteral(": %1 pixels in the tag row").arg(anything)));
        }
        // Nothing of the tag above its row: it is below the symbol
        QVERIFY(countIn(img, QRectF(0.0, 0.0, v.width(), tag.top() - 1.0), [](QRgb p) { return near(p, kText, 90); }) == 0);
    }
    QVERIFY2(unknownInk > 0, "no ? tag");
    // FAULT is five letters: more ink than ?
    QVERIFY2(faultInk > 2 * unknownInk, qPrintable(QStringLiteral("FAULT %1, ? %2 ink pixels").arg(faultInk).arg(unknownInk)));
}

// A control valve always shows its opening; the other types when openingVisible() is set
void tst_ValveIndicator::openingShownForControlOrWhenAsked()
{
    for (ValveType type : kTypes) {
        for (bool visible : {false, true}) {
            ValveIndicator v;
            prepare(v, QSize(220, 260));
            v.setValveType(type);
            v.setOpeningVisible(visible);
            v.setOpening(50.0);
            const QImage img = render(v);
            const bool shown = type == ValveType::Control || visible;
            const internal::ValveLayout l = layoutOf(v);
            const QString where = nameOf(type) + QStringLiteral(", visible %1").arg(visible);
            QVERIFY2(l.pod.isEmpty() == !shown && l.bar.isEmpty() == !shown, qPrintable(where));
            QVERIFY2(readoutInk(v).isEmpty() == !shown, qPrintable(where));
            // Nothing in outlineColor() below the tag row without the opening
            if (!shown) {
                QCOMPARE(countIn(img, QRectF(0.0, l.tag.bottom(), v.width(), v.height()), [](QRgb p) { return !near(p, kBackground, 0); }), 0);
            }
        }
    }
}

void tst_ValveIndicator::openingBar_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<double>("fraction"); // -1: no bar at all
    QTest::newRow("0") << 0.0 << 0.0;
    QTest::newRow("25") << 25.0 << 0.25;
    QTest::newRow("62.5") << 62.5 << 0.625;
    QTest::newRow("100") << 100.0 << 1.0;
    QTest::newRow("overrange") << 150.0 << 1.0;
    QTest::newRow("+Inf") << kInf << 1.0;
    QTest::newRow("underrange") << -10.0 << 0.0;
    QTest::newRow("-Inf") << -kInf << 0.0;
    QTest::newRow("NaN") << kNaN << -1.0;
}

// The bar is filled from the left up to the opening (at the bound out of range); an invalid input draws no bar
void tst_ValveIndicator::openingBar()
{
    QFETCH(double, input);
    QFETCH(double, fraction);
    for (bool vertical : {false, true}) {
        ValveIndicator v;
        prepare(v, QSize(240, 260));
        v.setValveType(ValveType::Control);
        v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
        v.setStatus(ValveStatus::Open);
        v.setOpening(input);
        const QImage img = render(v);
        const QRectF bar = layoutOf(v).bar;
        QVERIFY(!bar.isEmpty());
        const int outline = countIn(img, bar, [](QRgb p) { return near(p, kOutline, 120); });
        if (fraction < 0.0) {
            QVERIFY2(outline == 0, qPrintable(QStringLiteral("%1 bar pixels for NaN").arg(outline)));
            continue;
        }
        // The frame: the left and right ends of the bar
        QVERIFY(near(at(img, QPointF(bar.left() + 0.5, bar.center().y())), kOutline, 120));
        QVERIFY(near(at(img, QPointF(bar.right() - 0.5, bar.center().y())), kOutline, 120));
        // Inside the frame, one row: filled before the opening, empty after it (2 px from the edge of the fill)
        const double y = bar.center().y();
        const double edge = bar.left() + bar.width() * fraction;
        for (double x = bar.left() + 2.0; x < bar.right() - 2.0; x += 1.0) {
            if (std::abs(x - edge) < 2.0) {
                continue;
            }
            const QRgb got = at(img, QPointF(x, y));
            const bool filled = x < edge;
            QVERIFY2(near(got, filled ? kOutline : kBackground),
                     qPrintable(QStringLiteral("x %1 (fill to %2): %3").arg(x).arg(edge).arg(rgb(got))));
        }
    }
}

void tst_ValveIndicator::readoutStates()
{
    QString error;
    QFont font = ReadoutTest::readoutTestFont(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // Twice the size (#51: the readout follows the widget font), so that each dash is several pixels wide
    font.setPixelSize(static_cast<int>(std::lround(2.0 * internal::effectivePixelSize(QApplication::font()))));
    const auto box = [](ValveIndicator &v) {
        const QRectF pod = layoutOf(v).pod.adjusted(1.5, 1.5, -1.5, -1.5);
        const double dpr = v.grab().devicePixelRatio();
        return QRectF(pod.topLeft() * dpr, pod.size() * dpr).toRect();
    };
    const auto make = [&](ValveIndicator &v, double input) {
        v.setFont(font);
        prepare(v, QSize(320, 360));
        v.setValveType(ValveType::Control);
        v.setOpening(input);
    };
    struct Case {
        double input;
        double plainInput; // a valid input with the same digits, less the triangle (and the minus sign)
        int extraGroups;
        bool up;
    };
    for (const Case &c : {Case{150.0, 100.0, 1, true}, Case{-50.0, 50.0, 2, false}}) {
        ValveIndicator marked;
        make(marked, c.input);
        const QImage mi = render(marked);
        ValveIndicator plain;
        make(plain, c.plainInput);
        const QImage pi = render(plain);
        const auto markedInk = ReadoutTest::readoutInkGroups(mi, box(marked));
        const auto plainInk = ReadoutTest::readoutInkGroups(pi, box(plain));
        const QString where = QStringLiteral("input %1").arg(c.input);
        QVERIFY2(!plainInk.isEmpty() && markedInk.size() == plainInk.size() + c.extraGroups,
                 qPrintable(where + QStringLiteral(": %1 ink groups, %2 for the valid text").arg(markedInk.size()).arg(plainInk.size())));
        const ReadoutTest::InkGroup &tri = markedInk.first();
        QVERIFY2(tri.top.size() >= 3, qPrintable(where));
        const int flat = c.up ? ReadoutTest::spread(tri.bottom) : ReadoutTest::spread(tri.top);
        const int pointed = c.up ? ReadoutTest::spread(tri.top) : ReadoutTest::spread(tri.bottom);
        QVERIFY2(flat <= 1 && pointed >= 2, qPrintable(where + QStringLiteral(": flat %1, pointed %2").arg(flat).arg(pointed)));
    }
    ValveIndicator invalid;
    make(invalid, kNaN);
    const QImage ii = render(invalid);
    const QString dashes = ReadoutTest::dashesError(ReadoutTest::readoutInkGroups(ii, box(invalid)));
    ValveIndicator never;
    never.setFont(font);
    prepare(never, QSize(320, 360));
    never.setValveType(ValveType::Control);
    const bool sameAsNaN = render(never) == ii;
    // Out of the font database again before the checks: without installed fonts the test font (digits only) would
    // stay the only font of the application in the tests that follow
    QFontDatabase::removeAllApplicationFonts();
    QVERIFY2(dashes.isEmpty(), qPrintable(dashes));
    QVERIFY2(sameAsNaN, "a new control valve is not drawn as one with a NaN opening");
}

void tst_ValveIndicator::textsInsideTheWidgetFromTheMinimumUp_data()
{
    addTypesAndOrientations();
}

// From minimumSizeHint() up, with the widest tag (FAULT) and a long readout: no pixel drawn on the outer pixel of the
// widget (a text cut by the edge would leave one), the tag drawn. A valid readout may be left out without room (#60,
// #167; readoutNeverBelowSevenPoints()), "---" never: it is drawn for a NaN opening at every size
void tst_ValveIndicator::textsInsideTheWidgetFromTheMinimumUp()
{
    QFETCH(ValveType, type);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    const auto configure = [&](ValveIndicator &v) {
        v.setValveType(type);
        v.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
        v.setPipeStubsVisible(stubs);
        v.setOpeningVisible(true);
        v.setPrecision(1);
    };
    ValveIndicator probe;
    configure(probe);
    const QSize min = probe.minimumSizeHint();
    for (int dw : {0, 1, 9, 40}) {
        for (int dh : {0, 1, 13, 50}) {
            ValveIndicator v;
            configure(v);
            prepare(v, min + QSize(dw, dh));
            v.setStatus(ValveStatus::Fault);
            v.setOpening(100.0);
            QCOMPARE(v.minimumSizeHint(), min);
            const QImage img = render(v);
            const QString where = QStringLiteral("%1x%2").arg(v.width()).arg(v.height());
            int edge = 0;
            for (int x = 0; x < img.width(); ++x) {
                edge += !near(img.pixel(x, 0), kBackground, 0) + !near(img.pixel(x, img.height() - 1), kBackground, 0);
            }
            for (int y = 0; y < img.height(); ++y) {
                edge += !near(img.pixel(0, y), kBackground, 0) + !near(img.pixel(img.width() - 1, y), kBackground, 0);
            }
            QVERIFY2(edge == 0, qPrintable(where + QStringLiteral(": %1 pixels drawn on the edge").arg(edge)));
            const internal::ValveLayout l = layoutOf(v);
            QVERIFY2(l.scale >= internal::kValveMinimumScale - 1e-9, qPrintable(where));
            const int tag = countIn(img, l.tag, [](QRgb p) { return near(p, kText, 90); });
            QVERIFY2(tag > 0, qPrintable(where + QStringLiteral(": no tag")));
            v.setOpening(kNaN);
            QVERIFY2(!readoutInk(v).isEmpty(), qPrintable(where + QStringLiteral(": no \"---\"")));
        }
    }
}

void tst_ValveIndicator::readoutNeverBelowSevenPoints_data()
{
    QTest::addColumn<int>("extraWidth");
    QTest::addColumn<double>("input");
    QTest::addColumn<int>("precision");
    for (int dw : {0, 3, 10, 25, 60}) {
        for (double v : {42.0, 100.0, 1e5, 1e9, 1e15, -1e15, -kInf, kInf, kNaN}) {
            for (int precision : {0, 2}) {
                QTest::newRow(qPrintable(QStringLiteral("+%1 px, %2, precision %3").arg(dw).arg(v).arg(precision)))
                    << dw << v << precision;
            }
        }
    }
}

// #60, #167: from the minimum width up the readout is never below 7 pt nor cut. When the number is shown its line is
// at least as wide as the number alone at 7 pt (the unit may be left out); otherwise only a valid readout may be
// missing, and an out-of-range one keeps a triangle not smaller than the 7 pt one. "---" is always whole
void tst_ValveIndicator::readoutNeverBelowSevenPoints()
{
    QFETCH(int, extraWidth);
    QFETCH(double, input);
    QFETCH(int, precision);
    ValveIndicator v;
    v.setValveType(ValveType::Control);
    v.setPrecision(precision);
    const QSize min = v.minimumSizeHint();
    prepare(v, QSize(min.width() + extraWidth, min.height() + 40));
    v.setOpening(input);
    const QVector<QPoint> ink = readoutInk(v);
    const ValueState state = v.openingState();
    const qreal dpr = v.grab().devicePixelRatio();
    const QRectF pod = layoutOf(v).pod;
    const QRect podDevice = QRectF(pod.topLeft() * dpr, pod.size() * dpr).toAlignedRect();
    for (const QPoint &p : ink) {
        QVERIFY2(podDevice.contains(p), "readout ink outside the panel");
    }
    const bool outOfRange = state == ValueState::Overrange || state == ValueState::Underrange;
    if (ink.isEmpty()) {
        QVERIFY2(state == ValueState::Valid, "only a valid readout may be left out");
        // The unit goes first: the number is left out only when it does not fit alone at 7 pt, between the margins
        // of the panel (max(2, 0.12 x its shorter side) on each side)
        QFont seven = v.font();
        seven.setBold(true);
        seven.setPixelSize(internal::minimumReadoutPixelSize(v.logicalDpiY()));
        const double advance = QFontMetricsF(seven, &v).horizontalAdvance(QString::number(input, 'f', precision));
        const double available = pod.width() - 2.0 * std::max(2.0, std::min(pod.width(), pod.height()) * 0.12);
        QVERIFY2(advance > available, qPrintable(QStringLiteral("no readout, but the number alone at 7 pt is %1 px for %2 px")
                                                     .arg(advance).arg(available)));
        return;
    }
    const QString number = state == ValueState::Invalid
        ? QStringLiteral("---")
        : (std::isinf(input) ? (input > 0 ? QLatin1Char('>') + QString::number(100.0, 'f', precision)
                                          : QLatin1Char('<') + QString::number(0.0, 'f', precision))
                             : QString::number(input, 'f', precision));
    const QSize full = sevenPointLineInk(number, outOfRange, v, dpr);
    const QRect line = boundsOf(ink);
    if (line.width() >= full.width() && line.height() >= full.height() - 1) {
        return;
    }
    QVERIFY2(outOfRange, qPrintable(QStringLiteral("readout %1 x %2 px, \"%3\" at 7 pt is %4 x %5 px")
                                        .arg(line.width()).arg(line.height()).arg(number).arg(full.width()).arg(full.height())));
    const QSize tri = sevenPointLineInk(QString(), true, v, dpr);
    QVERIFY2(line.width() >= tri.width() && line.height() >= tri.height(),
             qPrintable(QStringLiteral("readout %1 x %2 px: smaller than the 7 pt triangle (%3 x %4 px)")
                            .arg(line.width()).arg(line.height()).arg(tri.width()).arg(tri.height())));
}

// The minimum follows what it is made of, and a change of it reaches the layout (updateGeometry())
void tst_ValveIndicator::minimumSizeHintFollowsTheConfiguration()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *v = new ValveIndicator;
    layout->addWidget(v);
    ScreenChange::LayoutRequests requests;
    host.installEventFilter(&requests);
    host.show(); // a hidden widget does not tell its layout
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const QSize base = v->minimumSizeHint();
    const auto check = [&](const char *what, const std::function<void()> &change, int wider, int taller) {
        const QSize before = v->minimumSizeHint();
        QCoreApplication::processEvents();
        requests.count = 0;
        change();
        QCoreApplication::processEvents();
        const QSize after = v->minimumSizeHint();
        const int dw = after.width() > before.width() ? 1 : after.width() < before.width() ? -1 : 0;
        const int dh = after.height() > before.height() ? 1 : after.height() < before.height() ? -1 : 0;
        QVERIFY2(dw == wider && dh == taller,
                 qPrintable(QStringLiteral("%1: %2x%3 -> %4x%5").arg(QLatin1String(what)).arg(before.width())
                                .arg(before.height()).arg(after.width()).arg(after.height())));
        QVERIFY2(requests.count > 0, qPrintable(QStringLiteral("%1: the layout was not told").arg(QLatin1String(what))));
    };
    check("opening visible", [&] { v->setOpeningVisible(true); }, 0, 1);
    check("opening hidden", [&] { v->setOpeningVisible(false); }, 0, -1);
    check("control valve (actuator and opening)", [&] { v->setValveType(ValveType::Control); }, 0, 1);
    check("gate valve", [&] { v->setValveType(ValveType::Gate); }, 0, -1);
    check("butterfly valve (the disc is taller than the body)", [&] { v->setValveType(ValveType::Butterfly); }, 0, 1);
    check("gate valve again", [&] { v->setValveType(ValveType::Gate); }, 0, -1);
    check("pipe stubs", [&] { v->setPipeStubsVisible(true); }, 1, 0);
    check("vertical", [&] { v->setOrientation(Qt::Vertical); }, -1, 1);
    check("horizontal", [&] { v->setOrientation(Qt::Horizontal); }, 1, -1);
    v->setPipeStubsVisible(false);
    check("larger font", [&] { v->setFont(FontTest::doubledFont()); }, 1, 1);
    v->setFont(QFont());
    QCOMPARE(v->minimumSizeHint(), base);
    // The status and the opening do not change it
    v->setStatus(ValveStatus::Fault);
    v->setOpening(100.0);
    QCOMPARE(v->minimumSizeHint(), base);
    // sizeHint() is larger, and at least the minimum for any configuration
    QVERIFY(v->sizeHint().width() > base.width() && v->sizeHint().height() > base.height());
}

void tst_ValveIndicator::screenChangeOfAVisibleValveIndicator_data()
{
    ScreenChange::addScreens();
}

// #177: moved to a screen of another DPR or logical DPI, the valve draws and sizes itself as a new one there, and its
// layout gets the new minimum once
void tst_ValveIndicator::screenChangeOfAVisibleValveIndicator()
{
    QFETCH(double, dpr);
    QFETCH(int, dpi);
    const std::vector<std::pair<const char *, std::function<void(ValveIndicator &)>>> configurations = {
        {"default", [](ValveIndicator &) {}},
        {"fault", [](ValveIndicator &v) { v.setStatus(ValveStatus::Fault); }},
        {"control valve, 42 %", [](ValveIndicator &v) {
             v.setValveType(ValveType::Control);
             v.setStatus(ValveStatus::Open);
             v.setOpening(42.0);
         }},
        {"vertical ball valve, stubs, opening out of range", [](ValveIndicator &v) {
             v.setValveType(ValveType::Ball);
             v.setOrientation(Qt::Vertical);
             v.setPipeStubsVisible(true);
             v.setOpeningVisible(true);
             v.setStatus(ValveStatus::Transit);
             v.setOpening(130.0);
         }},
    };
    int changed = 0;
    for (const auto &configuration : configurations) {
        bool minimumChanged = false;
        ScreenChange::checkScreenChange<ValveIndicator>(configuration.second, dpr, dpi, &minimumChanged);
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

void tst_ValveIndicator::nonFiniteInput()
{
    ValveIndicator v;
    prepare(v);
    v.setValveType(ValveType::Control);
    for (QSize size : {QSize(200, 200), QSize(1, 1), QSize(0, 0), QSize(20, 300), QSize(300, 20)}) {
        v.resize(size);
        for (ValveStatus s : kStatuses) {
            v.setStatus(s);
            for (double o : {kInf, -kInf, kNaN, 1e308, -1e308}) {
                v.setOpening(o);
                (void)render(v);
                const internal::ValveLayout l = layoutOf(v);
                QVERIFY(std::isfinite(l.scale) && l.scale >= 0.0);
            }
        }
    }
    v.setOpening(1e308);
    QCOMPARE(v.opening(), 100.0);
    QCOMPARE(v.openingState(), ValueState::Overrange);
}

QTEST_MAIN(tst_ValveIndicator)
#include "tst_ValveIndicator.moc"
