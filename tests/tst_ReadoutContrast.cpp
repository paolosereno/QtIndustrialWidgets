// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #159: contrast of the digital readouts against their pod, with the default readout and zone colours, in the
// dark and light styling of the gallery, in every value state. The styling and the showcase background come from
// examples/gallery/GalleryTheme.cpp (#190), the code the live gallery and the image export use, so a colour changed
// in the gallery is measured here.
//
// Targets (WCAG 2.x): 4.5:1 for text (1.4.3, normal text: the readouts); 3:1 for graphical objects that carry
// meaning (1.4.11: the fill, the limit lines and the hatch of an invalid input drawn on the empty inside of
// TankDisplay, tankInterior()). The status tags of ValveIndicator are text on the background of the widget
// (valveStatusTag()): 4.5:1 against the showcase background; its body in each status colour is a graphical object
// on that background (valveStatusColours()): 3:1. Each is measured on the rendered pixels, with no tolerance.

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSlider.h>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/TankDisplay.h>
#include <QtIndustrialWidgets/ThumbwheelSwitch.h>
#include <QtIndustrialWidgets/ValveIndicator.h>

#include "GalleryTheme.h"
#include "internal/IndustrialKnobTestAccess.h"
#include "internal/IndustrialSliderTestAccess.h"
#include "internal/LinearGaugeScale.h"
#include "internal/TankDisplayTestAccess.h"
#include "internal/ThumbwheelSwitchTestAccess.h"
#include "internal/ValveIndicatorTestAccess.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QLineEdit>

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

GalleryShowcase::Theme themeOf(bool dark) { return dark ? GalleryShowcase::Theme::Dark : GalleryShowcase::Theme::Light; }

// The showcase background of the gallery export
QColor showcaseBackground(bool dark) { return GalleryShowcase::backgroundColor(themeOf(dark)); }

