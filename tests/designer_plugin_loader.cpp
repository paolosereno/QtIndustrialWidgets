// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// Loads a Qt Designer plugin as Qt Designer does and creates every widget it offers (#127).
// It does not link QtIndustrialWidgets: the plugin must find the library by itself.
#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtCore/QPluginLoader>

#include <cstdio>
#include <memory>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: designer_plugin_loader <plugin file>\n");
        return 2;
    }
    QApplication app(argc, argv);

    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    if (!loader.load()) {
        std::fprintf(stderr, "load failed: %s\n", qPrintable(loader.errorString()));
        return 1;
    }
    auto *collection = qobject_cast<QDesignerCustomWidgetCollectionInterface *>(loader.instance());
    if (!collection) {
        std::fprintf(stderr, "the plugin offers no QDesignerCustomWidgetCollectionInterface\n");
        return 1;
    }

    const QList<QDesignerCustomWidgetInterface *> widgets = collection->customWidgets();
    int created = 0;
    for (QDesignerCustomWidgetInterface *widget : widgets) {
        std::unique_ptr<QWidget> instance(widget->createWidget(nullptr));
        if (instance && QString::fromLatin1(instance->metaObject()->className()) == widget->name()) {
            ++created;
        } else {
            std::fprintf(stderr, "%s: not created with the expected class\n", qPrintable(widget->name()));
        }
    }
    std::printf("%d widgets offered, %d created with the expected class\n", static_cast<int>(widgets.size()),
                created);
    return (created > 0 && created == widgets.size()) ? 0 : 1;
}
