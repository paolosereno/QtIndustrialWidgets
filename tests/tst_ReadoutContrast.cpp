// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #159: contrast of the digital readouts against their pod, with the default readout and zone colours, in the
// dark and light styling of the gallery (examples/gallery/Showcases.cpp, MainWindow::applyTheme()), in every
// value state. The target is the WCAG 2.x ratio for normal text, 4.5:1.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/RadialGauge.h>

#include "internal/LinearGaugeScale.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

using namespace QtIndustrialWidgets;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kMinimumRatio = 4.5;

double linear(int c8)
{
    const double c = c8 / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

// WCAG 2.x relative luminance and contrast ratio
double luminance(QRgb p) { return 0.2126 * linear(qRed(p)) + 0.7152 * linear(qGreen(p)) + 0.0722 * linear(qBlue(p)); }
double contrast(QRgb a, QRgb b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

// The showcase background of the gallery export
QImage renderOn(QWidget &w, bool dark)
{
    const QPixmap pix = w.grab();
    QImage img(pix.size(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(pix.devicePixelRatio());
    img.fill(dark ? QColor(0x18, 0x1d, 0x27) : QColor(0xff, 0xff, 0xff));
    QPainter p(&img);
    p.drawPixmap(0, 0, pix);
    p.end();
    return img;
}

struct Reading {
    double ratio = 0.0; // the most contrasting text pixel against the lightest or darkest pod row under the text
    int textPixels = 0;
    QRgb text = 0;
    QRgb pod = 0;       // the pod row that gives the ratio
};

// Contrast of the readout drawn inside \a inner (device px, a rectangle inside the pod, clear of its border).
// The pods are flat or vertical gradients and the readout is centred: the pod colour of a row is the most
// frequent colour of the two edge strips of the row. A row whose edge strips are covered by the text (far from
// the median pod colour) takes the pod colour of the nearest other row. Text pixels differ from the pod colour by
// more than 4 in a channel
Reading measureReadout(const QImage &img, const QRect &inner, const std::function<bool(int, int)> &inside = {})
{
    struct Row {
        int y;
        QVector<int> xs;
        QRgb pod;
        bool ok;
    };
    const int strip = std::max(2, static_cast<int>(std::round(2 * img.devicePixelRatio())));
    QVector<Row> rows;
    for (int y = inner.top(); y <= inner.bottom(); ++y) {
        Row row{y, {}, 0, true};
        for (int x = inner.left(); x <= inner.right(); ++x) {
            if (!inside || inside(x, y)) {
                row.xs.push_back(x);
            }
        }
        if (row.xs.size() < 4 * strip) {
            continue;
        }
        QHash<QRgb, int> count;
        for (int i = 0; i < strip; ++i) {
            ++count[img.pixel(row.xs[i], y)];
            ++count[img.pixel(row.xs[row.xs.size() - 1 - i], y)];
        }
        int most = -1;
        for (auto it = count.cbegin(); it != count.cend(); ++it) {
            if (it.value() > most) {
                most = it.value();
                row.pod = it.key();
            }
        }
        rows.push_back(row);
    }
    if (rows.isEmpty()) {
        return {};
    }
    QVector<QRgb> pods;
    for (const Row &row : rows) {
        pods.push_back(row.pod);
    }
    std::sort(pods.begin(), pods.end(), [](QRgb a, QRgb b) { return luminance(a) < luminance(b); });
    const QRgb median = pods.at(pods.size() / 2);
    for (Row &row : rows) {
        row.ok = channelDistance(row.pod, median) <= 24;
    }
    for (Row &row : rows) {
        if (row.ok) {
            continue;
        }
        int nearest = std::numeric_limits<int>::max();
        for (const Row &other : rows) {
            if (other.ok && std::abs(other.y - row.y) < nearest) {
                nearest = std::abs(other.y - row.y);
                row.pod = other.pod;
            }
        }
    }

    Reading r;
    double best = 0.0;
    QVector<QRgb> podsUnderText;
    for (const Row &row : rows) {
        bool hasText = false;
        for (int x : row.xs) {
            const QRgb p = img.pixel(x, row.y);
            if (channelDistance(p, row.pod) <= 4) {
                continue;
            }
            hasText = true;
            ++r.textPixels;
            const double c = contrast(p, row.pod);
            if (c > best) {
                best = c;
                r.text = p;
            }
        }
        if (hasText) {
            podsUnderText.push_back(row.pod);
        }
    }
    r.ratio = std::numeric_limits<double>::max();
    for (QRgb pod : podsUnderText) {
        const double c = contrast(r.text, pod);
        if (c < r.ratio) {
            r.ratio = c;
            r.pod = pod;
        }
    }
    return r;
}

QRect deviceRect(const QRectF &logical, qreal dpr)
{
    const int inset = static_cast<int>(std::ceil(3.0 * dpr));
    return QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect().adjusted(inset, inset, -inset, -inset);
}

// The bounding box of the largest 4-connected region where \a a and \a b differ
QRect largestDifference(const QImage &a, const QImage &b)
{
    const int w = a.width();
    const int h = a.height();
    QVector<char> differs(w * h, 0);
    QVector<char> seen(w * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            differs[y * w + x] = a.pixel(x, y) != b.pixel(x, y);
        }
    }
    QRect best;
    int bestSize = 0;
    for (int i = 0; i < w * h; ++i) {
        if (!differs[i] || seen[i]) {
            continue;
        }
        QVector<int> stack{i};
        seen[i] = 1;
        int size = 0;
        QRect box;
        while (!stack.isEmpty()) {
            const int j = stack.takeLast();
            const int x = j % w;
            const int y = j / w;
            ++size;
            box = box.isNull() ? QRect(x, y, 1, 1) : box.united(QRect(x, y, 1, 1));
            const int next[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
            for (const auto &n : next) {
                if (n[0] < 0 || n[0] >= w || n[1] < 0 || n[1] >= h) {
                    continue;
                }
                const int k = n[1] * w + n[0];
                if (differs[k] && !seen[k]) {
                    seen[k] = 1;
                    stack.push_back(k);
                }
            }
        }
        if (size > bestSize) {
            bestSize = size;
            best = box;
        }
    }
    return best;
}

QString describe(const Reading &r)
{
    return QStringLiteral("contrast %1:1 (text (%2,%3,%4) on pod (%5,%6,%7)), %8 text pixels")
        .arg(r.ratio, 0, 'f', 2)
        .arg(qRed(r.text)).arg(qGreen(r.text)).arg(qBlue(r.text))
        .arg(qRed(r.pod)).arg(qGreen(r.pod)).arg(qBlue(r.pod))
        .arg(r.textPixels);
}

// The readout is found (at least \a minPixels text pixels, in device px) and reaches the target
void checkReading(const Reading &r, int minPixels)
{
    QVERIFY2(r.textPixels >= minPixels, qPrintable(describe(r) + QStringLiteral(", %1 needed").arg(minPixels)));
    QVERIFY2(r.ratio >= kMinimumRatio, qPrintable(describe(r)));
}

int scaledPixels(int logical, qreal dpr) { return static_cast<int>(logical * dpr * dpr); }

// Gallery styling (examples/gallery/Showcases.cpp)
void style(RadialGauge &g, bool dark)
{
    if (dark) {
        g.setDialColor(QColor(20, 24, 32));
        g.setTextColor(QColor(245, 246, 250));
        g.setBezelColor(QColor(48, 56, 70));
    } else {
        g.setDialColor(QColor(242, 244, 248));
        g.setTextColor(QColor(30, 39, 46));
        g.setScaleColor(QColor(80, 90, 105));
        g.setBezelColor(QColor(200, 208, 218));
    }
}

void style(LinearGauge &g, bool dark)
{
    if (dark) {
        g.setBezelColor(QColor(36, 43, 56));
        g.setTroughColor(QColor(20, 24, 32));
        g.setTextColor(QColor(245, 246, 250));
    } else {
        g.setBezelColor(QColor(215, 222, 230));
        g.setTroughColor(QColor(235, 240, 245));
        g.setTextColor(QColor(30, 39, 46));
        g.setScaleColor(QColor(80, 90, 105));
    }
}

void style(Compass &c, bool dark)
{
    if (dark) {
        c.setDialColor(QColor(20, 24, 32));
        c.setBezelColor(QColor(48, 56, 70));
        c.setTextColor(QColor(225, 231, 236));
    } else {
        c.setDialColor(QColor(242, 244, 248));
        c.setBezelColor(QColor(200, 208, 218));
        c.setTextColor(QColor(30, 39, 46));
    }
}

void style(IndustrialKnob &k, bool dark)
{
    if (dark) {
        k.setKnobColor(QColor(42, 48, 60));
        k.setScaleColor(QColor(190, 200, 215));
        k.setTextColor(QColor(240, 244, 250));
    } else {
        k.setKnobColor(QColor(220, 226, 235));
        k.setScaleColor(QColor(70, 80, 95));
        k.setTextColor(QColor(30, 39, 46));
    }
}

// Rows for both themes: a value in the normal, warning and error zones, an invalid input (NaN), and an input
// above and below the range
void addGaugeRows(double normal, double warning, double error, double over, double under)
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<double>("value");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("%s normal", theme) << dark << normal;
        QTest::addRow("%s warning", theme) << dark << warning;
        QTest::addRow("%s error", theme) << dark << error;
        QTest::addRow("%s invalid", theme) << dark << kNaN;
        QTest::addRow("%s overrange", theme) << dark << over;
        QTest::addRow("%s underrange", theme) << dark << under;
    }
}
} // namespace

class tst_ReadoutContrast : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void radialGauge_data();
    void radialGauge();
    void linearGauge_data();
    void linearGauge();
    void compass_data();
    void compass();
    void industrialKnob_data();
    void industrialKnob();
    void circularBargraph_data();
    void circularBargraph();
    void readoutColorProperty();
    void readoutFollowsReadoutColorNotTextColor_data();
    void readoutFollowsReadoutColorNotTextColor();
};