QImage renderOn(QWidget &w, bool dark)
{
    const QPixmap pix = w.grab();
    QImage img(pix.size(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(pix.devicePixelRatio());
    img.fill(showcaseBackground(dark));
    QPainter p(&img);
    p.drawPixmap(0, 0, pix);
    p.end();
    return img;
}

// #183: the focus indicator of \a control is a graphical object (WCAG 1.4.11, 2.4.7): its colour against what lies
// next to and under it, at 3:1. \a window, with the showcase background of the theme (a control that is not opaque
// shows it), holds \a control and \a other, which takes the focus first. The ring is what changes when \a control
// takes the focus; its colour is the most frequent colour of those pixels, and each of them is compared with the same
// pixel without focus
void checkFocusRingContrast(QWidget &window, QWidget &control, QWidget &other, bool dark)
{
    constexpr double kGraphicsRatio = 3.0;
    QPalette background = window.palette();
    background.setColor(QPalette::Window, showcaseBackground(dark));
    window.setPalette(background);
    window.setAutoFillBackground(true);
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    other.setFocus();
    QTRY_VERIFY(other.hasFocus());
    const QImage plain = renderOn(control, dark);
    control.setFocus();
    QTRY_VERIFY(control.hasFocus());
    const QImage focused = renderOn(control, dark);

    QHash<QRgb, int> count;
    QVector<QPoint> ring;
    for (int y = 0; y < plain.height(); ++y) {
        for (int x = 0; x < plain.width(); ++x) {
            if (plain.pixel(x, y) != focused.pixel(x, y)) {
                ring.push_back(QPoint(x, y));
                ++count[focused.pixel(x, y)];
            }
        }
    }
    QVERIFY2(!ring.isEmpty(), "no focus indicator");
    QRgb colour = 0;
    int most = -1;
    for (auto it = count.cbegin(); it != count.cend(); ++it) {
        if (it.value() > most) {
            most = it.value();
            colour = it.key();
        }
    }
    double worst = std::numeric_limits<double>::max();
    QRgb worstNext = 0;
    for (const QPoint &p : ring) {
        if (contrast(colour, plain.pixel(p)) < worst) {
            worst = contrast(colour, plain.pixel(p));
            worstNext = plain.pixel(p);
        }
    }
    QVERIFY2(worst >= kGraphicsRatio,
             qPrintable(QStringLiteral("focus ring rgb(%1, %2, %3) against rgb(%4, %5, %6): %7:1 (target %8:1)")
                            .arg(qRed(colour)).arg(qGreen(colour)).arg(qBlue(colour))
                            .arg(qRed(worstNext)).arg(qGreen(worstNext)).arg(qBlue(worstNext))
                            .arg(worst, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
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

// Gallery styling, from examples/gallery/GalleryTheme.cpp
template <typename Widget>
void style(Widget &w, bool dark)
{
    GalleryShowcase::applyGalleryTheme(w, themeOf(dark));
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
    void industrialKnobPadlock_data();
    void industrialKnobPadlock();
    void industrialKnobFocusRing_data();
    void industrialKnobFocusRing();
    void industrialSwitchFocusRing_data();
    void industrialSwitchFocusRing();
    void circularBargraph_data();
    void circularBargraph();
    void tankDisplay_data();
    void tankDisplay();
    void tankInterior_data();
    void tankInterior();
    void valveIndicator_data();
    void valveIndicator();
    void industrialSlider_data();
    void industrialSlider();
    void industrialSliderPanel_data();
    void industrialSliderPanel();
    void industrialSliderHatch_data();
    void industrialSliderHatch();
    void industrialSliderTrackOutline_data();
    void industrialSliderTrackOutline();
    void industrialSliderFocusRing_data();
    void industrialSliderFocusRing();
    void thumbwheelDigits_data();
    void thumbwheelDigits();
    void thumbwheelArrows_data();
    void thumbwheelArrows();
    void thumbwheelPanelMarks_data();
    void thumbwheelPanelMarks();
    void thumbwheelInputMarks_data();
    void thumbwheelInputMarks();
    void thumbwheelFocusRing_data();
    void thumbwheelFocusRing();
    void valveStatusTag_data();
    void valveStatusTag();
    void valveStatusColours_data();
    void valveStatusColours();
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

void tst_ReadoutContrast::tankDisplay_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<double>("value");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("%s valid", theme) << dark << 62.4;
        QTest::addRow("%s invalid", theme) << dark << kNaN;
        QTest::addRow("%s overrange", theme) << dark << 130.0;
        QTest::addRow("%s underrange", theme) << dark << -10.0;
    }
}

// #152: the readout of TankDisplay on its opaque panel, with a limit in force (it does not change the readout)
void tst_ReadoutContrast::tankDisplay()
{
    QFETCH(bool, dark);
    QFETCH(double, value);
    TankDisplay t;
    t.setLimits(5.0, 15.0, 85.0, 95.0);
    t.setValue(value);
    style(t, dark);
    t.resize(160, 260);
    const QImage img = renderOn(t, dark);
    checkReading(measureReadout(img, deviceRect(internal::TankDisplayTestAccess::lastLayout(t).pod, img.devicePixelRatio())),
                 scaledPixels(15, img.devicePixelRatio()));
}

void tst_ReadoutContrast::tankInterior_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<QColor>("vessel"); // set by the user over the theme; invalid: the theme's
    // The hatch is derived from the inside and must keep 3:1 for any vesselColor(); the fill and the limit lines
    // are in colours the user picks (contentColor(), limitColor()), checked with the defaults of the themes and
    // a mid-grey vessel only
    QTest::addColumn<bool>("hatchOnly");
    QTest::newRow("dark") << true << QColor() << false;
    QTest::newRow("light") << false << QColor() << false;
    QTest::newRow("light, mid-grey vessel") << false << QColor(128, 128, 128) << false;
    QTest::newRow("dark, mid-grey vessel") << true << QColor(128, 128, 128) << false;
    QTest::newRow("light, near-white vessel") << false << QColor(235, 235, 235) << true;
    QTest::newRow("dark, near-black vessel") << true << QColor(20, 20, 20) << true;
    QTest::newRow("light, saturated yellow vessel") << false << QColor(255, 220, 0) << true;
}

// #152: the empty inside of TankDisplay is a dark shade of vesselColor() in every theme. What is drawn on it must
// stand out in both stylings and with a vesselColor() set by the user: the fill, a limit line and the hatch of an
// invalid input, at the ratio for graphical objects, 3:1 (each against the most frequent colour of the inside)
void tst_ReadoutContrast::tankInterior()
{
    QFETCH(bool, dark);
    QFETCH(QColor, vessel);
    QFETCH(bool, hatchOnly);
    constexpr double kGraphicsRatio = 3.0;
    TankDisplay t;
    t.setLimits(kNaN, kNaN, 70.0, kNaN);
    t.setValue(40.0);
    style(t, dark);
    if (vessel.isValid()) {
        t.setVesselColor(vessel);
    }
    t.resize(160, 260);
    const auto mostFrequent = [](const QImage &img, const QRect &r) {
        QHash<QRgb, int> count;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = r.left(); x <= r.right(); ++x) {
                ++count[img.pixel(x, y)];
            }
        }
        QRgb best = 0;
        int most = -1;
        for (auto it = count.cbegin(); it != count.cend(); ++it) {
            if (it.value() > most) {
                most = it.value();
                best = it.key();
            }
        }
        return best;
    };
    const auto mostContrasting = [](const QImage &img, const QRect &r, QRgb against) {
        QRgb best = against;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = r.left(); x <= r.right(); ++x) {
                if (contrast(img.pixel(x, y), against) > contrast(best, against)) {
                    best = img.pixel(x, y);
                }
            }
        }
        return best;
    };
    const auto check = [&](const char *what, QRgb a, QRgb b) {
        const double ratio = contrast(a, b);
        QVERIFY2(ratio >= kGraphicsRatio,
                 qPrintable(QStringLiteral("%1: %2 against the empty inside rgb(%3, %4, %5): %6:1 (target %7:1)")
                                .arg(QLatin1String(what))
                                .arg(QStringLiteral("rgb(%1, %2, %3)").arg(qRed(a)).arg(qGreen(a)).arg(qBlue(a)))
                                .arg(qRed(b)).arg(qGreen(b)).arg(qBlue(b))
                                .arg(ratio, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
    };
    QImage img = renderOn(t, dark);
    qreal dpr = img.devicePixelRatio();
    const QRectF v = internal::TankDisplayTestAccess::lastLayout(t).vessel;
    // The middle 40 % of the width, clear of the wall; heights as fractions of the scale from the bottom
    const auto band = [&](double from, double to) {
        return deviceRect(QRectF(v.left() + v.width() * 0.3, v.bottom() - to * v.height(), v.width() * 0.4,
                                 (to - from) * v.height()), dpr);
    };
    const QRgb empty = mostFrequent(img, band(0.75, 0.85));
    if (!hatchOnly) {
        check("fill", mostFrequent(img, band(0.1, 0.3)), empty);
    }
    const double lineY = v.bottom() - 0.7 * v.height();
    const QRectF line(v.left() + v.width() * 0.25, lineY - 1.5, v.width() * 0.5, 3.0);
    if (!hatchOnly) {
        check("limit line", mostContrasting(img, QRectF(line.topLeft() * dpr, line.size() * dpr).toAlignedRect(), empty),
              empty);
    }
    t.setValue(kNaN);
    img = renderOn(t, dark);
    dpr = img.devicePixelRatio();
    const QRect inside = band(0.2, 0.6); // below the limit line
    const QRgb hatchedEmpty = mostFrequent(img, inside);
    QCOMPARE(hatchedEmpty, empty);
    check("hatch", mostContrasting(img, inside, hatchedEmpty), hatchedEmpty);
}

void tst_ReadoutContrast::valveIndicator_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<double>("value");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("%s valid", theme) << dark << 42.0;
        QTest::addRow("%s invalid", theme) << dark << kNaN;
        QTest::addRow("%s overrange", theme) << dark << 130.0;
        QTest::addRow("%s underrange", theme) << dark << -10.0;
    }
}

// #153: the opening readout of ValveIndicator on its opaque panel, in every state of the opening
void tst_ReadoutContrast::valveIndicator()
{
    QFETCH(bool, dark);
    QFETCH(double, value);
    ValveIndicator v;
    v.setValveType(ValveIndicator::ValveType::Control);
    v.setStatus(ValveIndicator::ValveStatus::Open);
    v.setOpening(value);
    style(v, dark);
    v.resize(180, 220);
    const QImage img = renderOn(v, dark);
    checkReading(measureReadout(img, deviceRect(internal::ValveIndicatorTestAccess::lastLayout(v).pod, img.devicePixelRatio())),
                 scaledPixels(15, img.devicePixelRatio()));
}

void tst_ReadoutContrast::industrialSlider_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<double>("feedback");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("%s tracking", theme) << dark << 58.0;
        QTest::addRow("%s deviating (triangle)", theme) << dark << 15.0;
        QTest::addRow("%s unknown (?)", theme) << dark << kNaN;
        QTest::addRow("%s overrange", theme) << dark << 120.0;
    }
}

