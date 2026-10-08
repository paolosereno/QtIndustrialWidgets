#include <QtIndustrialWidgets/QSevenSegmentDisplay.h>

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtCore/QtMath>
#include <algorithm>
#include <vector>

QSevenSegmentDisplay::QSevenSegmentDisplay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    updateFormattedText();
}

QSize QSevenSegmentDisplay::sizeHint() const
{
    return QSize(m_digitCount * 45 + 30, 80);
}

QSize QSevenSegmentDisplay::minimumSizeHint() const
{
    return QSize(m_digitCount * 18 + 15, 35);
}

void QSevenSegmentDisplay::setValue(double val)
{
    m_isTextExplicit = false;
    if (qFuzzyCompare(val, m_value)) return;
    m_value = val;
    updateFormattedText();
    Q_EMIT valueChanged(m_value);
    update();
}

void QSevenSegmentDisplay::setText(const QString &text)
{
    m_isTextExplicit = true;
    if (m_text == text) return;
    m_text = text;
    bool ok = false;
    double parsed = text.toDouble(&ok);
    if (ok) {
        m_value = parsed;
        Q_EMIT valueChanged(m_value);
    }
    Q_EMIT textChanged(m_text);
    update();
}

void QSevenSegmentDisplay::display(double value)
{
    setValue(value);
}

void QSevenSegmentDisplay::display(int value)
{
    setValue(static_cast<double>(value));
}

void QSevenSegmentDisplay::display(const QString &text)
{
    setText(text);
}