void tst_ReadoutContrast::radialGauge_data()
{
    addGaugeRows(2000.0, 6400.0, 7500.0, 9000.0, -250.0);
}

void tst_ReadoutContrast::radialGauge()
{
    QFETCH(bool, dark);
    QFETCH(double, value);
    auto make = [&](RadialGauge &g, bool display) {
        g.setRange(0.0, 8000.0);
        g.setWarningThreshold(6000.0);
        g.setErrorThreshold(7000.0);
        g.setPrecision(0);
        g.setUnit(QStringLiteral("RPM"));
        g.setValue(value);
        g.setDigitalDisplayVisible(display);
        style(g, dark);
        g.resize(300, 300);
    };
    RadialGauge g;
    make(g, true);
    RadialGauge withoutDisplay;
    make(withoutDisplay, false);
    const QImage img = renderOn(g, dark);
    // The pod: the largest region drawn only with the digital display
    const int inset = static_cast<int>(std::ceil(3.0 * img.devicePixelRatio()));
    const QRect pod = largestDifference(img, renderOn(withoutDisplay, dark));
    QVERIFY(!pod.isEmpty());
    checkReading(measureReadout(img, pod.adjusted(inset, inset, -inset, -inset)),
                 scaledPixels(40, img.devicePixelRatio()));
}