// #180: the two readouts of IndustrialSlider on their opaque panels, with the deviation marks
void tst_ReadoutContrast::industrialSlider()
{
    QFETCH(bool, dark);
    QFETCH(double, feedback);
    IndustrialSlider s;
    s.setDeviationThreshold(5.0);
    s.setSetpoint(60.0);
    s.setFeedback(feedback);
    style(s, dark);
    s.resize(s.sizeHint());
    const QImage img = renderOn(s, dark);
    const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
    for (const QRectF &pod : {l.setpointPod, l.feedbackPod}) {
        checkReading(measureReadout(img, deviceRect(pod, img.devicePixelRatio())), scaledPixels(15, img.devicePixelRatio()));
    }
}

void tst_ReadoutContrast::industrialSliderPanel_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("vertical");
    for (const bool dark : {true, false}) {
        QTest::addRow("%s horizontal", dark ? "dark" : "light") << dark << false;
        QTest::addRow("%s vertical", dark ? "dark" : "light") << dark << true;
    }
}

// #180: IndustrialSlider draws its scale labels, its handle, its feedback bar and its zone band on the background of
// the widget, with the gallery styling: the labels are text (4.5:1), the others graphical objects (3:1), each
// measured on a pixel it covers fully, against the showcase background
void tst_ReadoutContrast::industrialSliderPanel()
{
    constexpr double kGraphicsRatio = 3.0;
    QFETCH(bool, dark);
    QFETCH(bool, vertical);
    IndustrialSlider s;
    s.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    s.setWarningThreshold(80.0);
    s.setErrorThreshold(90.0);
    s.setSetpoint(60.0);
    s.setFeedback(30.0);
    style(s, dark);
    s.resize(vertical ? QSize(s.sizeHint().width(), 300) : QSize(320, s.sizeHint().height()));
    // As the gallery export draws a showcase: onto the showcase background, without the window background
    const qreal scale = s.devicePixelRatioF();
    const QColor background = showcaseBackground(dark);
    QImage img(s.size() * scale, QImage::Format_ARGB32);
    img.setDevicePixelRatio(scale);
    img.fill(background);
    {
        QPainter painter(&img);
        s.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
    const qreal dpr = img.devicePixelRatio();
    const auto pixelAt = [&](double along, double across) {
        const QPointF p = vertical ? QPointF(across, along) : QPointF(along, across);
        return img.pixel(static_cast<int>(std::floor(p.x() * dpr)), static_cast<int>(std::floor(p.y() * dpr)));
    };
    const auto alongOf = [&](double v) { return l.along(v / 100.0); };
    const auto check = [&](const char *what, QRgb px) {
        const double ratio = contrast(px, background.rgb());
        QVERIFY2(ratio >= kGraphicsRatio,
                 qPrintable(QStringLiteral("%1 (%2,%3,%4) on (%5,%6,%7): %8:1, %9:1 needed")
                                .arg(QLatin1String(what)).arg(qRed(px)).arg(qGreen(px)).arg(qBlue(px))
                                .arg(background.red()).arg(background.green()).arg(background.blue())
                                .arg(ratio, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
    };
    check("handle", pixelAt(alongOf(60.0), l.handleBase + 2.0));
    check("feedback bar", pixelAt(alongOf(30.0), l.trackFar() + internal::kSliderBarOverhang * 0.5));
    const double zone = l.zoneNear + internal::kSliderZone * 0.5;
    check("normal zone", pixelAt(alongOf(40.0), zone));
    check("warning zone", pixelAt(alongOf(85.0), zone));
    check("error zone", pixelAt(alongOf(95.0), zone));
    // The labels: the room after labelNear, whose ends are background
    const QRectF room = vertical ? QRectF(l.labelNear, 0.0, s.width() - l.labelNear, s.height())
                                 : QRectF(0.0, l.labelNear, s.width(), s.height() - l.labelNear);
    const QRect box = QRectF(room.topLeft() * dpr, room.size() * dpr).toAlignedRect().intersected(img.rect());
    checkReading(measureReadout(img, box), scaledPixels(30, dpr));
}

void tst_ReadoutContrast::industrialSliderHatch_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<QColor>("track"); // set by the user over the default; invalid: the default
    QTest::newRow("dark") << true << QColor();
    QTest::newRow("light") << false << QColor();
    QTest::newRow("mid-grey track") << false << QColor(128, 128, 128);
    QTest::newRow("near-white track") << false << QColor(235, 235, 235);
    QTest::newRow("saturated yellow track") << false << QColor(255, 220, 0);
}

// #180: both hatches of IndustrialSlider (an invalid feedback, a deviation) are derived from trackColor(), not from
// the text colour, and stand out on the track at 3:1 (graphical objects) for any track colour: the most contrasting
// pixel of the hatched track against the track as drawn without a hatch (the same area with a feedback at the
// setpoint). Whether a pixel reaches the colour of a thin line depends on where the line falls on the pixel grid:
// four widths and four heights move the track and the lines by fractions of a pixel. Not against the most frequent colour of the hatched area: on the narrow track the hatch covers about
// half the pixels, and an antialiased shade can be as frequent as the track
void tst_ReadoutContrast::industrialSliderHatch()
{
    constexpr double kGraphicsRatio = 3.0;
    QFETCH(bool, dark);
    QFETCH(QColor, track);
    for (int k = 0; k < 4; ++k) {
        IndustrialSlider s;
        s.setDeviationThreshold(5.0);
        s.setSetpoint(90.0);
        style(s, dark);
        if (track.isValid()) {
            s.setTrackColor(track);
        }
        s.resize(320 + k, s.sizeHint().height() + k);
        const auto coloursOf = [&](double feedback) {
            s.setFeedback(feedback);
            const QImage img = renderOn(s, dark);
            const qreal dpr = img.devicePixelRatio();
            const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
            // Between the bar (at 10) and the handle (at 90), 1 px inside the outline of the track
            const QRectF area(l.along(0.2), l.track.top() + 1.0, l.along(0.8) - l.along(0.2), l.track.height() - 2.0);
            const QRect r = QRectF(area.topLeft() * dpr, area.size() * dpr).toAlignedRect();
            QHash<QRgb, int> count;
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    ++count[img.pixel(x, y)];
                }
            }
            return count;
        };
        const QHash<QRgb, int> plain = coloursOf(90.0);
        QCOMPARE(s.deviationState(), IndustrialSlider::DeviationState::Normal);
        QRgb base = 0;
        int most = -1;
        for (auto it = plain.cbegin(); it != plain.cend(); ++it) {
            if (it.value() > most) {
                most = it.value();
                base = it.key();
            }
        }
        for (const double feedback : {kNaN, 10.0}) {
            const QHash<QRgb, int> hatched = coloursOf(feedback);
            QRgb best = base;
            for (auto it = hatched.cbegin(); it != hatched.cend(); ++it) {
                if (contrast(it.key(), base) > contrast(best, base)) {
                    best = it.key();
                }
            }
            const double ratio = contrast(best, base);
            QVERIFY2(ratio >= kGraphicsRatio,
                     qPrintable(QStringLiteral("%1, %10x%11: hatch (%2,%3,%4) on the track (%5,%6,%7): "
                                                    "%8:1, %9:1 needed")
                                    .arg(std::isnan(feedback) ? QStringLiteral("invalid") : QStringLiteral("deviation"))
                                    .arg(qRed(best)).arg(qGreen(best)).arg(qBlue(best))
                                    .arg(qRed(base)).arg(qGreen(base)).arg(qBlue(base))
                                    .arg(ratio, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)
                                    .arg(s.width()).arg(s.height())));
        }
    }
}

void tst_ReadoutContrast::industrialSliderTrackOutline_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("vertical");
    for (const bool dark : {true, false}) {
        QTest::addRow("%s horizontal", dark ? "dark" : "light") << dark << false;
        QTest::addRow("%s vertical", dark ? "dark" : "light") << dark << true;
    }
}