void QSevenSegmentDisplay::setDigitCount(int count)
{
    int c = std::clamp(count, 1, 16);
    if (m_digitCount == c) return;
    m_digitCount = c;
    updateFormattedText();
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setDecimalPlaces(int places)
{
    if (m_decimalPlaces == places) return;
    m_decimalPlaces = places;
    if (!m_isTextExplicit) {
        updateFormattedText();
    }
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setShowLeadingZeros(bool show)
{
    if (m_showLeadingZeros == show) return;
    m_showLeadingZeros = show;
    if (!m_isTextExplicit) {
        updateFormattedText();
    }
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setShowDecimalPoint(bool show)
{
    if (m_showDecimalPoint == show) return;
    m_showDecimalPoint = show;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setSkewAngle(double angle)
{
    if (qFuzzyCompare(m_skewAngle, angle)) return;
    m_skewAngle = std::clamp(angle, -30.0, 30.0);
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setSegmentWidthRatio(double ratio)
{
    if (qFuzzyCompare(m_segmentWidthRatio, ratio)) return;
    m_segmentWidthRatio = std::clamp(ratio, 0.08, 0.25);
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setActiveSegmentColor(const QColor &color)
{
    if (m_activeSegmentColor == color) return;
    m_activeSegmentColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setInactiveSegmentColor(const QColor &color)
{
    if (m_inactiveSegmentColor == color) return;
    m_inactiveSegmentColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor == color) return;
    m_backgroundColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setBezelColor(const QColor &color)
{
    if (m_bezelColor == color) return;
    m_bezelColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::setBezelVisible(bool visible)
{
    if (m_bezelVisible == visible) return;
    m_bezelVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void QSevenSegmentDisplay::updateFormattedText()
{
    if (m_isTextExplicit) return;

    QString s;
    if (m_decimalPlaces >= 0) {
        s = QString::number(m_value, 'f', m_decimalPlaces);
    } else {
        s = QString::number(m_value);
    }

    if (m_showLeadingZeros) {
        int intDigitsNeeded = m_digitCount - ((m_decimalPlaces > 0) ? (m_decimalPlaces + 1) : 0);
        if (m_value < 0.0) intDigitsNeeded--;
        QStringList parts = s.split(QLatin1Char('.'));
        if (!parts.isEmpty()) {
            QString intPart = parts.first();
            bool isNeg = intPart.startsWith(QLatin1Char('-'));
            if (isNeg) intPart.remove(0, 1);
            while (intPart.length() < intDigitsNeeded) {
                intPart.prepend(QLatin1Char('0'));
            }
            if (isNeg) intPart.prepend(QLatin1Char('-'));
            parts[0] = intPart;
            s = parts.join(QLatin1Char('.'));
        }
    }

    m_text = s;
    Q_EMIT textChanged(m_text);
}

quint8 QSevenSegmentDisplay::encodeChar(QChar ch)
{
    // Segment bits:
    // bit 0: a (top)
    // bit 1: b (top-right)
    // bit 2: c (bottom-right)
    // bit 3: d (bottom)
    // bit 4: e (bottom-left)
    // bit 5: f (top-left)
    // bit 6: g (middle)
    char c = ch.toLatin1();
    switch (c) {
        case '0': return 0x3F; // a,b,c,d,e,f
        case '1': return 0x06; // b,c
        case '2': return 0x5B; // a,b,d,e,g
        case '3': return 0x4F; // a,b,c,d,g
        case '4': return 0x66; // b,c,f,g
        case '5': return 0x6D; // a,c,d,f,g
        case '6': return 0x7D; // a,c,d,e,f,g
        case '7': return 0x07; // a,b,c
        case '8': return 0x7F; // a,b,c,d,e,f,g
        case '9': return 0x6F; // a,b,c,d,f,g
        case '-': return 0x40; // g
        case ' ': return 0x00;
        case '_': return 0x08; // d
        case '=': return 0x48; // d,g
        case 'A': case 'a': return 0x77;
        case 'B': case 'b': return 0x7C;
        case 'C': return 0x39;
        case 'c': return 0x58;
        case 'D': case 'd': return 0x5E;
        case 'E': case 'e': return 0x79;
        case 'F': case 'f': return 0x71;
        case 'H': case 'h': return 0x76;
        case 'L': case 'l': return 0x38;
        case 'P': case 'p': return 0x73;
        case 'O': case 'o': return 0x5C;
        case 'r': return 0x50;
        case 'U': case 'u': return 0x3E;
        default:  return 0x00;
    }
}

void QSevenSegmentDisplay::drawDigit(QPainter &painter, const QRectF &rect, quint8 mask, bool hasDecimalPoint) const
{
    const double w = rect.width();
    const double h = rect.height();
    const double t = w * m_segmentWidthRatio;
    const double gap = t * 0.18;
    const double yMid = h * 0.5;

    // We define each of the 7 segments as a polygon relative to (0, 0, w, h)
    std::vector<QPolygonF> segments(7);

    // Segment 0 (a): top horizontal
    {
        double x0 = t + gap;
        double x1 = w - t - gap;
        double y0 = gap;
        double y1 = gap + t;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1 - t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1 - t * 0.5, y1)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y0 + t * 0.5);
        segments[0] = poly;
    }

    // Segment 1 (b): top-right vertical
    {
        double x0 = w - gap - t;
        double x1 = w - gap;
        double y0 = gap + t;
        double y1 = yMid - gap;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1, y1 - t * 0.5)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y1 - t * 0.5)
             << QPointF(x0, y0 + t * 0.5);
        segments[1] = poly;
    }

    // Segment 2 (c): bottom-right vertical
    {
        double x0 = w - gap - t;
        double x1 = w - gap;
        double y0 = yMid + gap;
        double y1 = h - gap - t;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1, y1 - t * 0.5)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y1 - t * 0.5)
             << QPointF(x0, y0 + t * 0.5);
        segments[2] = poly;
    }

    // Segment 3 (d): bottom horizontal
    {
        double x0 = t + gap;
        double x1 = w - t - gap;
        double y0 = h - gap - t;
        double y1 = h - gap;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1 - t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1 - t * 0.5, y1)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y0 + t * 0.5);
        segments[3] = poly;
    }

    // Segment 4 (e): bottom-left vertical
    {
        double x0 = gap;
        double x1 = gap + t;
        double y0 = yMid + gap;
        double y1 = h - gap - t;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1, y1 - t * 0.5)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y1 - t * 0.5)
             << QPointF(x0, y0 + t * 0.5);
        segments[4] = poly;
    }

    // Segment 5 (f): top-left vertical
    {
        double x0 = gap;
        double x1 = gap + t;
        double y0 = gap + t;
        double y1 = yMid - gap;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1, y0 + t * 0.5)
             << QPointF(x1, y1 - t * 0.5)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, y1 - t * 0.5)
             << QPointF(x0, y0 + t * 0.5);
        segments[5] = poly;
    }

    // Segment 6 (g): middle horizontal
    {
        double x0 = t + gap;
        double x1 = w - t - gap;
        double y0 = yMid - t * 0.5;
        double y1 = yMid + t * 0.5;
        QPolygonF poly;
        poly << QPointF(x0 + t * 0.5, y0)
             << QPointF(x1 - t * 0.5, y0)
             << QPointF(x1, yMid)
             << QPointF(x1 - t * 0.5, y1)
             << QPointF(x0 + t * 0.5, y1)
             << QPointF(x0, yMid);
        segments[6] = poly;
    }

    painter.save();
    painter.translate(rect.topLeft());

    // Apply italic skew if configured
    if (!qFuzzyIsNull(m_skewAngle)) {
        double shear = -std::tan(m_skewAngle * M_PI / 180.0);
        painter.translate(0.0, h);
        painter.shear(shear, 0.0);
        painter.translate(0.0, -h);
    }

    // Draw 7 segments: inactive first, then active
    painter.setPen(Qt::NoPen);

    for (int i = 0; i < 7; ++i) {
        bool isActive = (mask & (1 << i)) != 0;
        painter.setBrush(isActive ? m_activeSegmentColor : m_inactiveSegmentColor);
        painter.drawPolygon(segments[i]);
    }

    // Draw Decimal Point (dp) at bottom-right of digit
    if (m_showDecimalPoint) {
        double dpSize = t * 0.95;
        QRectF dpRect(w + gap * 1.5, h - t - gap, dpSize, dpSize);
        painter.setBrush(hasDecimalPoint ? m_activeSegmentColor : m_inactiveSegmentColor);
        painter.drawRoundedRect(dpRect, 1.5, 1.5);
    }

    painter.restore();
}

