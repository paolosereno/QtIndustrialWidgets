// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "QtIndustrialWidgets/Compass.h"

#include "internal/Compass_p.h"
#include "internal/RadialGaugeLayout.h"

#include <QtCore/QHash>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QPolygonF>
#include <QtGui/QTransform>
#include <QtGui/QMouseEvent>
#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace QtIndustrialWidgets {

namespace {

// The geometry of the card for a side, around the centre: the same formulas for the drawing and for the layout (#226)
struct CardGeometry {
    double radius;
    double outerTickR;
    double majorTickR; // the inner end of the major ticks, where a circle is drawn
    double textR;      // the centre of the ring labels
    QRectF pod;        // the readout pod, without its 1.5 px border
    QRectF flag;       // the HDG flag of an invalid heading (#151), without its 1 px border

    explicit CardGeometry(double side)
        : radius(side / 2.0 - 6.0)
        , outerTickR(radius - 3.0)
        , majorTickR(radius - 3.0 - radius * 0.12)
        , textR(radius - 3.0 - radius * 0.22)
    {
        const double podW = std::clamp(radius * 0.52, 48.0, 110.0);
        const double podH = std::clamp(radius * 0.26, 24.0, 50.0);
        pod = QRectF(-podW / 2.0, -podH / 2.0, podW, podH);
        const double flagW = std::clamp(radius * 0.42, 34.0, 90.0);
        const double flagH = std::clamp(radius * 0.17, 14.0, 30.0);
        flag = QRectF(-flagW / 2.0, -radius * 0.45 - flagH / 2.0, flagW, flagH);
    }
};

// #226: the layout of the card. All lengths in logical px, around the centre of the card
constexpr double kMinimumPt = 7.0;   // the smallest text of the library
constexpr double kLabelGap = 3.0;    // between two labels, and between a label and the pod or the flag
constexpr double kTickGap = 4.0;     // between a label and the circle of the major ticks
constexpr double kReadoutPad = 1.0;  // between the readout and the inside of the pod border
constexpr double kPodBorder = 0.75;  // half of the 1.5 px pod border
constexpr double kFlagBorder = 0.5;  // half of the 1 px flag border
constexpr double kCirclePen = 0.5;   // half of the 1 px circle at majorTickR
// The rasterization margin: reserved on each side of the ink box of a text measured with QFontMetricsF in the
// containers that must hold that ink, the pod and the HDG flag (#226). The glyphs are drawn beyond the tight box that
// Qt reports: by up to ~0.8 logical px per side with CoreText (macOS) and 0.5 px with the Windows rasterizer, measured
// in the CI probe of #226. The gaps around the ring labels (kLabelGap, kTickGap) are measured on the metrics of Qt
// without it
constexpr double kRasterMargin = 1.0;
constexpr int kPreferredSide = 260;
constexpr int kLargestMinimum = 4096;

enum class LabelKind { Cardinal, Intercardinal, Number };

struct RingLabel {
    int angle; // degrees clockwise from North
    LabelKind kind;
};

// N, 030, NE, 060, E, …: in angle order, so that each label is next to its neighbours
const std::vector<RingLabel> &ringLabels()
{
    static const std::vector<RingLabel> labels{
        {0, LabelKind::Cardinal},     {30, LabelKind::Number},  {45, LabelKind::Intercardinal},
        {60, LabelKind::Number},      {90, LabelKind::Cardinal}, {120, LabelKind::Number},
        {135, LabelKind::Intercardinal}, {150, LabelKind::Number}, {180, LabelKind::Cardinal},
        {210, LabelKind::Number},     {225, LabelKind::Intercardinal}, {240, LabelKind::Number},
        {270, LabelKind::Cardinal},   {300, LabelKind::Number}, {315, LabelKind::Intercardinal},
        {330, LabelKind::Number}};
    return labels;
}

QString labelText(int angle)
{
    switch (angle) {
    case 0: return QStringLiteral("N");
    case 45: return QStringLiteral("NE");
    case 90: return QStringLiteral("E");
    case 135: return QStringLiteral("SE");
    case 180: return QStringLiteral("S");
    case 225: return QStringLiteral("SW");
    case 270: return QStringLiteral("W");
    case 315: return QStringLiteral("NW");
    default: return QStringLiteral("%1").arg(angle, 3, 10, QLatin1Char('0'));
    }
}

bool isShown(const RingLabel &label, const CompassCardLayout &layout)
{
    switch (label.kind) {
    case LabelKind::Cardinal: return true;
    case LabelKind::Intercardinal: return layout.intercardinals;
    case LabelKind::Number:
        switch (layout.numbers) {
        case CompassCardLayout::Numbers::All: return true;
        case CompassCardLayout::Numbers::EveryOther: return label.angle % 90 == 30; // 030, 120, 210, 300
        case CompassCardLayout::Numbers::None: return false;
        }
    }
    return false;
}

QFont textFont(const QFont &widgetFont, bool bold, double pointSize)
{
    QFont f = widgetFont;
    f.setBold(bold);
    f.setPointSizeF(pointSize);
    return f;
}

QFont labelFont(const QFont &widgetFont, LabelKind kind, const CompassCardLayout &layout)
{
    switch (kind) {
    case LabelKind::Cardinal: return textFont(widgetFont, true, layout.cardinalPt);
    case LabelKind::Intercardinal: return textFont(widgetFont, true, layout.intercardinalPt);
    case LabelKind::Number: break;
    }
    return textFont(widgetFont, false, layout.numberPt);
}

// The baseline origin of \a text drawn centred on (0, 0), as Qt::AlignCenter does: by its advance and its font height
QPointF textOrigin(const QFontMetricsF &fm, const QString &text)
{
    return {-fm.horizontalAdvance(text) / 2.0, -fm.height() / 2.0 + fm.ascent()};
}

// \a ink with the rasterization margin on each side; nothing drawn stays nothing
QRectF withRasterMargin(const QRectF &ink)
{
    return ink.isEmpty() ? ink : ink.adjusted(-kRasterMargin, -kRasterMargin, kRasterMargin, kRasterMargin);
}

// The ink of \a text drawn centred on (0, 0): the union of the glyph ink at the advance of each glyph, which keeps the
// letter spacing also where the platform has no fonts and draws boxes
QRectF textInk(const QFontMetricsF &fm, const QString &text)
{
    const QPointF origin = textOrigin(fm, text);
    QRectF ink;
    for (int i = 0; i < text.size(); ++i) {
        const QRectF glyph = fm.tightBoundingRect(text.mid(i, 1));
        if (glyph.isEmpty()) continue;
        ink |= glyph.translated(origin.x() + fm.horizontalAdvance(text.left(i)), origin.y());
    }
    return ink;
}

// The ink of the widest readout: every heading 000° … 359° and "---°", centred on (0, 0). Measured once per point size
// for the font and DPI of \a cache; the advances of the prefixes and the ink of each character are measured once in a
// measure, since the 361 texts share them
QRectF readoutInk(const QFont &widgetFont, double pointSize, const QPaintDevice *device, CompassLayoutCache &cache)
{
    const auto cached = cache.readoutInks.find(pointSize);
    if (cached != cache.readoutInks.end()) return cached->second;

    const QFontMetricsF fm(textFont(widgetFont, true, pointSize), device);
    QHash<QString, double> advances;
    QHash<QChar, QRectF> glyphs;
    const auto advance = [&](const QString &text) {
        const auto it = advances.constFind(text);
        return it != advances.constEnd() ? *it : *advances.insert(text, fm.horizontalAdvance(text));
    };
    const auto glyph = [&](QChar c) {
        const auto it = glyphs.constFind(c);
        return it != glyphs.constEnd() ? *it : *glyphs.insert(c, fm.tightBoundingRect(QString(c)));
    };
    // As textInk(): the union of the glyph ink at the advance of each glyph, the ink centred horizontally (as
    // readoutInkCentre() draws it)
    const auto inkOf = [&](const QString &text) {
        const double x0 = -advance(text) / 2.0;
        const double y0 = -fm.height() / 2.0 + fm.ascent();
        QRectF ink;
        for (int i = 0; i < text.size(); ++i) {
            const QRectF g = glyph(text.at(i));
            if (g.isEmpty()) continue;
            ink |= g.translated(x0 + (i == 0 ? 0.0 : advance(text.left(i))), y0);
        }
        return ink.translated(-ink.center().x(), 0.0);
    };
    QRectF ink = inkOf(QStringLiteral("---°"));
    for (int heading = 0; heading < 360; ++heading) {
        ink |= inkOf(QStringLiteral("%1°").arg(heading, 3, 10, QLatin1Char('0')));
    }
    cache.readoutInks.emplace(pointSize, ink);
    return ink;
}

// The centre of the ink of the readout \a text drawn with \a font at \a dpr, in device px from a baseline origin on a
// whole device pixel (#226). It is the ink of the text rendered so: the metrics of Qt round the tight box of a glyph
// out to whole px, and do not see the letter spacing around the box drawn for a missing glyph; and the glyphs are
// placed on whole device px, so that the origin is drawn on one too. \a cache keeps the measure of each text (at most
// the 360 headings and "---°") for one font, DPR and logical DPI: the text is rendered once, not at every paint
double readoutInkCentre(const QFont &font, const QPaintDevice *device, qreal dpr, const QString &text,
                        CompassReadoutOrigin &cache)
{
    if (cache.font != font || cache.dpr != dpr || cache.dpi != device->logicalDpiY()) {
        cache.font = font;
        cache.dpr = dpr;
        cache.dpi = device->logicalDpiY();
        cache.inkCentres.clear();
    }
    const auto cached = cache.inkCentres.constFind(text);
    if (cached != cache.inkCentres.constEnd()) return *cached;
    const QFontMetricsF fm(font, device);
    // The origin at (margin, margin + ascent), margin on a whole device pixel, in an image large enough for any
    // glyph overhang
    const int margin = static_cast<int>(std::ceil(fm.height() * dpr));
    const QSize size(static_cast<int>(std::ceil(fm.horizontalAdvance(text) * dpr)) + 2 * margin,
                     static_cast<int>(std::ceil(fm.height() * dpr)) + 2 * margin);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont f = font; // its point size at the logical DPI of the widget
        f.setPointSizeF(font.pointSizeF() * device->logicalDpiY() / static_cast<double>(image.logicalDpiY()));
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(QPointF(margin / dpr, margin / dpr + fm.ascent()), text);
    }
    int left = image.width();
    int right = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) != 0) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    }
    // Nothing drawn: the advance
    const double inkCentre = right < left ? fm.horizontalAdvance(text) * dpr / 2.0 : (left + right + 1) / 2.0 - margin;
    cache.inkCentres.insert(text, inkCentre);
    return inkCentre;
}