// #180: the outline of the track of IndustrialSlider (scaleColor().darker(160), 1 px) is what shows where the track
// is on the panel: in the dark gallery theme the default track (30, 36, 45) is almost the background (24, 29, 39). A
// graphical object, 3:1 against the showcase background, with the gallery styling. Measured on the two long sides of
// the track, clear of the handle and of the ends: the most contrasting pixel within half a pixel of each side,
// against the background. A 1 px line on a fractional coordinate covers no pixel fully, so four widths and heights
// move the track by fractions of a pixel, and every one must reach the target
void tst_ReadoutContrast::industrialSliderTrackOutline()
{
    constexpr double kGraphicsRatio = 3.0;
    QFETCH(bool, dark);
    QFETCH(bool, vertical);
    for (int k = 0; k < 4; ++k) {
        IndustrialSlider s;
        s.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
        s.setSetpoint(95.0); // the handle at the far end, clear of the measured part
        style(s, dark);
        s.resize(vertical ? QSize(s.sizeHint().width() + k, 300 + k) : QSize(320 + k, s.sizeHint().height() + k));
        // As the gallery export draws a showcase: onto the showcase background, without the window background
        const qreal scale = s.devicePixelRatioF();
        const QColor background = showcaseBackground(dark);
        QImage img(s.size() * scale, QImage::Format_ARGB32);
        img.setDevicePixelRatio(scale);
        img.fill(background);
        {
            QPainter painter(&img);
            s.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
        }
        const qreal dpr = img.devicePixelRatio();
        const internal::SliderLayout l = internal::IndustrialSliderTestAccess::lastLayout(s);
        // From 10 % to 80 % of the scale
        const double a0 = std::min(l.along(0.1), l.along(0.8));
        const double a1 = std::max(l.along(0.1), l.along(0.8));
        for (const double side : {l.trackNear(), l.trackFar()}) {
            const QRectF strip = vertical ? QRectF(side - 0.5, a0, 1.0, a1 - a0) : QRectF(a0, side - 0.5, a1 - a0, 1.0);
            const QRect r = QRectF(strip.topLeft() * dpr, strip.size() * dpr).toAlignedRect().intersected(img.rect());
            QRgb best = background.rgb();
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    if (contrast(img.pixel(x, y), background.rgb()) > contrast(best, background.rgb())) {
                        best = img.pixel(x, y);
                    }
                }
            }
            const double ratio = contrast(best, background.rgb());
            QVERIFY2(ratio >= kGraphicsRatio,
                     qPrintable(QStringLiteral("%1 side, %2x%3: outline (%4,%5,%6) on (%7,%8,%9): %10:1, %11:1 needed")
                                    .arg(side == l.trackNear() ? QStringLiteral("near") : QStringLiteral("far"))
                                    .arg(s.width()).arg(s.height())
                                    .arg(qRed(best)).arg(qGreen(best)).arg(qBlue(best))
                                    .arg(background.red()).arg(background.green()).arg(background.blue())
                                    .arg(ratio, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
        }
    }
}

namespace {

void addThemeRows()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("dark") << true;
    QTest::newRow("light") << false;
}

// As the gallery export draws a showcase: onto the showcase background, without the window background of the widget
QImage renderShowcase(QWidget &w, bool dark)
{
    const qreal scale = w.devicePixelRatioF();
    QImage img(w.size() * scale, QImage::Format_ARGB32);
    img.setDevicePixelRatio(scale);
    img.fill(showcaseBackground(dark));
    QPainter painter(&img);
    w.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    return img;
}

QRect deviceBox(const QRectF &logical, const QImage &img)
{
    const qreal dpr = img.devicePixelRatio();
    return QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect().intersected(img.rect());
}

QRgb pixelAt(const QImage &img, const QPointF &p)
{
    const qreal dpr = img.devicePixelRatio();
    return img.pixel(std::clamp(static_cast<int>(std::floor(p.x() * dpr)), 0, img.width() - 1),
                     std::clamp(static_cast<int>(std::floor(p.y() * dpr)), 0, img.height() - 1));
}

// The pixel of \a box (device px) of highest contrast against \a against
QRgb strongestIn(const QImage &img, const QRect &box, QRgb against)
{
    QRgb best = against;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            if (contrast(img.pixel(x, y), against) > contrast(best, against)) {
                best = img.pixel(x, y);
            }
        }
    }
    return best;
}