void tst_ReadoutContrast::linearGauge_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<double>("value");
    for (const bool vertical : {true, false}) {
        for (const bool dark : {true, false}) {
            const char *name = vertical ? "vertical" : "horizontal";
            const char *theme = dark ? "dark" : "light";
            QTest::addRow("%s %s normal", name, theme) << dark << vertical << 60.0;
            QTest::addRow("%s %s warning", name, theme) << dark << vertical << 100.0;
            QTest::addRow("%s %s error", name, theme) << dark << vertical << 112.0;
            QTest::addRow("%s %s invalid", name, theme) << dark << vertical << kNaN;
            QTest::addRow("%s %s overrange", name, theme) << dark << vertical << 130.0;
            QTest::addRow("%s %s underrange", name, theme) << dark << vertical << -10.0;
        }
    }
}

void tst_ReadoutContrast::linearGauge()
{
    QFETCH(bool, dark);
    QFETCH(bool, vertical);
    QFETCH(double, value);
    LinearGauge g;
    g.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    g.setThermometerMode(vertical);
    g.setRange(0.0, 120.0);
    g.setWarningThreshold(95.0);
    g.setErrorThreshold(108.0);
    g.setPrecision(1);
    g.setUnit(QStringLiteral("°C"));
    g.setValue(value);
    style(g, dark);
    const QSize size = vertical ? QSize(120, 320) : QSize(420, 110);
    g.resize(size);
    const QImage img = renderOn(g, dark);
    const internal::GaugeRect r = internal::linearGaugeDisplayRect(vertical, size.width(), size.height());
    checkReading(measureReadout(img, deviceRect(QRectF(r.x, r.y, r.w, r.h), img.devicePixelRatio())),
                 scaledPixels(15, img.devicePixelRatio()));
}

