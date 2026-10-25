/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <memory>
#include <QtGui/QColor>

namespace QtIndustrialWidgets {

class SevenSegmentDisplayPrivate;

/**
 * \class SevenSegmentDisplay
 * \brief Seven-segment numeric display.
 *
 * SevenSegmentDisplay draws multi-digit seven-segment readouts, as on panel meters, counters, and digital clocks.
 * Features include:
 * - Segments painted as polygons, with an adjustable skew angle and segment width.
 * - Unlit segments drawn in inactiveSegmentColor (by default the lit color at low opacity).
 * - Decimal point placement, leading zero blanking or padding. setText() shows digits, '-', '_', '=', space and
 *   the letter patterns A, b, C, c, d, E, F, H, L, o, P, r, U; any other character is drawn blank.
 * - display(double), display(int) and display(const QString &) slots with the signatures of QLCDNumber's; the
 *   class derives from QWidget, not from QLCDNumber, and the rest of its API differs.
 *
 * \code
 * auto *display = new SevenSegmentDisplay(parent);
 * display->setDigitCount(6);
 * display->setDecimalPlaces(2);
 * display->display(123.45);
 * display->setActiveSegmentColor(QColor(0, 229, 255)); // the default cyan
 * \endcode
 */
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
     * \brief Constructs a SevenSegmentDisplay widget with 5 digits and cyan segments.
     * \param parent Optional parent widget.
     */
    explicit SevenSegmentDisplay(QWidget *parent = nullptr);
    ~SevenSegmentDisplay() override;

    /** \brief Returns the currently displayed numeric value. */
    [[nodiscard]] double value() const;
    /** \brief Returns the currently formatted display text string. */
    [[nodiscard]] QString text() const;
    /** \brief Returns the total number of digits displayed. */
    [[nodiscard]] int digitCount() const;
    /** \brief Returns the number of decimal digits after the decimal point. */
    [[nodiscard]] int decimalPlaces() const;
    /** \brief Returns true if leading zeros are displayed instead of blanked. */
    [[nodiscard]] bool showLeadingZeros() const;
    /** \brief Returns true if the decimal point separator is shown. */
    [[nodiscard]] bool showDecimalPoint() const;
    /** \brief Returns the italic forward skew slant angle in degrees. */
    [[nodiscard]] double skewAngle() const;
    /** \brief Returns the segment stroke thickness ratio relative to digit width. */
    [[nodiscard]] double segmentWidthRatio() const;

    /** \brief Returns the illuminated active segment color. */
    [[nodiscard]] QColor activeSegmentColor() const;
    /** \brief Returns the unlit ghost segment shadow color. */
    [[nodiscard]] QColor inactiveSegmentColor() const;
    /** \brief Returns the display panel background color. */
    [[nodiscard]] QColor backgroundColor() const;
    /** \brief Returns the outer rim bezel frame color. */
    [[nodiscard]] QColor bezelColor() const;
    /** \brief Returns true if the outer bezel frame is visible. */
    [[nodiscard]] bool isBezelVisible() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the numeric value to display. */
    void setValue(double value);
    /** \brief Sets arbitrary text or numeric characters to display. */
    void setText(const QString &text);
    /** \brief Convenience slot to display a double (same signature as QLCDNumber::display(double)). */
    void display(double value);
    /** \brief Convenience slot to display an integer (same signature as QLCDNumber::display(int)). */
    void display(int value);
    /** \brief Convenience slot to display a string (same signature as QLCDNumber::display(const QString &)). */
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
    /** \brief Sets the display panel background color. */
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

    std::unique_ptr<SevenSegmentDisplayPrivate> d_ptr;
    Q_DECLARE_PRIVATE(SevenSegmentDisplay)
};

} // namespace QtIndustrialWidgets