void checkGraphics(const char *what, QRgb ink, QRgb under)
{
    constexpr double kGraphicsRatio = 3.0;
    const double ratio = contrast(ink, under);
    QVERIFY2(ratio >= kGraphicsRatio, qPrintable(QStringLiteral("%1 (%2,%3,%4) on (%5,%6,%7): %8:1, %9:1 needed")
                                                     .arg(QLatin1String(what)).arg(qRed(ink)).arg(qGreen(ink))
                                                     .arg(qBlue(ink)).arg(qRed(under)).arg(qGreen(under))
                                                     .arg(qBlue(under)).arg(ratio, 0, 'f', 2)
                                                     .arg(kGraphicsRatio, 0, 'f', 1)));
}

} // namespace

void tst_ReadoutContrast::thumbwheelDigits_data() { addThemeRows(); }

// #154: the digits and the sign of ThumbwheelSwitch are text on the drum faces (4.5:1), with the gallery styling
void tst_ReadoutContrast::thumbwheelDigits()
{
    QFETCH(bool, dark);
    ThumbwheelSwitch t;
    t.setSignDrumVisible(true);
    t.setRange(-9999.0, 9999.0);
    t.setValue(-1250.0);
    style(t, dark);
    t.resize(t.sizeHint());
    const QImage img = renderShowcase(t, dark);
    for (const QRectF &drum : internal::ThumbwheelSwitchTestAccess::lastLayout(t).drums) {
        checkReading(measureReadout(img, deviceRect(drum, img.devicePixelRatio())), scaledPixels(8, img.devicePixelRatio()));
    }
}

void tst_ReadoutContrast::thumbwheelArrows_data() { addThemeRows(); }

// #154: the arrows of the buttons carry whether a step is applied: graphical objects on the button face (3:1), filled
// and outlined
void tst_ReadoutContrast::thumbwheelArrows()
{
    QFETCH(bool, dark);
    ThumbwheelSwitch t;
    t.setValue(909.0); // + filled on the drums at 0, outlined on those at 9; − the other way round
    style(t, dark);
    t.resize(t.sizeHint());
    const QImage img = renderShowcase(t, dark);
    const internal::ThumbwheelLayout l = internal::ThumbwheelSwitchTestAccess::lastLayout(t);
    const QVector<bool> open = internal::ThumbwheelSwitchTestAccess::lastPlusOpen(t);
    QCOMPARE(open, (QVector<bool>{true, false, true, false}));
    for (int i = 0; i < l.drums.size(); ++i) {
        for (const QRectF &button : {l.plus.at(i), l.minus.at(i)}) {
            // The face: beside the arrow, at mid height, clear of the rounded corners
            const QRgb face = pixelAt(img, QPointF(button.left() + 3.0, button.center().y()));
            const QRect box = deviceBox(button.adjusted(2.0, 2.0, -2.0, -2.0), img);
            const bool filled = open.at(i) == (button == l.plus.at(i));
            checkGraphics(filled ? "filled arrow" : "outlined arrow", strongestIn(img, box, face), face);
            // The arrow is seen as a shape, not as a few pixels at its corners: at least half of its perimeter (a
            // triangle of base 2a and height a, a as in drawArrow()) in pixels that reach the target
            const double side = std::min(button.width() * 0.25, button.height() * 0.4);
            const double perimeter = (2.0 + 2.0 * std::sqrt(2.0)) * side * img.devicePixelRatio();
            int strong = 0;
            for (int y = box.top(); y <= box.bottom(); ++y) {
                for (int x = box.left(); x <= box.right(); ++x) {
                    strong += contrast(img.pixel(x, y), face) >= 3.0 ? 1 : 0;
                }
            }
            QVERIFY2(strong >= 0.5 * perimeter, qPrintable(QStringLiteral("%1 arrow: %2 pixels at 3:1, %3 needed")
                                                                .arg(QLatin1String(filled ? "filled" : "outlined"))
                                                                .arg(strong).arg(0.5 * perimeter, 0, 'f', 1)));
        }
    }
}