void tst_ReadoutContrast::compass_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("headingUp");
    QTest::addColumn<double>("heading");
    for (const bool headingUp : {true, false}) {
        for (const bool dark : {true, false}) {
            const char *mode = headingUp ? "heading-up" : "north-up";
            const char *theme = dark ? "dark" : "light";
            QTest::addRow("%s %s valid", mode, theme) << dark << headingUp << 47.0;
            QTest::addRow("%s %s invalid", mode, theme) << dark << headingUp << kNaN;
        }
    }
}

void tst_ReadoutContrast::compass()
{
    QFETCH(bool, dark);
    QFETCH(bool, headingUp);
    QFETCH(double, heading);
    Compass c;
    c.setDisplayMode(headingUp ? Compass::DisplayMode::HeadingUp : Compass::DisplayMode::NorthUp);
    style(c, dark);
    c.setHeading(heading);
    c.resize(300, 300);
    const QImage img = renderOn(c, dark);
    // As in Compass::paintEvent()
    const double radius = std::min(c.width(), c.height()) / 2.0 - 6.0;
    const double podW = std::clamp(radius * 0.52, 48.0, 110.0);
    const double podH = std::clamp(radius * 0.26, 24.0, 50.0);
    const QPointF centre(c.rect().center());
    checkReading(measureReadout(img, deviceRect(QRectF(centre.x() - podW / 2.0, centre.y() - podH / 2.0, podW, podH),
                                                img.devicePixelRatio())),
                 scaledPixels(30, img.devicePixelRatio()));
}

void tst_ReadoutContrast::industrialKnob_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("discrete");
    QTest::addColumn<double>("value");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("continuous %s value", theme) << dark << false << 2400.0;
        QTest::addRow("continuous %s above range", theme) << dark << false << 9000.0;
        QTest::addRow("continuous %s invalid", theme) << dark << false << kNaN;
        QTest::addRow("discrete %s value", theme) << dark << true << 25.0;
    }
}

