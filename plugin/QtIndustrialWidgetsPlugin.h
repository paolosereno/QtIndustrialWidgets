/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>

// What every entry of the widget box does the same way (#154). Designer reaches an entry through
// qobject_cast<QDesignerCustomWidgetInterface *>, which Q_INTERFACES here answers for every derived entry
class IndustrialWidgetPluginBase : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    explicit IndustrialWidgetPluginBase(QObject *parent = nullptr);

    bool isContainer() const override { return false; }
    bool isInitialized() const override { return m_initialized; }
    QString group() const override;
    void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool m_initialized{false};
};

// One entry: what differs between the widgets, defined in QtIndustrialWidgetsPlugin.cpp. A macro, so that the 16
// declarations are not 16 copies of the same block; internal to the plugin and undefined below
#define QIW_DESIGNER_PLUGIN_ENTRY(Entry)                                                                               \
    class Entry : public IndustrialWidgetPluginBase                                                                    \
    {                                                                                                                  \
    public:                                                                                                            \
        explicit Entry(QObject *parent = nullptr)                                                                      \
            : IndustrialWidgetPluginBase(parent)                                                                       \
        {                                                                                                              \
        }                                                                                                              \
        QIcon icon() const override;                                                                                   \
        QString domXml() const override;                                                                               \
        QString includeFile() const override;                                                                          \
        QString name() const override;                                                                                 \
        QString toolTip() const override;                                                                              \
        QString whatsThis() const override;                                                                            \
        QWidget *createWidget(QWidget *parent) override;                                                               \
    };

QIW_DESIGNER_PLUGIN_ENTRY(RadialGaugePlugin)
QIW_DESIGNER_PLUGIN_ENTRY(LinearGaugePlugin)
QIW_DESIGNER_PLUGIN_ENTRY(SevenSegmentDisplayPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(LedIndicatorPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(IndustrialKnobPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(StripChartPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(AnalysisChartPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(IndustrialSwitchPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(LevelMeterPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(AnnunciatorPanelPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(CompassPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(CircularBargraphPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(TankDisplayPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(ValveIndicatorPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(IndustrialSliderPlugin)
QIW_DESIGNER_PLUGIN_ENTRY(ThumbwheelSwitchPlugin)

#undef QIW_DESIGNER_PLUGIN_ENTRY

class QtIndustrialWidgetsPlugin : public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface")
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
    explicit QtIndustrialWidgetsPlugin(QObject *parent = nullptr);

    QList<QDesignerCustomWidgetInterface *> customWidgets() const override;

private:
    QList<QDesignerCustomWidgetInterface *> m_widgets;
};
