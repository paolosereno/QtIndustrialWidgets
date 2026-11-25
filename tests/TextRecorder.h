// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

// A paint device that records the texts drawn on it (#208): QWidget::render() onto it runs the widget's paintEvent()
// with a painter whose engine keeps the text of every drawTextItem(), in the order drawn, and draws nothing. For the
// widgets that paint their scale labels directly (no cached pixmap): what they draw, not a function they call

#include <QtCore/QStringList>
#include <QtGui/QPaintDevice>
#include <QtGui/QImage>
#include <QtGui/QPaintEngine>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtGui/QTextItem>
#include <QtWidgets/QWidget>

#include <memory>

namespace TextRecorder {

class Engine : public QPaintEngine
{
public:
    Engine() : QPaintEngine(QPaintEngine::AllFeatures) {}
    bool begin(QPaintDevice *) override { return true; }
    bool end() override { return true; }
    void updateState(const QPaintEngineState &) override {}
    // Every primitive is a no-op: QPaintEngine's defaults call each other with every feature on
    void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override {}
    void drawTiledPixmap(const QRectF &, const QPixmap &, const QPointF &) override {}
    void drawImage(const QRectF &, const QImage &, const QRectF &, Qt::ImageConversionFlags) override {}
    void drawPath(const QPainterPath &) override {}
    void drawPolygon(const QPointF *, int, PolygonDrawMode) override {}
    void drawPolygon(const QPoint *, int, PolygonDrawMode) override {}
    void drawRects(const QRect *, int) override {}
    void drawRects(const QRectF *, int) override {}
    void drawLines(const QLine *, int) override {}
    void drawLines(const QLineF *, int) override {}
    void drawEllipse(const QRectF &) override {}
    void drawEllipse(const QRect &) override {}
    void drawPoints(const QPointF *, int) override {}
    void drawPoints(const QPoint *, int) override {}
    void drawTextItem(const QPointF &, const QTextItem &item) override { texts << item.text(); }
    Type type() const override { return QPaintEngine::User; }

    QStringList texts;
};

class Device : public QPaintDevice
{
public:
    explicit Device(QSize size) : m_size(size) {}
    QPaintEngine *paintEngine() const override { return &m_engine; }
    QStringList texts() const { return m_engine.texts; }

protected:
    int metric(PaintDeviceMetric m) const override
    {
        switch (m) {
        case PdmWidth: return m_size.width();
        case PdmHeight: return m_size.height();
        case PdmWidthMM: return m_size.width() * 254 / 960;
        case PdmHeightMM: return m_size.height() * 254 / 960;
        case PdmNumColors: return 0x7fffffff;
        case PdmDepth: return 32;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY: return 96;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return static_cast<int>(QPaintDevice::devicePixelRatioFScale());
        default: return 0;
        }
    }

private:
    QSize m_size;
    mutable Engine m_engine;
};

/// The texts \a widget draws in one paint, in the order drawn
inline QStringList drawnTexts(QWidget &widget)
{
    Device device(widget.size());
    widget.render(&device, QPoint(), QRegion(), QWidget::DrawChildren);
    return device.texts();
}

/// True if \a sequence holds \a part as consecutive items
inline bool containsRun(const QStringList &sequence, const QStringList &part)
{
    for (int i = 0; i + part.size() <= sequence.size(); ++i) {
        if (sequence.mid(i, part.size()) == part) return true;
    }
    return false;
}

} // namespace TextRecorder