void tst_ReadoutContrast::thumbwheelPanelMarks_data() { addThemeRows(); }

// #154: the unit and the OVF tag are text on the background of the widget (4.5:1 against the showcase background);
// the decimal point and the ▲ / ▼ marks are graphical objects there (3:1)
void tst_ReadoutContrast::thumbwheelPanelMarks()
{
    QFETCH(bool, dark);
    ThumbwheelSwitch t;
    t.setDigitCount(5);
    t.setDecimals(1);
    t.setUnit(QStringLiteral("rpm"));
    t.setRange(0.0, 1500.0);
    t.setValue(1820.0);
    style(t, dark);
    t.resize(t.sizeHint());
    const QRgb background = showcaseBackground(dark).rgb();
    QImage img = renderShowcase(t, dark);
    const internal::ThumbwheelLayout l = internal::ThumbwheelSwitchTestAccess::lastLayout(t);
    QCOMPARE(internal::ThumbwheelSwitchTestAccess::lastTag(t), QStringLiteral("▲"));
    // The text boxes widened into the gaps around them, whose ends are background
    checkReading(measureReadout(img, deviceBox(l.unit.adjusted(-3.0, 0.0, 3.0, 0.0), img)),
                 scaledPixels(20, img.devicePixelRatio()));
    checkGraphics("decimal point", pixelAt(img, l.decimalPoint.center()), background);
    checkGraphics("▲", strongestIn(img, deviceBox(l.tag, img), background), background);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be shown")));
    t.setValue(1e9);
    img = renderShowcase(t, dark);
    QCOMPARE(internal::ThumbwheelSwitchTestAccess::lastTag(t), QStringLiteral("OVF"));
    checkReading(measureReadout(img, deviceBox(l.tag.adjusted(-3.0, 0.0, 3.0, 0.0), img)),
                 scaledPixels(20, img.devicePixelRatio()));
}

void tst_ReadoutContrast::thumbwheelInputMarks_data() { addThemeRows(); }

// #154, part 2, with the gallery styling: the dashed outline of a pending value on the drum face (a graphical object,
// 3:1), the SET button filled (text on the button face, 4.5:1) and outlined (text on the background, 4.5:1), the status
// line (text on the background, 4.5:1) and the padlock of read-only (a graphical object on the background, 3:1)
void tst_ReadoutContrast::thumbwheelInputMarks()
{
    QFETCH(bool, dark);
    ThumbwheelSwitch t;
    t.setDigitCount(5);
    t.setDecimals(1);
    t.setUnit(QStringLiteral("V"));
    t.setValue(125.0);
    style(t, dark);
    t.resize(t.sizeHint());
    const QRgb background = showcaseBackground(dark).rgb();
    QImage img = renderShowcase(t, dark);
    const internal::ThumbwheelLayout l = internal::ThumbwheelSwitchTestAccess::lastLayout(t);
    QVERIFY(!internal::ThumbwheelSwitchTestAccess::lastSetOpen(t));
    checkReading(measureReadout(img, deviceBox(l.setButton.adjusted(2.0, 2.0, -2.0, -2.0), img)),
                 scaledPixels(15, img.devicePixelRatio()));

    QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier); // The first decade: a pending value
    QCoreApplication::sendEvent(&t, &up);
    QVERIFY(t.hasPendingValue());
    img = renderShowcase(t, dark);
    QVERIFY(internal::ThumbwheelSwitchTestAccess::lastSetOpen(t));
    for (const QRectF &drum : l.drums) {
        const QRgb face = t.drumColor().rgb(); // Opaque and flat (#159)
        checkGraphics("dashed outline", strongestIn(img, deviceBox(QRectF(drum.left(), drum.top(), drum.width(), 2.0), img),
                                                    face), face);
    }
    checkReading(measureReadout(img, deviceBox(l.setButton.adjusted(2.0, 2.0, -2.0, -2.0), img)),
                 scaledPixels(15, img.devicePixelRatio()));
    checkReading(measureReadout(img, deviceBox(l.status.adjusted(0.0, 0.0, 3.0, 0.0), img)),
                 scaledPixels(40, img.devicePixelRatio()));

    t.setReadOnly(true);
    img = renderShowcase(t, dark);
    QVERIFY(internal::ThumbwheelSwitchTestAccess::lastPadlock(t));
    checkGraphics("padlock", strongestIn(img, deviceBox(l.padlock, img), background), background);
}

void tst_ReadoutContrast::thumbwheelFocusRing_data() { addThemeRows(); }

// #154, #183: the focus ring and the bar of the current decade, in textColor(), against what is under them
void tst_ReadoutContrast::thumbwheelFocusRing()
{
    QFETCH(bool, dark);
    QWidget window;
    auto *t = new ThumbwheelSwitch(&window);
    auto *other = new QLineEdit(&window);
    t->setDigitCount(5);
    t->setDecimals(1);
    t->setValue(125.0);
    style(*t, dark);
    t->resize(t->sizeHint());
    other->setGeometry(0, t->height() + 10, 120, 30);
    window.resize(std::max(t->width(), 120), t->height() + 50);
    checkFocusRingContrast(window, *t, *other, dark);
}

void tst_ReadoutContrast::valveStatusTag_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<int>("status");
    QTest::addColumn<int>("minPixels");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        QTest::addRow("%s unknown (?)", theme) << dark << static_cast<int>(ValveIndicator::ValveStatus::Unknown) << 8;
        QTest::addRow("%s fault (FAULT)", theme) << dark << static_cast<int>(ValveIndicator::ValveStatus::Fault) << 30;
    }
}