// The flag clear of the pod by kLabelGap. With a font that has letters the ring labels set a larger minimum; without
// them (a font without N, E, S, W) this sets it (#226)
bool flagClearOfPod(const CardGeometry &g)
{
    return (g.pod.top() - kPodBorder) - (g.flag.bottom() + kFlagBorder) >= kLabelGap;
}

bool readoutFits(const CardGeometry &g, const QRectF &ink)
{
    const double inset = kPodBorder + kReadoutPad;
    // No glyphs: nothing drawn
    return ink.isEmpty() || g.pod.adjusted(inset, inset, -inset, -inset).contains(withRasterMargin(ink));
}

double segmentDistance(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double length2 = QPointF::dotProduct(ab, ab);
    const double t = length2 > 0.0 ? std::clamp(QPointF::dotProduct(p - a, ab) / length2, 0.0, 1.0) : 0.0;
    const QPointF q = a + ab * t;
    return std::hypot(p.x() - q.x(), p.y() - q.y());
}

// The distance between two convex polygons, 0 when they overlap
double polygonDistance(const QPolygonF &a, const QPolygonF &b)
{
    if (a.intersects(b)) return 0.0;
    double d = std::numeric_limits<double>::infinity();
    for (int i = 0; i < a.size(); ++i) {
        for (int j = 0; j < b.size(); ++j) {
            d = std::min(d, segmentDistance(a[i], b[j], b[(j + 1) % b.size()]));
            d = std::min(d, segmentDistance(b[j], a[i], a[(i + 1) % a.size()]));
        }
    }
    return d;
}

