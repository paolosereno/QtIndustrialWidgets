/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtIndustrialWidgets/ValueState.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <memory>

namespace QtIndustrialWidgets {

class ValveIndicatorPrivate;
namespace internal {
class ValveIndicatorTestAccess;
}

/**
 * \class ValveIndicator
 * \brief Valve symbol that shows the status of a valve and, optionally, its opening (#153).
 *
 * The symbol follows common P&ID practice: two triangles tip to tip (a gate valve), with a circle at the centre
 * (a ball valve), with a line through the centre (a butterfly valve), or with a stem and a diaphragm actuator
 * (a control valve). orientation() turns it: \c Qt::Horizontal has the flow from left to right and the actuator on
 * top, \c Qt::Vertical the flow from the top down and the actuator on the right. pipeStubsVisible() adds a short
 * pipe on both sides (off by default).
 *
 * ValveIndicator is an indicator only: it shows what the application tells it and sends no command.
 *
 * Status (status()), told by the shape first and by the colour second, so that no status depends on colour:
 * - \c Unknown (the default of a new widget): the body hatched with diagonal lines, and a "?" tag;
 * - \c Closed: the body filled;
 * - \c Open: the body outlined, empty;
 * - \c Transit: the upstream half of the body filled, and a dashed outline;
 * - \c Fault: a cross over the body, and a "FAULT" tag.
 *
 * Opening (opening(), 0 to 100 %): an opening bar and a readout under the symbol. A control valve always shows them;
 * for the other types they are shown when openingVisible() is true (false by default). The input is kept unclamped
 * (rawOpening()) and classified as for the gauges (openingState(), #151): out of range or ±Inf, the bar stops at
 * the bound and the readout shows the input after a triangle; NaN gives no bar and "---". A new ValveIndicator has
 * no opening: rawOpening() and opening() are NaN and openingState() is \c Invalid until the first setOpening().
 * The readout text is never below 7 pt: without room the unit goes first, then the number, never the triangle or
 * "---".
 *
 * status() and opening() are independent: the widget never derives one from the other, and shows, for example, a
 * \c Closed status with a 40 % opening if it is told so. Keeping them consistent is the application's job.
 *
 * Colours: openColor(), closedColor(), transitColor(), faultColor() and unknownColor() draw the body in each status.
 * The defaults are only a default choice: the conventions differ between sectors (green for open and red for closed
 * in some, the reverse in others), so pick the colours of the plant. The meaning is carried by the shape, never by
 * the colour alone.
 *
 * minimumSizeHint() is computed from the font, the logical DPI and the configuration (type, orientation, stubs,
 * opening), and follows a change of screen (#177).
 *
 * \code
 * auto *valve = new ValveIndicator(parent);
 * valve->setValveType(ValveIndicator::ValveType::Control);
 * valve->setStatus(ValveIndicator::ValveStatus::Open);
 * valve->setOpening(42.0);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT ValveIndicator : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(ValveType valveType READ valveType WRITE setValveType NOTIFY appearanceChanged)
    Q_PROPERTY(ValveStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(double opening READ opening WRITE setOpening NOTIFY openingChanged)
    Q_PROPERTY(double rawOpening READ rawOpening)
    Q_PROPERTY(QtIndustrialWidgets::ValueState openingState READ openingState NOTIFY openingStateChanged)
    Q_PROPERTY(bool openingVisible READ openingVisible WRITE setOpeningVisible NOTIFY appearanceChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool pipeStubsVisible READ pipeStubsVisible WRITE setPipeStubsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor openColor READ openColor WRITE setOpenColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor closedColor READ closedColor WRITE setClosedColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor transitColor READ transitColor WRITE setTransitColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor faultColor READ faultColor WRITE setFaultColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor unknownColor READ unknownColor WRITE setUnknownColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor readoutColor READ readoutColor WRITE setReadoutColor NOTIFY appearanceChanged)

public:
    /// The symbol.
    enum class ValveType {
        Gate,      ///< Two triangles tip to tip
        Ball,      ///< Two triangles and a circle at the centre
        Butterfly, ///< Two triangles and a line through the centre (the disc)
        Control    ///< Two triangles, a stem and a diaphragm actuator; the opening is always shown
    };
    Q_ENUM(ValveType)

    /// The status, as the application reports it.
    enum class ValveStatus {
        Unknown, ///< Not known (the default): hatched body and "?"
        Closed,  ///< Filled body
        Open,    ///< Outlined body
        Transit, ///< Moving: upstream half filled, dashed outline
        Fault    ///< Cross over the body and "FAULT"
    };
    Q_ENUM(ValveStatus)

    explicit ValveIndicator(QWidget *parent = nullptr);
    ~ValveIndicator() override;

    /** \brief Returns the symbol. Default ValveType::Gate. */
    [[nodiscard]] ValveType valveType() const;
    /** \brief Returns the status. Default ValveStatus::Unknown. */
    [[nodiscard]] ValveStatus status() const;
    /**
     * \brief Returns the opening shown: the last input clamped to [0, 100] (±Inf gives the bound), or NaN when the
     * input is NaN or before the first setOpening() (openingState() is then ValueState::Invalid).
     */
    [[nodiscard]] double opening() const;
    /** \brief Returns the last opening input as it was given, unclamped (#151); NaN before the first setOpening(). */
    [[nodiscard]] double rawOpening() const;
    /** \brief Returns the state of the last opening input against [0, 100] (#151); \c Invalid before the first one. */
    [[nodiscard]] QtIndustrialWidgets::ValueState openingState() const;
    /**
     * \brief Returns true if the opening bar and readout are shown for a gate, ball or butterfly valve. Default
     * false. A control valve always shows them.
     */
    [[nodiscard]] bool openingVisible() const;
    /** \brief Returns the decimals of the opening readout. Default 0. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the orientation of the flow. Default Qt::Horizontal. */
    [[nodiscard]] Qt::Orientation orientation() const;
    /** \brief Returns true if the pipe stubs are drawn. Default false. */
    [[nodiscard]] bool pipeStubsVisible() const;