void tst_ReadoutContrast::industrialKnob()
{
    QFETCH(bool, dark);
    QFETCH(bool, discrete);
    QFETCH(double, value);
    IndustrialKnob k;
    if (discrete) {
        k.setMode(IndustrialKnob::KnobMode::Discrete);
        k.setDiscreteSteps(5);
    } else {
        k.setRange(0.0, 8000.0);
        k.setPrecision(0);
        k.setUnit(QStringLiteral("RPM"));
    }
    k.setValue(value);
    style(k, dark);
    k.resize(220, 240);
    const QImage img = renderOn(k, dark);
    // As in IndustrialKnob::renderStaticScale()
    const double w = k.width();
    const double h = k.height();
    const double podW = std::clamp(w * 0.65, 50.0, 110.0);
    const double podH = 22.0;
    checkReading(measureReadout(img, deviceRect(QRectF((w - podW) * 0.5, h - podH - 2.0, podW, podH),
                                                img.devicePixelRatio())),
                 scaledPixels(15, img.devicePixelRatio()));
}

void tst_ReadoutContrast::circularBargraph_data()
{
    addGaugeRows(65.0, 75.0, 90.0, 120.0, -10.0);
}

void tst_ReadoutContrast::circularBargraph()
{
    QFETCH(bool, dark);
    QFETCH(double, value);
    CircularBargraph b;
    b.setRange(0.0, 100.0);
    b.setSegmentCount(24);
    b.setWarningThreshold(70.0);
    b.setErrorThreshold(85.0);
    b.setPeakHoldEnabled(true);
    b.setUnit(QStringLiteral("%"));
    b.setValue(82.0);
    b.setValue(value);
    b.setDialColor(dark ? QColor(0, 0, 0) : QColor(24, 28, 36)); // as in the gallery
    b.resize(260, 260);
    const QImage img = renderOn(b, dark);
    // The pod well, as in CircularBargraph::paintEvent(): a circle of 0.9 x the inner radius of the segments
    const qreal dpr = img.devicePixelRatio();
    const double radius = std::min(b.width(), b.height()) / 2.0 - 4.0;
    const QPointF centre(b.width() * 0.5 * dpr, b.height() * 0.5 * dpr);
    const double r = radius * 0.66 * 0.90 * dpr - std::ceil(3.0 * dpr);
    const QRect box = QRectF(centre.x() - r, centre.y() - r, 2.0 * r, 2.0 * r).toAlignedRect();
    auto inWell = [&](int x, int y) { return std::hypot(x + 0.5 - centre.x(), y + 0.5 - centre.y()) <= r; };
    checkReading(measureReadout(img, box, inWell), scaledPixels(40, dpr));
}

namespace {
// readoutColor(): default, setter, property and appearanceChanged(), emitted once per change
template <typename Widget>
void checkReadoutColorProperty(const QColor &defaultColor)
{
    Widget w;
    QCOMPARE(w.readoutColor(), defaultColor);
    QCOMPARE(w.property("readoutColor").template value<QColor>(), defaultColor);
    QSignalSpy spy(&w, &Widget::appearanceChanged);
    w.setReadoutColor(QColor(10, 20, 30));
    QCOMPARE(w.readoutColor(), QColor(10, 20, 30));
    QCOMPARE(spy.count(), 1);
    w.setReadoutColor(QColor(10, 20, 30));
    QCOMPARE(spy.count(), 1);
    QVERIFY(w.setProperty("readoutColor", QColor(40, 50, 60)));
    QCOMPARE(w.readoutColor(), QColor(40, 50, 60));
    QCOMPARE(spy.count(), 2);
    // textColor() is independent
    QCOMPARE(w.textColor(), defaultColor);
}

bool isMagenta(QRgb p) { return qRed(p) > 200 && qBlue(p) > 200 && qGreen(p) < 60; }
bool isPureGreen(QRgb p) { return qGreen(p) > 200 && qRed(p) < 60 && qBlue(p) < 60; }

int countIn(const QImage &img, const QRect &box, bool (*pred)(QRgb))
{
    int n = 0;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            n += pred(img.pixel(x, y)) ? 1 : 0;
        }
    }
    return n;
}
} // namespace

