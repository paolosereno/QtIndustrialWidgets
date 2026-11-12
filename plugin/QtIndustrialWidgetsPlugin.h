/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>

// One widget of the collection (#154: one class for every entry, instead of one class per widget). What differs
// between the widgets is in its Entry, defined in QtIndustrialWidgetsPlugin.cpp
class IndustrialWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    struct Entry {
        QString (*name)();
        QString (*includeFile)();
        QString (*toolTip)();
        QString (*whatsThis)();
        QString (*domXml)();
        QIcon (*icon)();
        QWidget *(*createWidget)(QWidget *parent);
    };

    IndustrialWidgetPlugin(const Entry &entry, QObject *parent);

    bool isContainer() const override { return false; }
    bool isInitialized() const override { return m_initialized; }
    QIcon icon() const override;
    QString domXml() const override;
    QString group() const override;
    QString includeFile() const override;
    QString name() const override;
    QString toolTip() const override;
    QString whatsThis() const override;
    QWidget *createWidget(QWidget *parent) override;
    void initialize(QDesignerFormEditorInterface *core) override;

private:
    Entry m_entry;
    bool m_initialized{false};
};

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
