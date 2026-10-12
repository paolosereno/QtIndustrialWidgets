// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

// TEMP (#31): does drawText() into a rectangle exactly as large as boundingRect() (what
// LinearGauge does for its scale labels) cut the label's pixels? Remove before merging.
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <algorithm>
#include <cstdio>

namespace {

bool isLabelPixel(QRgb p) // same classifier as tst_LinearGauge
{
    return qRed(p) >= 120 && qBlue(p) >= 120 && qGreen(p) * 2 <= qRed(p) && qGreen(p) * 2 <= qBlue(p);
}

enum class Box { Tight, Wide, TightDontClip };

// Length of the label's pixels along \a axis (logical px)
double inkLength(const QString &text, const QFont &font, qreal dpr, Qt::Orientation axis, Box box, int phase)
{
    const QRectF br = QFontMetricsF(font).boundingRect(text);
    const QSize canvas = (br.size() + QSizeF(40, 40)).toSize();
    QImage img(canvas * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(QColor(44, 53, 64));
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font);
        p.setPen(QColor(255, 0, 255));
        const double shift = phase * 0.25 / dpr;
        QRectF rect(20.0, 20.0, br.width(), br.height());
        int flags = Qt::AlignCenter;
        if (axis == Qt::Vertical) {
            flags = Qt::AlignLeft | Qt::AlignVCenter; // vertical labels: tight in height only
            rect.setWidth(br.width() + 20.0);
        }
        if (box == Box::Wide) {
            rect = axis == Qt::Horizontal ? rect.adjusted(-10, 0, 10, 0) : rect.adjusted(0, -10, 0, 10);
        }
        if (box == Box::TightDontClip) {
            flags |= Qt::TextDontClip;
        }
        rect.translate(axis == Qt::Horizontal ? QPointF(shift, 0) : QPointF(0, shift));
        p.drawText(rect, flags, text);
    }
    int lo = -1, hi = -1;
    const int len = axis == Qt::Horizontal ? img.width() : img.height();
    const int across = axis == Qt::Horizontal ? img.height() : img.width();
    for (int a = 0; a < len; ++a) {
        for (int b = 0; b < across; ++b) {
            if (isLabelPixel(axis == Qt::Horizontal ? img.pixel(a, b) : img.pixel(b, a))) {
                if (lo < 0) lo = a;
                hi = a;
                break;
            }
        }
    }
    return lo < 0 ? 0.0 : (hi - lo + 1) / dpr;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QFont widgetFont = QWidget().font();
    const qreal dpr = app.primaryScreen()->devicePixelRatio();
    std::printf("clipdiag: widget font '%s' %.1fpt, DPR %.2f\n", qPrintable(widgetFont.family()), widgetFont.pointSizeF(), dpr);

    QStringList texts;
    for (int v = 0; v <= 250; v += 25) texts << QString::number(v);
    for (int v = 0; v <= 120; v += 12) texts << QString::number(v);
    for (int v = 0; v <= 100; v += 10) texts << QString::number(v);
    texts.removeDuplicates();

    int cut[2] = {0, 0}, total = 0;
    for (int px : {8, 12}) {
        QFont font = widgetFont;
        font.setPixelSize(px);
        font.setBold(true);
        for (const QString &t : texts) {
            const QRectF br = QFontMetricsF(font).boundingRect(t);
            QString line = QStringLiteral("%1px '%2' boundingRect %3 x %4 |").arg(px).arg(t, 3).arg(br.width(), 0, 'f', 2).arg(br.height(), 0, 'f', 2);
            for (Qt::Orientation axis : {Qt::Horizontal, Qt::Vertical}) {
                double mn[3] = {1e9, 1e9, 1e9}, mx[3] = {0, 0, 0};
                for (int phase = 0; phase < 4; ++phase) {
                    for (int b = 0; b < 3; ++b) {
                        const double l = inkLength(t, font, dpr, axis, static_cast<Box>(b), phase);
                        mn[b] = std::min(mn[b], l);
                        mx[b] = std::max(mx[b], l);
                    }
                }
                const bool clipped = mn[0] + 1e-9 < mn[1] || mx[0] + 1e-9 < mx[1];
                const int ai = axis == Qt::Horizontal ? 0 : 1;
                cut[ai] += clipped ? 1 : 0;
                line += QStringLiteral(" %1 tight %2-%3 wide %4-%5 tight+DontClip %6-%7%8 |")
                            .arg(axis == Qt::Horizontal ? QStringLiteral("H width") : QStringLiteral("V height"))
                            .arg(mn[0], 0, 'f', 2).arg(mx[0], 0, 'f', 2).arg(mn[1], 0, 'f', 2).arg(mx[1], 0, 'f', 2)
                            .arg(mn[2], 0, 'f', 2).arg(mx[2], 0, 'f', 2).arg(clipped ? QStringLiteral(" CUT") : QString());
            }
            ++total;
            std::printf("%s\n", qPrintable(line));
        }
    }
    std::printf("clipdiag summary (DPR %.2f): horizontal cut in %d of %d label texts, vertical cut in %d of %d\n", dpr, cut[0], total, cut[1], total);
    return 0;
}