struct LabelInk {
    int angle;
    QRectF ink; // around the centre of the label
};

// The labels fit the ring: each one clear of the tick circle and of its neighbours, on the metrics of Qt (no
// rasterization margin: the gaps are larger than it, #226). The pod and the flag lie inside the
// ring of the labels at every size (the geometry, not the font: measured never to bind, #226), as the test on the
// layout checks
bool labelsFit(const CardGeometry &g, const std::vector<LabelInk> &labels)
{
    std::vector<QPolygonF> polygons;
    polygons.reserve(labels.size());
    for (const LabelInk &label : labels) {
        QTransform rotation;
        rotation.rotate(label.angle);
        const QPolygonF polygon = rotation.map(QPolygonF(label.ink.translated(0.0, -g.textR)));
        double farthest = 0.0;
        for (const QPointF &corner : polygon) farthest = std::max(farthest, std::hypot(corner.x(), corner.y()));
        if (farthest > g.majorTickR - kCirclePen - kTickGap) return false;
        polygons.push_back(polygon);
    }
    for (size_t i = 0; polygons.size() > 1 && i < polygons.size(); ++i) {
        if (polygonDistance(polygons[i], polygons[(i + 1) % polygons.size()]) < kLabelGap) return false;
    }
    return true;
}

std::vector<LabelInk> labelInks(const CompassCardLayout &layout, const QFont &widgetFont, const QPaintDevice *device)
{
    std::vector<LabelInk> inks;
    for (const RingLabel &label : ringLabels()) {
        if (!isShown(label, layout)) continue;
        const QFontMetricsF fm(labelFont(widgetFont, label.kind, layout), device);
        inks.push_back({label.angle, textInk(fm, labelText(label.angle))});
    }
    return inks;
}

// The last layout: the cardinals and the readout at 7 pt
CompassCardLayout smallestLayout()
{
    CompassCardLayout layout;
    layout.cardinalPt = layout.intercardinalPt = layout.numberPt = layout.readoutPt = kMinimumPt;
    layout.numbers = CompassCardLayout::Numbers::None;
    layout.intercardinals = false;
    return layout;
}

// "HDG" at \a pixelSize inside \a flag (around its centre), clear of its border with the rasterization margin; text
// without glyphs draws nothing
bool flagTextFitsAt(const QRectF &flag, const QFont &widgetFont, int pixelSize, const QPaintDevice *device)
{
    QFont f = widgetFont;
    f.setBold(true);
    f.setPixelSize(pixelSize);
    const QRectF ink = textInk(QFontMetricsF(f, device), QStringLiteral("HDG")).translated(flag.center());
    const double inset = kFlagBorder + kReadoutPad;
    return ink.isEmpty() || flag.adjusted(inset, inset, -inset, -inset).contains(withRasterMargin(ink));
}

// The flag of a widget whose centre is \a centre, measured on \a device. "HDG" at 0.6 of the flag height, or the
// largest pixel size that fits the flag, never below 7 pt, the smallest text of the library (#226): the text grows in
// whole pixels and the flag in fractions, so a size that fits at one side could stick out at a larger one
CompassFlag headingFailureFlag(const QFont &widgetFont, const CardGeometry &geometry, const QPointF &centre,
                               const QPaintDevice *device)
{
    CompassFlag flag;
    flag.rect = geometry.flag.translated(centre);
    const int smallest = internal::minimumReadoutPixelSize(device->logicalDpiY());
    flag.textPixelSize = std::max(smallest, static_cast<int>(geometry.flag.height() * 0.6));
    while (flag.textPixelSize > smallest && !flagTextFitsAt(geometry.flag, widgetFont, flag.textPixelSize, device)) {
        --flag.textPixelSize;
    }
    flag.fill = QColor(225, 29, 46);
    flag.text = QColor(255, 255, 255);
    flag.textFont = widgetFont;
    flag.textFont.setBold(true);
    flag.textFont.setPixelSize(flag.textPixelSize);
    flag.textOrigin = flag.rect.center() + textOrigin(QFontMetricsF(flag.textFont, device), QStringLiteral("HDG"));
    flag.textInk = textInk(QFontMetricsF(flag.textFont, device), QStringLiteral("HDG")).translated(flag.rect.center());
    return flag;
}

// "HDG" fits the flag at 7 pt
bool flagTextFits(const CardGeometry &g, const QFont &widgetFont, const QPaintDevice *device)
{
    return flagTextFitsAt(g.flag, widgetFont, internal::minimumReadoutPixelSize(device->logicalDpiY()), device);
}

// #151: the failure flag of an invalid heading, centred above the readout. A fixed red with white letters, as
// on aircraft heading indicators: it means a failure whatever the theme
void drawHeadingFailureFlag(QPainter &painter, const CompassFlag &geometry)
{
    const QRectF flag = geometry.rect;

    painter.save();
    painter.setPen(QPen(QColor(20, 20, 20), 1.0));
    painter.setBrush(geometry.fill);
    painter.drawRoundedRect(flag, 2.0, 2.0);

    painter.setFont(geometry.textFont);
    painter.setPen(geometry.text);
    painter.drawText(geometry.textOrigin, QStringLiteral("HDG"));
    painter.restore();
}

} // namespace

