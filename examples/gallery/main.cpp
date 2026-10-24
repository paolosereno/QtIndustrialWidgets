// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "GalleryExport.h"
#include "MainWindow.h"
#include <QtWidgets/QApplication>

#include <cstdio>

int main(int argc, char *argv[])
{
    // --export-gallery: headless documentation image export (docs/gallery_images.md). The
    // platform and the device pixel ratio must be chosen before the QApplication exists.
    GalleryExport::Options exportOptions;
    QString exportError;
    const GalleryExport::Request request = GalleryExport::parseArguments(argc, argv, &exportOptions, &exportError);
    if (request == GalleryExport::Request::Invalid) {
        std::fprintf(stderr, "error: %s\n", qPrintable(exportError));
        return 2;
    }
    if (request == GalleryExport::Request::Export) {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        qputenv("QT_SCALE_FACTOR", QByteArray::number(exportOptions.dpr));
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QtIndustrialWidgets Showcase Gallery"));
    app.setOrganizationName(QStringLiteral("QtIndustrialWidgets"));

    if (request == GalleryExport::Request::Export) {
        return GalleryExport::run(exportOptions);
    }

    MainWindow window;
    window.show();

    return app.exec();
}