// #159: the new property of the three widgets whose readout used textColor()
void tst_ReadoutContrast::readoutColorProperty()
{
    checkReadoutColorProperty<RadialGauge>(QColor(245, 246, 250));
    checkReadoutColorProperty<LinearGauge>(QColor(240, 244, 248));
    checkReadoutColorProperty<Compass>(QColor(225, 231, 236));
}

void tst_ReadoutContrast::readoutFollowsReadoutColorNotTextColor_data()
{
    QTest::addColumn<QString>("widget");
    QTest::addColumn<double>("value");
    QTest::newRow("RadialGauge valid") << QStringLiteral("RadialGauge") << 50.0;
    QTest::newRow("RadialGauge invalid") << QStringLiteral("RadialGauge") << kNaN;
    QTest::newRow("RadialGauge underrange") << QStringLiteral("RadialGauge") << -20.0;
    QTest::newRow("LinearGauge invalid") << QStringLiteral("LinearGauge") << kNaN;
    QTest::newRow("Compass valid") << QStringLiteral("Compass") << 47.0;
    QTest::newRow("Compass invalid") << QStringLiteral("Compass") << kNaN;
}

// #159: with textColor() pure green and readoutColor() magenta, the readout is magenta and has no green
void tst_ReadoutContrast::readoutFollowsReadoutColorNotTextColor()
{
    QFETCH(QString, widget);
    QFETCH(double, value);
    const QColor text(0, 255, 0);
    const QColor readout(255, 0, 255);
    QImage img;
    QRect pod;
    if (widget == QLatin1String("RadialGauge")) {
        auto make = [&](RadialGauge &g, bool display) {
            g.setRange(0.0, 100.0);
            g.setWarningThreshold(1000.0); // the whole range is the normal zone
            g.setErrorThreshold(2000.0);
            g.setThresholdBandsVisible(false);
            g.setValue(value);
            g.setTextColor(text);
            g.setReadoutColor(readout);
            g.setDigitalDisplayVisible(display);
            g.resize(300, 300);
        };
        RadialGauge g;
        make(g, true);
        RadialGauge withoutDisplay;
        make(withoutDisplay, false);
        img = renderOn(g, true);
        pod = largestDifference(img, renderOn(withoutDisplay, true));
    } else if (widget == QLatin1String("LinearGauge")) {
        LinearGauge g;
        g.setOrientation(Qt::Vertical);
        g.setValue(value);
        g.setTextColor(text);
        g.setReadoutColor(readout);
        g.resize(120, 320);
        img = renderOn(g, true);
        const internal::GaugeRect r = internal::linearGaugeDisplayRect(true, 120, 320);
        pod = deviceRect(QRectF(r.x, r.y, r.w, r.h), img.devicePixelRatio());
    } else {
        Compass c;
        c.setHeading(value);
        c.setTextColor(text);
        c.setCardinalColor(text);
        c.setReadoutColor(readout);
        c.resize(300, 300);
        img = renderOn(c, true);
        const double radius = 150.0 - 6.0;
        const double podW = std::clamp(radius * 0.52, 48.0, 110.0);
        const double podH = std::clamp(radius * 0.26, 24.0, 50.0);
        const QPointF centre(c.rect().center());
        pod = deviceRect(QRectF(centre.x() - podW / 2.0, centre.y() - podH / 2.0, podW, podH), img.devicePixelRatio());
    }
    QVERIFY(!pod.isEmpty());
    const int magenta = countIn(img, pod, isMagenta);
    const int green = countIn(img, pod, isPureGreen);
    QVERIFY2(magenta >= scaledPixels(10, img.devicePixelRatio()),
             qPrintable(QStringLiteral("%1 readout pixels in readoutColor()").arg(magenta)));
    QVERIFY2(green == 0, qPrintable(QStringLiteral("%1 readout pixels in textColor()").arg(green)));
}

QTEST_MAIN(tst_ReadoutContrast)
#include "tst_ReadoutContrast.moc"