// #153: the status tags of ValveIndicator are text drawn on the background of the widget, not on a panel: textColor()
// against the showcase background of each theme, with the gallery styling
void tst_ReadoutContrast::valveStatusTag()
{
    QFETCH(bool, dark);
    QFETCH(int, status);
    QFETCH(int, minPixels);
    ValveIndicator v;
    v.setStatus(static_cast<ValveIndicator::ValveStatus>(status));
    style(v, dark);
    v.resize(180, 160);
    // As the gallery export draws a showcase (examples/gallery/GalleryExport.cpp): onto the showcase background,
    // without the window background of the widget (grab() would paint the default palette under the tag)
    const qreal scale = v.devicePixelRatioF();
    QImage img(v.size() * scale, QImage::Format_ARGB32);
    img.setDevicePixelRatio(scale);
    img.fill(showcaseBackground(dark));
    {
        QPainter painter(&img);
        v.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    const QRectF tag = internal::ValveIndicatorTestAccess::lastLayout(v).tag;
    const qreal dpr = img.devicePixelRatio();
    // The whole row (no inset: the row is as tall as the text line); its ends are background
    const QRect box = QRectF(tag.topLeft() * dpr, tag.size() * dpr).toAlignedRect().intersected(img.rect());
    checkReading(measureReadout(img, box), scaledPixels(minPixels, dpr));
}

void tst_ReadoutContrast::valveStatusColours_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<int>("status");
    const auto statuses = QMetaEnum::fromType<ValveIndicator::ValveStatus>();
    for (const bool dark : {true, false}) {
        for (int i = 0; i < statuses.keyCount(); ++i) {
            QTest::addRow("%s %s", dark ? "dark" : "light", statuses.key(i)) << dark << statuses.value(i);
        }
    }
}

// #153: the body of ValveIndicator in each status colour, with the gallery styling, against the showcase background
// of each theme: 3:1 (graphical objects). Measured on a pixel the body covers fully: the fill of a closed valve and
// of the upstream half in transit, the solid outline (the downstream base, 2 px wide) of the other statuses
void tst_ReadoutContrast::valveStatusColours()
{
    constexpr double kGraphicsRatio = 3.0;
    QFETCH(bool, dark);
    QFETCH(int, status);
    const auto s = static_cast<ValveIndicator::ValveStatus>(status);
    ValveIndicator v;
    v.setStatus(s);
    v.setPipeStubsVisible(true);
    style(v, dark);
    v.resize(180, 160);
    const qreal scale = v.devicePixelRatioF();
    const QColor background = showcaseBackground(dark);
    QImage img(v.size() * scale, QImage::Format_ARGB32);
    img.setDevicePixelRatio(scale);
    img.fill(background);
    {
        QPainter painter(&img);
        v.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    const bool filled = s == ValveIndicator::ValveStatus::Closed || s == ValveIndicator::ValveStatus::Transit;
    const QPointF unit = filled ? QPointF(-0.5, 0.0) : QPointF(1.0, 0.3);
    const QPointF at = internal::ValveIndicatorTestAccess::lastLayout(v).transform.map(unit) * scale;
    const QRgb px = img.pixel(static_cast<int>(std::floor(at.x())), static_cast<int>(std::floor(at.y())));
    const double ratio = contrast(px, background.rgb());
    QVERIFY2(ratio >= kGraphicsRatio,
             qPrintable(QStringLiteral("body (%1,%2,%3) on (%4,%5,%6): %7:1, %8:1 needed")
                            .arg(qRed(px)).arg(qGreen(px)).arg(qBlue(px))
                            .arg(background.red()).arg(background.green()).arg(background.blue())
                            .arg(ratio, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
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

void tst_ReadoutContrast::industrialSwitchFocusRing_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("dark") << true;
    QTest::newRow("light") << false;
}

// #183: the focus ring of the switch against what is under it without focus, at least 3:1, in both gallery themes
void tst_ReadoutContrast::industrialSwitchFocusRing()
{
    QFETCH(bool, dark);
    QWidget window;
    auto *sw = new IndustrialSwitch(&window);
    auto *other = new QLineEdit(&window);
    sw->setAnimated(false);
    sw->setLabel(QStringLiteral("MAIN POWER"));
    sw->setGeometry(0, 0, 120, 190);
    other->setGeometry(0, 200, 120, 30);
    window.resize(120, 240);
    checkFocusRingContrast(window, *sw, *other, dark);
}

void tst_ReadoutContrast::industrialSliderFocusRing_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("vertical");
    for (const bool dark : {true, false}) {
        QTest::addRow("%s horizontal", dark ? "dark" : "light") << dark << false;
        QTest::addRow("%s vertical", dark ? "dark" : "light") << dark << true;
    }
}

// #180, #183: the focus ring of IndustrialSlider (textColor(), 2 px along the edge) against what is under it without
// focus, at least 3:1, in both gallery themes
void tst_ReadoutContrast::industrialSliderFocusRing()
{
    QFETCH(bool, dark);
    QFETCH(bool, vertical);
    QWidget window;
    auto *s = new IndustrialSlider(&window);
    auto *other = new QLineEdit(&window);
    s->setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    s->setSetpoint(40.0);
    s->setFeedback(45.0);
    style(*s, dark);
    const QSize size = vertical ? QSize(140, 300) : QSize(300, 92);
    s->setGeometry(0, 0, size.width(), size.height());
    other->setGeometry(0, size.height() + 10, 120, 30);
    window.resize(size.width(), size.height() + 50);
    checkFocusRingContrast(window, *s, *other, dark);
}

void tst_ReadoutContrast::industrialKnobFocusRing_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("dark") << true;
    QTest::newRow("light") << false;
}

// #183: the focus indicator is a graphical object (WCAG 1.4.11, 2.4.7): its colour against what lies next to and
// under it, at 3:1, in both gallery themes (checkFocusRingContrast())
void tst_ReadoutContrast::industrialKnobFocusRing()
{
    QFETCH(bool, dark);
    QWidget window;
    auto *k = new IndustrialKnob(&window);
    auto *other = new QLineEdit(&window);
    k->setRange(0.0, 8000.0);
    k->setValue(2400.0);
    style(*k, dark);
    k->setGeometry(0, 0, 220, 240);
    other->setGeometry(0, 250, 220, 30);
    window.resize(220, 290);
    checkFocusRingContrast(window, *k, *other, dark);
}

void tst_ReadoutContrast::industrialKnobPadlock_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<double>("value");
    for (const bool dark : {true, false}) {
        const char *theme = dark ? "dark" : "light";
        for (const double value : {0.0, 2400.0, 8000.0}) {
            QTest::addRow("%s, value %g", theme, value) << dark << value;
        }
    }
}

// #182: the padlock of read-only is a graphical object that carries the state: textColor() against every pixel of
// the cap around it, inside its rectangle, at 3:1 (WCAG 1.4.11) in both gallery themes. Its pixels are those that change
// with textColor() inside its rectangle; the others there are the cap
void tst_ReadoutContrast::industrialKnobPadlock()
{
    QFETCH(bool, dark);
    QFETCH(double, value);
    constexpr double kGraphicsRatio = 3.0;
    IndustrialKnob k;
    k.setRange(0.0, 8000.0);
    k.setValue(value);
    style(k, dark);
    k.resize(220, 240);
    k.setReadOnly(true);
    const QColor text = k.textColor();
    const QImage img = renderOn(k, dark);
    k.setTextColor(QColor(255 - text.red(), 255 - text.green(), 255 - text.blue()));
    const QImage other = renderOn(k, dark);
    k.setTextColor(text);

    const qreal dpr = img.devicePixelRatio();
    const QRectF logical = internal::IndustrialKnobTestAccess::padlockRect(k);
    QVERIFY(!logical.isEmpty());
    const QRect box = QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect();
    int ink = 0;
    int solid = 0;
    int cap = 0;
    double worst = std::numeric_limits<double>::max();
    QRgb worstCap = 0;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            const QRgb p = img.pixel(x, y);
            if (p != other.pixel(x, y)) {
                ++ink;
                solid += channelDistance(p, text.rgb()) <= 2 ? 1 : 0;
                continue;
            }
            ++cap;
            if (contrast(text.rgb(), p) < worst) {
                worst = contrast(text.rgb(), p);
                worstCap = p;
            }
        }
    }
    QVERIFY2(ink > 0 && solid > 0, qPrintable(QStringLiteral("%1 padlock pixels, %2 of them in textColor").arg(ink).arg(solid)));
    QVERIFY2(cap > 0, "no cap pixel around the padlock");
    QVERIFY2(worst >= kGraphicsRatio,
             qPrintable(QStringLiteral("padlock rgb(%1, %2, %3) against the cap rgb(%4, %5, %6): %7:1 (target %8:1)")
                            .arg(text.red()).arg(text.green()).arg(text.blue())
                            .arg(qRed(worstCap)).arg(qGreen(worstCap)).arg(qBlue(worstCap))
                            .arg(worst, 0, 'f', 2).arg(kGraphicsRatio, 0, 'f', 1)));
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
    style(b, dark);
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