void QSevenSegmentDisplay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double w = width();
    const double h = height();

    // 1. Draw Casing & Background
    QRectF panelRect(1.0, 1.0, w - 2.0, h - 2.0);
    if (m_bezelVisible) {
        painter.setPen(QPen(m_bezelColor.darker(150), 1.5));
        painter.setBrush(m_backgroundColor);
        painter.drawRoundedRect(panelRect, 5.0, 5.0);

        // Subtle inner shadow highlight
        painter.setPen(QPen(QColor(0, 0, 0, 90), 1.2));
        painter.drawRoundedRect(panelRect.adjusted(1.5, 1.5, -1.5, -1.5), 4.0, 4.0);
    } else {
        painter.fillRect(panelRect, m_backgroundColor);
    }

    // 2. Parse text into digit slots with decimal points
    struct DigitData {
        QChar ch;
        bool hasDp{false};
    };

    std::vector<DigitData> parsedDigits;
    for (int i = 0; i < m_text.length(); ++i) {
        QChar c = m_text.at(i);
        if (c == QLatin1Char('.')) {
            if (!parsedDigits.empty()) {
                parsedDigits.back().hasDp = true;
            } else {
                parsedDigits.push_back({QLatin1Char(' '), true});
            }
        } else {
            parsedDigits.push_back({c, false});
        }
    }

    // Align into m_digitCount slots (right aligned)
    std::vector<DigitData> displaySlots(m_digitCount, {QLatin1Char(' '), false});
    int startIdx = m_digitCount - static_cast<int>(parsedDigits.size());
    for (size_t i = 0; i < parsedDigits.size(); ++i) {
        int targetSlot = startIdx + static_cast<int>(i);
        if (targetSlot >= 0 && targetSlot < m_digitCount) {
            displaySlots[targetSlot] = parsedDigits[i];
        }
    }

    // 3. Compute digit layout
    const double marginX = 14.0;
    const double marginY = 10.0;
    const double availW = std::max(10.0, w - marginX * 2.0);
    const double availH = std::max(10.0, h - marginY * 2.0);

    // Each digit has width D, spacing S, DP space
    // Aspect ratio of 7-segment digit is roughly 0.55 (width/height)
    double targetDigitH = availH;
    double targetDigitW = targetDigitH * 0.52;
    double spacing = targetDigitW * 0.35;

    double totalNeededW = m_digitCount * targetDigitW + (m_digitCount - 1) * spacing;
    if (totalNeededW > availW) {
        // Scale down to fit width
        double scale = availW / totalNeededW;
        targetDigitW *= scale;
        targetDigitH *= scale;
        spacing *= scale;
        totalNeededW = availW;
    }

    double startX = marginX + (availW - totalNeededW) * 0.5;
    double startY = marginY + (availH - targetDigitH) * 0.5;

    for (int i = 0; i < m_digitCount; ++i) {
        QRectF digitRect(startX + i * (targetDigitW + spacing), startY, targetDigitW, targetDigitH);
        quint8 mask = encodeChar(displaySlots[i].ch);
        drawDigit(painter, digitRect, mask, displaySlots[i].hasDp);
    }
}