CompassCardLayout CompassPrivate::cardLayout(int side, const QFont &widgetFont, const QPaintDevice *device,
                                             CompassLayoutCache &cache)
{
    cache.use(widgetFont, device->logicalDpiY());
    const auto cached = cache.layouts.find(side);
    if (cached != cache.layouts.end()) return cached->second;

    const CardGeometry g(side);
    const double r = g.radius;
    const auto fits = [&](const CompassCardLayout &layout) {
        return labelsFit(g, labelInks(layout, widgetFont, device));
    };

    // The labels: the fonts of the size, then down to 7 pt together, then fewer labels at 7 pt
    const double cardinalPt = std::clamp(r * 0.13, 8.0, 22.0);
    const double intercardinalPt = std::clamp(r * 0.08, kMinimumPt, 14.0);
    const double numberPt = std::clamp(r * 0.07, kMinimumPt, 12.0);
    const auto scaled = [&](double k) {
        CompassCardLayout layout;
        layout.cardinalPt = std::max(kMinimumPt, cardinalPt * k);
        layout.intercardinalPt = std::max(kMinimumPt, intercardinalPt * k);
        layout.numberPt = std::max(kMinimumPt, numberPt * k);
        return layout;
    };
    CompassCardLayout layout = scaled(1.0);
    bool labelsFitting = fits(layout);
    if (!labelsFitting) {
        double lo = kMinimumPt / std::max({cardinalPt, intercardinalPt, numberPt}); // every font at 7 pt
        if (fits(scaled(lo))) {
            double hi = 1.0;
            for (int i = 0; i < 12; ++i) {
                const double k = (lo + hi) / 2.0;
                (fits(scaled(k)) ? lo : hi) = k;
            }
            layout = scaled(lo);
            labelsFitting = true;
        } else {
            layout = smallestLayout();
            layout.numbers = CompassCardLayout::Numbers::EveryOther;
            layout.intercardinals = true;
            labelsFitting = fits(layout);
            if (!labelsFitting) {
                layout.numbers = CompassCardLayout::Numbers::None;
                labelsFitting = fits(layout);
            }
            if (!labelsFitting) {
                layout.intercardinals = false;
                labelsFitting = fits(layout);
            }
        }
    }

    // The readout: the font of the pod, else the largest size on a grid of 1/8 pt that fits, down to 7 pt. The grid
    // lets the sizes measured at one side serve the others
    const double readoutPt = std::clamp(g.pod.height() * 0.48, 8.0, 20.0);
    layout.readoutPt = readoutPt;
    bool readoutFitting = readoutFits(g, readoutInk(widgetFont, readoutPt, device, cache));
    if (!readoutFitting) {
        layout.readoutPt = kMinimumPt;
        readoutFitting = readoutFits(g, readoutInk(widgetFont, kMinimumPt, device, cache));
        if (readoutFitting) {
            int lo = 0;                                                       // 7 pt, fits
            int hi = static_cast<int>(std::ceil((readoutPt - kMinimumPt) * 8.0)); // does not fit
            while (hi - lo > 1) {
                const int mid = (lo + hi) / 2;
                (readoutFits(g, readoutInk(widgetFont, kMinimumPt + mid / 8.0, device, cache)) ? lo : hi) = mid;
            }
            layout.readoutPt = kMinimumPt + lo / 8.0;
        }
    }
    layout.fits = labelsFitting && readoutFitting && flagClearOfPod(g) && flagTextFits(g, widgetFont, device);
    layout.readoutInk = readoutInk(widgetFont, layout.readoutPt, device, cache);
    layout.majorTickR = g.majorTickR;
    layout.pod = g.pod;
    layout.flag = g.flag;
    for (const LabelInk &label : labelInks(layout, widgetFont, device)) {
        QTransform rotation;
        rotation.rotate(label.angle);
        layout.labels.push_back({label.angle, rotation.map(QPolygonF(label.ink.translated(0.0, -g.textR)))});
    }
    cache.layouts.emplace(side, layout);
    return layout;
}

int CompassPrivate::computeMinimumSide(const QFont &widgetFont, const QPaintDevice *device, CompassLayoutCache &cache)
{
    cache.use(widgetFont, device->logicalDpiY());
    // The last layout, measured once: its texts do not change with the side
    const CompassCardLayout smallest = smallestLayout();
    const std::vector<LabelInk> cardinals = labelInks(smallest, widgetFont, device);
    const QRectF readout = readoutInk(widgetFont, kMinimumPt, device, cache);
    const auto sideFits = [&](int side) {
        const CardGeometry g(side);
        return flagClearOfPod(g) && flagTextFits(g, widgetFont, device) && readoutFits(g, readout)
               && labelsFit(g, cardinals);
    };
    // From the preferred size: down while the side below fits, or up to the first side that fits
    int side = kPreferredSide;
    if (!sideFits(side)) {
        while (side < kLargestMinimum && !sideFits(side)) ++side;
        return side;
    }
    while (side > 1 && sideFits(side - 1)) --side;
    return side;
}




Compass::Compass(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<CompassPrivate>())
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

Compass::~Compass() = default;

double Compass::heading() const { Q_D(const Compass); return d->m_heading; }
ValueState Compass::valueState() const { Q_D(const Compass); return d->m_valueState; }
double Compass::targetHeading() const { Q_D(const Compass); return d->m_targetHeading; }
Compass::DisplayMode Compass::displayMode() const { Q_D(const Compass); return d->m_displayMode; }
bool Compass::isHeadingBugVisible() const { Q_D(const Compass); return d->m_bugVisible; }
bool Compass::isHeadingBugInteractive() const { Q_D(const Compass); return d->m_bugInteractive; }
bool Compass::isLubberLineVisible() const { Q_D(const Compass); return d->m_lubberVisible; }
bool Compass::isDigitalReadoutVisible() const { Q_D(const Compass); return d->m_digitalVisible; }