// #159: the property of the widgets whose readout once used textColor(), and of TankDisplay (#152) and
// ValveIndicator (#153)
void tst_ReadoutContrast::readoutColorProperty()
{
    checkReadoutColorProperty<RadialGauge>(QColor(245, 246, 250));
    checkReadoutColorProperty<LinearGauge>(QColor(240, 244, 248));
    checkReadoutColorProperty<Compass>(QColor(225, 231, 236));
    checkReadoutColorProperty<TankDisplay>(QColor(240, 244, 248));
    checkReadoutColorProperty<ValveIndicator>(QColor(240, 244, 248));
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
    QTest::newRow("TankDisplay valid") << QStringLiteral("TankDisplay") << 47.0;
    QTest::newRow("TankDisplay invalid") << QStringLiteral("TankDisplay") << kNaN;
    QTest::newRow("TankDisplay overrange") << QStringLiteral("TankDisplay") << 130.0;
    QTest::newRow("ValveIndicator valid") << QStringLiteral("ValveIndicator") << 47.0;
    QTest::newRow("ValveIndicator invalid") << QStringLiteral("ValveIndicator") << kNaN;
    QTest::newRow("ValveIndicator overrange") << QStringLiteral("ValveIndicator") << 130.0;
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
    } else if (widget == QLatin1String("TankDisplay")) {
        TankDisplay t;
        t.setValue(value);
        t.setTextColor(text);
        t.setScaleColor(text);
        t.setReadoutColor(readout);
        t.resize(160, 260);
        img = renderOn(t, true);
        pod = deviceRect(internal::TankDisplayTestAccess::lastLayout(t).pod, img.devicePixelRatio());
    } else if (widget == QLatin1String("ValveIndicator")) {
        ValveIndicator v;
        v.setValveType(ValveIndicator::ValveType::Control);
        v.setStatus(ValveIndicator::ValveStatus::Fault); // the FAULT tag in textColor()
        v.setOpening(value);
        v.setTextColor(text);
        v.setOutlineColor(text);
        v.setReadoutColor(readout);
        v.resize(180, 220);
        img = renderOn(v, true);
        pod = deviceRect(internal::ValveIndicatorTestAccess::lastLayout(v).pod, img.devicePixelRatio());
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
