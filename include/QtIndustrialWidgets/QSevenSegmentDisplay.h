#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>

class QTINDUSTRIALWIDGETS_EXPORT QSevenSegmentDisplay : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int digitCount READ digitCount WRITE setDigitCount NOTIFY appearanceChanged)
    Q_PROPERTY(int decimalPlaces READ decimalPlaces WRITE setDecimalPlaces NOTIFY appearanceChanged)
    Q_PROPERTY(bool showLeadingZeros READ showLeadingZeros WRITE setShowLeadingZeros NOTIFY appearanceChanged)
    Q_PROPERTY(bool showDecimalPoint READ showDecimalPoint WRITE setShowDecimalPoint NOTIFY appearanceChanged)
    Q_PROPERTY(double skewAngle READ skewAngle WRITE setSkewAngle NOTIFY appearanceChanged)
    Q_PROPERTY(double segmentWidthRatio READ segmentWidthRatio WRITE setSegmentWidthRatio NOTIFY appearanceChanged)
    Q_PROPERTY(QColor activeSegmentColor READ activeSegmentColor WRITE setActiveSegmentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor inactiveSegmentColor READ inactiveSegmentColor WRITE setInactiveSegmentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)
    Q_PROPERTY(bool bezelVisible READ isBezelVisible WRITE setBezelVisible NOTIFY appearanceChanged)

public:
    explicit QSevenSegmentDisplay(QWidget *parent = nullptr);
    ~QSevenSegmentDisplay() override = default;

    [[nodiscard]] double value() const { return m_value; }
    [[nodiscard]] QString text() const { return m_text; }
    [[nodiscard]] int digitCount() const { return m_digitCount; }
    [[nodiscard]] int decimalPlaces() const { return m_decimalPlaces; }
    [[nodiscard]] bool showLeadingZeros() const { return m_showLeadingZeros; }
    [[nodiscard]] bool showDecimalPoint() const { return m_showDecimalPoint; }
    [[nodiscard]] double skewAngle() const { return m_skewAngle; }
    [[nodiscard]] double segmentWidthRatio() const { return m_segmentWidthRatio; }

    [[nodiscard]] QColor activeSegmentColor() const { return m_activeSegmentColor; }
    [[nodiscard]] QColor inactiveSegmentColor() const { return m_inactiveSegmentColor; }
    [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    [[nodiscard]] bool isBezelVisible() const { return m_bezelVisible; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setValue(double value);
    void setText(const QString &text);
    void display(double value);
    void display(int value);
    void display(const QString &text);

    void setDigitCount(int count);
    void setDecimalPlaces(int places);
    void setShowLeadingZeros(bool show);
    void setShowDecimalPoint(bool show);
    void setSkewAngle(double angle);
    void setSegmentWidthRatio(double ratio);

    void setActiveSegmentColor(const QColor &color);
    void setInactiveSegmentColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setBezelColor(const QColor &color);
    void setBezelVisible(bool visible);

Q_SIGNALS:
    void valueChanged(double value);
    void textChanged(const QString &text);
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateFormattedText();
    static quint8 encodeChar(QChar ch);
    void drawDigit(QPainter &painter, const QRectF &rect, quint8 mask, bool hasDecimalPoint) const;

    double m_value{0.0};
    QString m_text{QStringLiteral("0.0")};
    int m_digitCount{5};
    int m_decimalPlaces{1};
    bool m_showLeadingZeros{false};
    bool m_showDecimalPoint{true};
    double m_skewAngle{8.0};             // Degrees italic skew
    double m_segmentWidthRatio{0.14};    // Thickness relative to width

    QColor m_activeSegmentColor{QColor(0, 229, 255)};         // Neon Cyan LED
    QColor m_inactiveSegmentColor{QColor(0, 229, 255, 28)};    // Dim unlit ghost
    QColor m_backgroundColor{QColor(16, 20, 28)};             // Dark LCD Panel
    QColor m_bezelColor{QColor(40, 48, 60)};                  // Bezel rim
    bool m_bezelVisible{true};
    bool m_isTextExplicit{false};
};