QColor Compass::dialColor() const { Q_D(const Compass); return d->m_dialColor; }
QColor Compass::bezelColor() const { Q_D(const Compass); return d->m_bezelColor; }
QColor Compass::textColor() const { Q_D(const Compass); return d->m_textColor; }
QColor Compass::readoutColor() const { Q_D(const Compass); return d->m_readoutColor; }
QColor Compass::cardinalColor() const { Q_D(const Compass); return d->m_cardinalColor; }
QColor Compass::needleColor() const { Q_D(const Compass); return d->m_needleColor; }
QColor Compass::needleTailColor() const { Q_D(const Compass); return d->m_needleTailColor; }
QColor Compass::bugColor() const { Q_D(const Compass); return d->m_bugColor; }
QColor Compass::lubberColor() const { Q_D(const Compass); return d->m_lubberColor; }



QSize Compass::sizeHint() const
{
    const int side = std::max(kPreferredSide, minimumSizeHint().width());
    return {side, side};
}

QSize Compass::minimumSizeHint() const
{
    // #226: from the font, at the logical DPI of the widget; computed again when either changes
    const double dpi = logicalDpiY();
    if (d_ptr->m_minimumSide == 0 || d_ptr->m_minimumSideDpi != dpi) {
        d_ptr->m_minimumSide = CompassPrivate::computeMinimumSide(font(), this, d_ptr->m_layoutCache);
        d_ptr->m_minimumSideDpi = dpi;
    }
    return {d_ptr->m_minimumSide, d_ptr->m_minimumSide};
}

double Compass::normalizeDegrees(double deg)
{
    if (!std::isfinite(deg)) {
        return 0.0;
    }
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    return r;
}

double Compass::courseDeviation() const
{
    double diff = std::fmod(d_ptr->m_heading - d_ptr->m_targetHeading + 540.0, 360.0) - 180.0;
    return diff;
}

void Compass::setHeading(double heading)
{
    // #151: NaN and ±Inf have no direction: the heading is invalid, never the last heading or 0°
    if (!std::isfinite(heading)) {
        if (d_ptr->isInvalid()) {
            return;
        }
        d_ptr->m_heading = std::numeric_limits<double>::quiet_NaN();
        d_ptr->m_valueState = ValueState::Invalid;
        Q_EMIT headingChanged(d_ptr->m_heading);
        Q_EMIT valueStateChanged(d_ptr->m_valueState);
        update();
        return;
    }
    double norm = normalizeDegrees(heading);
    const bool wasInvalid = d_ptr->isInvalid();
    if (wasInvalid || std::abs(d_ptr->m_heading - norm) > 0.001) {
        d_ptr->m_heading = norm;
        d_ptr->m_valueState = ValueState::Valid;
        Q_EMIT headingChanged(d_ptr->m_heading);
        if (wasInvalid) {
            Q_EMIT valueStateChanged(d_ptr->m_valueState);
        }
        update();
    }
}

void Compass::setTargetHeading(double target)
{
    if (!std::isfinite(target)) {
        return;
    }
    double norm = normalizeDegrees(target);
    if (std::abs(d_ptr->m_targetHeading - norm) > 0.001) {
        d_ptr->m_targetHeading = norm;
        Q_EMIT targetHeadingChanged(d_ptr->m_targetHeading);
        update();
    }
}

void Compass::setDisplayMode(DisplayMode mode)
{
    if (d_ptr->m_displayMode != mode) {
        d_ptr->m_displayMode = mode;
        invalidateCache();
        Q_EMIT displayModeChanged(d_ptr->m_displayMode);
        update();
    }
}

