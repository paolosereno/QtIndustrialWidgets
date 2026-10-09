/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>

/**
 * \class SevenSegmentDisplay
 * \brief Realistic electronic 7-segment LED/LCD numeric display widget.
 *
 * SevenSegmentDisplay emulates multi-digit electronic digital panel meters, counters, and digital clocks.
 * Features include:
 * - Geometric vector segment rendering with customizable italic skew angle and segment stroke width.
 * - Realistic "ghost" unlit segment glow effect via inactiveSegmentColor.
 * - Decimal point placement, leading zero blanking or padding, and arbitrary ASCII character approximations.
 * - Drop-in QLCDNumber replacement with extended modern industrial styling properties.
 *
 * \code
 * auto *display = new SevenSegmentDisplay(parent);
 * display->setDigitCount(6);
 * display->setDecimalPlaces(2);
 * display->display(123.45);
 * display->setActiveSegmentColor(QColor(0, 229, 255)); // Neon Cyan
 * \endcode
 */
namespace QtIndustrialWidgets {

class QTINDUSTRIALWIDGETS_EXPORT SevenSegmentDisplay : public QWidget
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
    /**
     * \brief Constructs a SevenSegmentDisplay widget with 5 digits and cyan LED styling.
     * \param parent Optional parent widget.
     */
    explicit SevenSegmentDisplay(QWidget *parent = nullptr);
    ~SevenSegmentDisplay() override = default;

    /** \brief Returns the currently displayed numeric value. */
    [[nodiscard]] double value() const { return m_value; }
    /** \brief Returns the currently formatted display text string. */
    [[nodiscard]] QString text() const { return m_text; }
    /** \brief Returns the total number of digits displayed. */
    [[nodiscard]] int digitCount() const { return m_digitCount; }
    /** \brief Returns the number of decimal digits after the decimal point. */
    [[nodiscard]] int decimalPlaces() const { return m_decimalPlaces; }
    /** \brief Returns true if leading zeros are displayed instead of blanked. */
    [[nodiscard]] bool showLeadingZeros() const { return m_showLeadingZeros; }
    /** \brief Returns true if the decimal point separator is shown. */
    [[nodiscard]] bool showDecimalPoint() const { return m_showDecimalPoint; }
    /** \brief Returns the italic forward skew slant angle in degrees. */
    [[nodiscard]] double skewAngle() const { return m_skewAngle; }
    /** \brief Returns the segment stroke thickness ratio relative to digit width. */
    [[nodiscard]] double segmentWidthRatio() const { return m_segmentWidthRatio; }

    /** \brief Returns the illuminated active segment color. */
    [[nodiscard]] QColor activeSegmentColor() const { return m_activeSegmentColor; }
    /** \brief Returns the unlit ghost segment shadow color. */
    [[nodiscard]] QColor inactiveSegmentColor() const { return m_inactiveSegmentColor; }
    /** \brief Returns the display panel background color. */
    [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
    /** \brief Returns the outer rim bezel frame color. */
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    /** \brief Returns true if the outer bezel frame is visible. */
    [[nodiscard]] bool isBezelVisible() const { return m_bezelVisible; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the numeric value to display. */
    void setValue(double value);
    /** \brief Sets arbitrary text or numeric characters to display. */
    void setText(const QString &text);
    /** \brief Convenience slot to display a double (compatible with QLCDNumber). */
    void display(double value);
    /** \brief Convenience slot to display an integer (compatible with QLCDNumber). */
    void display(int value);
    /** \brief Convenience slot to display a string (compatible with QLCDNumber). */
    void display(const QString &text);

    /** \brief Sets the total digit capacity. */
    void setDigitCount(int count);
    /** \brief Sets the decimal places for floating-point formatting. */
    void setDecimalPlaces(int places);
    /** \brief Toggles leading zero padding vs blanking. */
    void setShowLeadingZeros(bool show);
    /** \brief Toggles decimal point visibility. */
    void setShowDecimalPoint(bool show);
    /** \brief Sets the italic skew angle in degrees (0 = vertical). */
    void setSkewAngle(double angle);
    /** \brief Sets the segment thickness ratio relative to width. */
    void setSegmentWidthRatio(double ratio);

    /** \brief Sets the illuminated segment color. */
    void setActiveSegmentColor(const QColor &color);
    /** \brief Sets the unlit ghost segment shadow color. */
    void setInactiveSegmentColor(const QColor &color);
    /** \brief Sets the LCD background glass color. */
    void setBackgroundColor(const QColor &color);
    /** \brief Sets the outer chassis bezel frame color. */
    void setBezelColor(const QColor &color);
    /** \brief Toggles visibility of the outer bezel frame. */
    void setBezelVisible(bool visible);

Q_SIGNALS:
    /** \brief Emitted when the numeric value changes. */
    void valueChanged(double value);
    /** \brief Emitted when the displayed text string changes. */
    void textChanged(const QString &text);
    /** \brief Emitted when visual appearance styling properties change. */
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

} // namespace QtIndustrialWidgets
