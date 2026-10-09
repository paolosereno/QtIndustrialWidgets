// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QtIndustrialWidgets Showcase Gallery"));
    app.setOrganizationName(QStringLiteral("QtIndustrialWidgets"));

    MainWindow window;
    window.show();

    return app.exec();
}