void Compass::setHeadingBugVisible(bool visible)
{
    if (d_ptr->m_bugVisible != visible) {
        d_ptr->m_bugVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setHeadingBugInteractive(bool interactive)
{
    if (d_ptr->m_bugInteractive != interactive) {
        d_ptr->m_bugInteractive = interactive;
        Q_EMIT appearanceChanged();
    }
}

void Compass::setLubberLineVisible(bool visible)
{
    if (d_ptr->m_lubberVisible != visible) {
        d_ptr->m_lubberVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setDigitalReadoutVisible(bool visible)
{
    if (d_ptr->m_digitalVisible != visible) {
        d_ptr->m_digitalVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setDialColor(const QColor &color)
{
    if (d_ptr->m_dialColor != color) {
        d_ptr->m_dialColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setBezelColor(const QColor &color)
{
    if (d_ptr->m_bezelColor != color) {
        d_ptr->m_bezelColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor != color) {
        d_ptr->m_textColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setReadoutColor(const QColor &color)
{
    if (d_ptr->m_readoutColor != color) {
        d_ptr->m_readoutColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setCardinalColor(const QColor &color)
{
    if (d_ptr->m_cardinalColor != color) {
        d_ptr->m_cardinalColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setNeedleColor(const QColor &color)
{
    if (d_ptr->m_needleColor != color) {
        d_ptr->m_needleColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setNeedleTailColor(const QColor &color)
{
    if (d_ptr->m_needleTailColor != color) {
        d_ptr->m_needleTailColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setBugColor(const QColor &color)
{
    if (d_ptr->m_bugColor != color) {
        d_ptr->m_bugColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setLubberColor(const QColor &color)
{
    if (d_ptr->m_lubberColor != color) {
        d_ptr->m_lubberColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
}

void Compass::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void Compass::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
    if (event->type() == QEvent::FontChange) {
        d_ptr->minimumSizeChanged(this);
    }
}

double Compass::angleFromPoint(const QPointF &pos) const
{
    QPointF center = rect().center();
    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();

    // In screen coordinates: Y is down, X is right.
    // 12 o'clock is (dx=0, dy<0), angle = 0°.
    // Clockwise: 3 o'clock is 90°, 6 o'clock is 180°, 9 o'clock is 270°.
    double rad = std::atan2(dx, -dy);
    double deg = qRadiansToDegrees(rad);
    return normalizeDegrees(deg);
}

void Compass::mousePressEvent(QMouseEvent *event)
{
    if (d_ptr->m_bugInteractive && event->button() == Qt::LeftButton) {
        double r = std::min(width(), height()) / 2.0;
        QPointF center = rect().center();
        double dist = std::hypot(event->position().x() - center.x(), event->position().y() - center.y());
        if (dist > r * 0.35 && dist <= r * 1.1) {
            d_ptr->m_isDraggingBug = true;
            double angle = angleFromPoint(event->position());
            if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
                // In HeadingUp mode, dial is rotated by -cardRotation(), so cursor angle corresponds to:
                setTargetHeading(normalizeDegrees(angle + d_ptr->cardRotation()));
            } else {
                setTargetHeading(angle);
            }
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void Compass::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->m_isDraggingBug && (event->buttons() & Qt::LeftButton)) {
        double angle = angleFromPoint(event->position());
        if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
            setTargetHeading(normalizeDegrees(angle + d_ptr->cardRotation()));
        } else {
            setTargetHeading(angle);
        }
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void Compass::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && d_ptr->m_isDraggingBug) {
        d_ptr->m_isDraggingBug = false;
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void Compass::renderCompassCard(const QSize &size)
{
    const qreal dpr = devicePixelRatioF();
    d_ptr->m_cardCache = QPixmap(size * dpr);
    d_ptr->m_cardCache.setDevicePixelRatio(dpr);
    d_ptr->m_cardCache.fill(Qt::transparent);

    QPainter p(&d_ptr->m_cardCache);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const double side = std::min(size.width(), size.height());
    const QPointF center(size.width() / 2.0, size.height() / 2.0);
    const CardGeometry geometry(side);
    const double radius = geometry.radius;

    // #226: the texts that fit at this size, at the logical DPI of the widget, also in the pixmap (#177)
    d_ptr->m_cacheDpi = logicalDpiY();
    d_ptr->m_cardLayout = CompassPrivate::cardLayout(static_cast<int>(side), font(), this, d_ptr->m_layoutCache);
    const CompassCardLayout &layout = d_ptr->m_cardLayout;

    if (radius <= 10.0) {
        d_ptr->m_cacheDirty = false;
        return;
    }

    // 1. Outer Bezel Ring (Machined metal chamfer)
    QRadialGradient bezelGrad(center, radius + 5.0, center - QPointF(radius * 0.3, radius * 0.3));
    bezelGrad.setColorAt(0.0, d_ptr->m_bezelColor.lighter(140));
    bezelGrad.setColorAt(0.7, d_ptr->m_bezelColor);
    bezelGrad.setColorAt(1.0, d_ptr->m_bezelColor.darker(170));

    p.setPen(QPen(d_ptr->m_bezelColor.darker(200), 1.5));
    p.setBrush(bezelGrad);
    p.drawEllipse(center, radius + 5.0, radius + 5.0);

    // Bezel inner groove
    p.setPen(QPen(QColor(15, 18, 25), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, radius, radius);

    // 2. Dial Face Background
    QRadialGradient dialGrad(center, radius, center - QPointF(0.0, radius * 0.25));
    dialGrad.setColorAt(0.0, d_ptr->m_dialColor.lighter(125));
    dialGrad.setColorAt(0.8, d_ptr->m_dialColor);
    dialGrad.setColorAt(1.0, d_ptr->m_dialColor.darker(140));

    p.setPen(Qt::NoPen);
    p.setBrush(dialGrad);
    p.drawEllipse(center, radius - 1.0, radius - 1.0);

    // 3. Central Decorative Nautical Star (8-point compass rose watermark)
    p.save();
    p.translate(center);
    const double starRadius = radius * 0.32;
    for (int pt = 0; pt < 8; ++pt) {
        p.save();
        p.rotate(pt * 45.0);

        QPainterPath lightHalf;
        lightHalf.moveTo(0.0, 0.0);
        lightHalf.lineTo(0.0, -starRadius);
        lightHalf.lineTo(starRadius * 0.25, -starRadius * 0.3);
        lightHalf.closeSubpath();

        QPainterPath darkHalf;
        darkHalf.moveTo(0.0, 0.0);
        darkHalf.lineTo(0.0, -starRadius);
        darkHalf.lineTo(-starRadius * 0.25, -starRadius * 0.3);
        darkHalf.closeSubpath();

        QColor baseC = (pt % 2 == 0) ? d_ptr->m_cardinalColor : d_ptr->m_textColor;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(baseC.red(), baseC.green(), baseC.blue(), 38));
        p.drawPath(lightHalf);

        p.setBrush(QColor(baseC.red(), baseC.green(), baseC.blue(), 18));
        p.drawPath(darkHalf);

        p.restore();
    }
    p.restore();

    // 4. Dial Graduation Scale & Tick Marks
    p.save();
    p.translate(center);

    const double outerTickR = geometry.outerTickR;
    const double minorTickR = radius - 3.0 - (radius * 0.045);
    const double medTickR   = radius - 3.0 - (radius * 0.08);
    const double majorTickR = geometry.majorTickR;
    const double textR      = geometry.textR;

    // Outer track circle line
    p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 60), 1.0));
    p.drawEllipse(QPointF(0.0, 0.0), outerTickR, outerTickR);
    p.drawEllipse(QPointF(0.0, 0.0), majorTickR, majorTickR);

    for (int deg = 0; deg < 360; ++deg) {
        if (deg % 5 != 0) continue;

        p.save();
        p.rotate(deg);

        if (deg % 30 == 0) {
            // Major tick mark (every 30°)
            p.setPen(QPen(d_ptr->m_textColor, 1.8));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -majorTickR));
        } else if (deg % 10 == 0) {
            // Medium tick mark (every 10°)
            p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 180), 1.2));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -medTickR));
        } else {
            // Minor tick mark (every 5°)
            p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 100), 0.8));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -minorTickR));
        }

        p.restore();
    }

    // Labels (#226): those of the layout, centred at textR, at the logical DPI of the widget (#177): the pixmap has
    // the DPI of the primary screen, so its point sizes are scaled to that of the widget
    const double dpiScale = logicalDpiY() / static_cast<double>(d_ptr->m_cardCache.logicalDpiY());
    for (const RingLabel &label : ringLabels()) {
        if (!isShown(label, layout)) continue;
        QFont f = labelFont(font(), label.kind, layout);
        f.setPointSizeF(f.pointSizeF() * dpiScale);
        const QString text = labelText(label.angle);
        p.save();
        p.rotate(label.angle);
        p.setFont(f);
        p.setPen(label.angle == 0                       ? d_ptr->m_needleColor
                 : label.kind == LabelKind::Cardinal ? d_ptr->m_cardinalColor
                                                      : d_ptr->m_textColor);
        // Measured on the pixmap, where it is drawn: a font from font() keeps the logical DPI of the widget (#226)
        p.drawText(QPointF(0.0, -textR) + textOrigin(QFontMetricsF(f, &d_ptr->m_cardCache), text), text);
        p.restore();
    }
    p.restore();

    d_ptr->m_cacheDirty = false;
}

void Compass::paintEvent(QPaintEvent *)
{
    const int side = std::min(width(), height());
    if (side <= 10) return;

    // #177, #226: a window moved to a screen of another logical DPI is repainted. The minimum and the card depend on
    // that DPI (points), so from this repaint the layout gets the new minimum, once, and the card is drawn again
    if (d_ptr->m_minimumSide != 0 && d_ptr->m_minimumSideDpi != logicalDpiY()) {
        d_ptr->minimumSizeChanged(this);
    }
    if (d_ptr->m_cacheDirty || d_ptr->m_cardCache.isNull() || d_ptr->m_cardCache.size() != size() * devicePixelRatioF()
        || d_ptr->m_cacheDpi != logicalDpiY()) {
        renderCompassCard(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPointF center = rect().center();
    const double radius = (side / 2.0) - 6.0;

    // 1. Draw Compass Card
    if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
        // HeadingUp: Card rotates by -heading around center (#151: drawn at 0°, dimmed, while invalid)
        painter.save();
        painter.translate(center);
        painter.rotate(-d_ptr->cardRotation());
        painter.drawPixmap(-width() / 2, -height() / 2, d_ptr->m_cardCache);

        // Draw Target Heading Bug rotated on the moving card:
        if (d_ptr->m_bugVisible) {
            painter.save();
            painter.rotate(d_ptr->m_targetHeading);
            // Draw Heading Bug Chevron at top
            QPainterPath bug;
            const double bugR = radius - 2.0;
            const double bugW = std::clamp(radius * 0.12, 8.0, 18.0);
            const double bugH = std::clamp(radius * 0.10, 6.0, 15.0);

            bug.moveTo(-bugW, -bugR);
            bug.lineTo(-bugW * 0.6, -bugR + bugH);
            bug.lineTo(0.0, -bugR + bugH * 0.4);
            bug.lineTo(bugW * 0.6, -bugR + bugH);
            bug.lineTo(bugW, -bugR);
            bug.lineTo(0.0, -bugR - 2.0);
            bug.closeSubpath();

            painter.setPen(QPen(d_ptr->m_bugColor.darker(140), 1.2));
            painter.setBrush(d_ptr->m_bugColor);
            painter.drawPath(bug);
            painter.restore();
        }

        // #151: an invalid heading veils the card and the bug (whose place from 12 o'clock would read as a
        // course deviation) with the dial colour; the veil covers the tip of the bug, drawn up to the radius
        if (d_ptr->isInvalid()) {
            QColor veil = d_ptr->m_dialColor;
            veil.setAlpha(170);
            painter.setPen(Qt::NoPen);
            painter.setBrush(veil);
            painter.drawEllipse(QPointF(0.0, 0.0), radius + 1.0, radius + 1.0);
        }

        painter.restore();

        // 2. Static Lubber Line at 12 o'clock (Fixed vessel bow reference); none for an invalid heading (#151)
        if (d_ptr->m_lubberVisible && !d_ptr->isInvalid()) {
            painter.save();
            painter.translate(center);

            const double lubberR = radius + 2.0;
            const double lubberW = std::clamp(radius * 0.08, 6.0, 14.0);
            const double lubberH = std::clamp(radius * 0.12, 8.0, 18.0);

            QPainterPath lubber;
            lubber.moveTo(0.0, -lubberR + lubberH);
            lubber.lineTo(-lubberW / 2.0, -lubberR);
            lubber.lineTo(lubberW / 2.0, -lubberR);
            lubber.closeSubpath();

            painter.setPen(QPen(QColor(20, 20, 20), 1.0));
            painter.setBrush(d_ptr->m_lubberColor);
            painter.drawPath(lubber);

            // Center lubber vertical pointer line
            painter.setPen(QPen(d_ptr->m_lubberColor, 2.0));
            painter.drawLine(QPointF(0.0, -lubberR), QPointF(0.0, -lubberR + radius * 0.16));

            painter.restore();
        }
    } else {
        // NorthUp: Card is static (North at top)
        painter.save();
        painter.translate(center);
        painter.drawPixmap(-width() / 2, -height() / 2, d_ptr->m_cardCache);

        // Draw Target Heading Bug rotated to targetHeading
        if (d_ptr->m_bugVisible) {
            painter.save();
            painter.rotate(d_ptr->m_targetHeading);
            const double bugR = radius - 2.0;
            const double bugW = std::clamp(radius * 0.12, 8.0, 18.0);
            const double bugH = std::clamp(radius * 0.10, 6.0, 15.0);

            QPainterPath bug;
            bug.moveTo(-bugW, -bugR);
            bug.lineTo(-bugW * 0.6, -bugR + bugH);
            bug.lineTo(0.0, -bugR + bugH * 0.4);
            bug.lineTo(bugW * 0.6, -bugR + bugH);
            bug.lineTo(bugW, -bugR);
            bug.lineTo(0.0, -bugR - 2.0);
            bug.closeSubpath();

            painter.setPen(QPen(d_ptr->m_bugColor.darker(140), 1.2));
            painter.setBrush(d_ptr->m_bugColor);
            painter.drawPath(bug);
            painter.restore();
        }

        // Draw Rotating Magnetic Needle pointing to d_ptr->m_heading; none for an invalid heading (#151)
        if (!d_ptr->isInvalid()) {
        painter.save();
        painter.rotate(d_ptr->m_heading);

        const double needleLen = radius * 0.68;
        const double needleW = std::clamp(radius * 0.08, 6.0, 16.0);

        // North half (Vivid red arrow)
        QPainterPath northHalfLight;
        northHalfLight.moveTo(0.0, 0.0);
        northHalfLight.lineTo(0.0, -needleLen);
        northHalfLight.lineTo(needleW / 2.0, -needleLen * 0.2);
        northHalfLight.closeSubpath();

        QPainterPath northHalfDark;
        northHalfDark.moveTo(0.0, 0.0);
        northHalfDark.lineTo(0.0, -needleLen);
        northHalfDark.lineTo(-needleW / 2.0, -needleLen * 0.2);
        northHalfDark.closeSubpath();

        painter.setPen(Qt::NoPen);
        painter.setBrush(d_ptr->m_needleColor);
        painter.drawPath(northHalfLight);
        painter.setBrush(d_ptr->m_needleColor.darker(140));
        painter.drawPath(northHalfDark);

        // South half (Slate / silver tail arrow)
        QPainterPath southHalfLight;
        southHalfLight.moveTo(0.0, 0.0);
        southHalfLight.lineTo(0.0, needleLen * 0.85);
        southHalfLight.lineTo(needleW / 2.0, needleLen * 0.2);
        southHalfLight.closeSubpath();

        QPainterPath southHalfDark;
        southHalfDark.moveTo(0.0, 0.0);
        southHalfDark.lineTo(0.0, needleLen * 0.85);
        southHalfDark.lineTo(-needleW / 2.0, needleLen * 0.2);
        southHalfDark.closeSubpath();

        painter.setBrush(d_ptr->m_needleTailColor);
        painter.drawPath(southHalfLight);
        painter.setBrush(d_ptr->m_needleTailColor.darker(140));
        painter.drawPath(southHalfDark);

        painter.restore();
        }

        // Lubber reference line at 12 o'clock (the painter is already at the centre, #165)
        if (d_ptr->m_lubberVisible) {
            painter.save();
            const double lubberR = radius + 2.0;
            const double lubberW = std::clamp(radius * 0.08, 6.0, 14.0);
            const double lubberH = std::clamp(radius * 0.12, 8.0, 18.0);

            QPainterPath lubber;
            lubber.moveTo(0.0, -lubberR + lubberH);
            lubber.lineTo(-lubberW / 2.0, -lubberR);
            lubber.lineTo(lubberW / 2.0, -lubberR);
            lubber.closeSubpath();

            painter.setPen(QPen(QColor(20, 20, 20), 1.0));
            painter.setBrush(d_ptr->m_lubberColor);
            painter.drawPath(lubber);
            painter.restore();
        }

        painter.restore();
    }

    // #151: failure flag of an invalid heading
    d_ptr->m_flag = headingFailureFlag(font(), CardGeometry(side), center, this);
    if (d_ptr->isInvalid()) {
        drawHeadingFailureFlag(painter, d_ptr->m_flag);
    }

    // 3. Central Digital Readout Pod
    if (d_ptr->m_digitalVisible) {
        painter.save();
        painter.translate(center);

        const QRectF podRect = CardGeometry(side).pod;

        // Recessed bevel pod background
        QLinearGradient podGrad(podRect.topLeft(), podRect.bottomLeft());
        podGrad.setColorAt(0.0, QColor(14, 18, 24));
        podGrad.setColorAt(1.0, QColor(24, 30, 40));

        painter.setPen(QPen(d_ptr->m_bezelColor.darker(150), 1.5));
        painter.setBrush(podGrad);
        painter.drawRoundedRect(podRect, 4.0, 4.0);

        // Digital Heading Readout text (e.g. "045°")
        QFont podFont = font();
        podFont.setBold(true);
        podFont.setPointSizeF(d_ptr->m_cardLayout.readoutPt); // #226: down to 7 pt so that the widest heading fits
        painter.setFont(podFont);
        painter.setPen(d_ptr->m_readoutColor);

        QString text = QStringLiteral("---°"); // #151: an invalid heading
        if (!d_ptr->isInvalid()) {
            int intHdg = static_cast<int>(std::round(d_ptr->m_heading)) % 360;
            text = QStringLiteral("%1°").arg(intHdg, 3, 10, QLatin1Char('0'));
        }
        // Its ink centred on the pod, as the layout measured it (#226)
        // Its ink centred on the pod (#226), the origin on a whole device pixel (the painter is translated by center)
        const qreal dpr = devicePixelRatioF();
        const double inkCentre = readoutInkCentre(podFont, this, dpr, text, d_ptr->m_readoutOrigin);
        const double originX = std::round((center.x() + podRect.center().x()) * dpr - inkCentre) / dpr - center.x();
        painter.drawText(QPointF(originX, podRect.center().y() + textOrigin(QFontMetricsF(podFont, this), text).y()),
                         text);

        painter.restore();
    }
}

} // namespace QtIndustrialWidgets