    /** \brief Returns the colour of the body of an open valve (its outline). Default QColor(46, 204, 113). */
    [[nodiscard]] QColor openColor() const;
    /** \brief Returns the colour of the body of a closed valve. Default QColor(231, 76, 60). */
    [[nodiscard]] QColor closedColor() const;
    /** \brief Returns the colour of the body of a valve in transit. Default QColor(241, 196, 15). */
    [[nodiscard]] QColor transitColor() const;
    /** \brief Returns the colour of the body and of the cross of a valve in fault. Default QColor(217, 70, 239). */
    [[nodiscard]] QColor faultColor() const;
    /** \brief Returns the colour of the body and of the hatch of an unknown status. Default QColor(149, 165, 166). */
    [[nodiscard]] QColor unknownColor() const;
    /**
     * \brief Returns the colour of the stubs, of the marks of the type (disc, stem, actuator) and of the opening bar.
     * Default QColor(200, 208, 218).
     */
    [[nodiscard]] QColor outlineColor() const;
    /** \brief Returns the colour of the status tags ("?", "FAULT"). Default QColor(240, 244, 248). */
    [[nodiscard]] QColor textColor() const;
    /**
     * \brief Returns the colour of the opening readout (#159). Its panel is opaque and dark in every theme, so this
     * colour does not follow textColor(). Default QColor(240, 244, 248).
     */
    [[nodiscard]] QColor readoutColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the symbol. */
    void setValveType(ValveType type);
    /** \brief Sets the status. It does not change the opening. */
    void setStatus(ValveStatus status);
    /**
     * \brief Sets the opening input, in %. opening() is the input clamped within [0, 100], and NaN for a NaN input;
     * rawOpening() keeps the input and openingState() classifies it (#151). It does not change the status.
     * openingChanged() is emitted when opening() changes, then openingStateChanged() when the state changes.
     */
    void setOpening(double opening);
    /** \brief Shows or hides the opening of a gate, ball or butterfly valve (a control valve always shows it). */
    void setOpeningVisible(bool visible);
    /** \brief Sets the decimals of the opening readout (negative values give 0). */
    void setPrecision(int precision);
    /** \brief Sets the orientation of the flow. */
    void setOrientation(Qt::Orientation orientation);
    /** \brief Shows or hides the pipe stubs. */
    void setPipeStubsVisible(bool visible);

    /** \brief Sets the colour of an open valve. */
    void setOpenColor(const QColor &color);
    /** \brief Sets the colour of a closed valve. */
    void setClosedColor(const QColor &color);
    /** \brief Sets the colour of a valve in transit. */
    void setTransitColor(const QColor &color);
    /** \brief Sets the colour of a valve in fault. */
    void setFaultColor(const QColor &color);
    /** \brief Sets the colour of an unknown status. */
    void setUnknownColor(const QColor &color);
    /** \brief Sets the colour of the stubs, of the marks of the type and of the opening bar. */
    void setOutlineColor(const QColor &color);
    /** \brief Sets the colour of the status tags. */
    void setTextColor(const QColor &color);
    /** \brief Sets the colour of the opening readout. */
    void setReadoutColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when status() changes. */
    void statusChanged(QtIndustrialWidgets::ValveIndicator::ValveStatus status);
    /** \brief Emitted when opening() changes. */
    void openingChanged(double opening);
    /** \brief Emitted when openingState() changes, after openingChanged() when both change. */
    void openingStateChanged(QtIndustrialWidgets::ValueState state);
    /** \brief Emitted when a visual property changes. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ValveIndicatorPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ValveIndicator)
    friend class internal::ValveIndicatorTestAccess;
};

} // namespace QtIndustrialWidgets
