// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "GalleryExport.h"

#include "MainWindow.h"
#include "Showcases.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtGui/QImage>
#include <QtGui/QImageWriter>
#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

#include <cstdio>
#include <memory>

namespace GalleryExport {

namespace {

constexpr QSize kWindowSize(1200, 820);

void printLine(const QString &line) { std::printf("%s\n", qPrintable(line)); std::fflush(stdout); }
void printError(const QString &line) { std::fprintf(stderr, "error: %s\n", qPrintable(line)); }

bool writePng(const QImage &image, const QString &path, QString *error)
{
    QImageWriter writer(path, "png");
    writer.setQuality(0); // PNG: maximum compression
    if (!writer.write(image)) {
        *error = writer.errorString();
        return false;
    }
    return true;
}

// Renders a showcase right after it is configured, without running the event loop: no timer
// (blinking, peak decay, flashing) can fire, so the image is the same on every run.
bool exportShowcase(const GalleryShowcase::Showcase &showcase, GalleryShowcase::Theme theme,
                    const Options &options, QString *path, QString *error)
{
    std::unique_ptr<QWidget> widget(showcase.create(theme));
    if (!widget) {
        *error = QStringLiteral("could not be created");
        return false;
    }
    widget->ensurePolished();
    const QSize size = showcase.size.isValid() ? showcase.size : widget->sizeHint();
    widget->resize(size);
    QImage image(size * options.dpr, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(options.dpr);
    image.fill(GalleryShowcase::backgroundColor(theme));
    {
        QPainter painter(&image);
        widget->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    *path = QStringLiteral("%1/%2.png").arg(options.outputDir, showcase.fileStem(theme));
    return writePng(image, *path, error);
}

} // namespace

Request parseArguments(int argc, char *argv[], Options *options, QString *error)
{
    bool requested = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        const auto next = [&](QString *value) {
            if (i + 1 >= argc) {
                *error = QStringLiteral("%1 needs a value").arg(arg);
                return false;
            }
            *value = QString::fromLocal8Bit(argv[++i]);
            return true;
        };
        QString value;
        if (arg == QLatin1String("--export-gallery")) {
            if (!next(&value)) return Request::Invalid;
            options->outputDir = value;
            requested = true;
        } else if (arg == QLatin1String("--dpr")) {
            if (!next(&value)) return Request::Invalid;
            if (value != QLatin1String("1") && value != QLatin1String("2")) {
                *error = QStringLiteral("--dpr must be 1 or 2, not \"%1\"").arg(value);
                return Request::Invalid;
            }
            options->dpr = value.toInt();
        } else if (arg == QLatin1String("--theme")) {
            if (!next(&value)) return Request::Invalid;
            if (value == QLatin1String("dark")) {
                options->dark = true;
                options->light = false;
            } else if (value == QLatin1String("light")) {
                options->dark = false;
                options->light = true;
            } else if (value == QLatin1String("both")) {
                options->dark = true;
                options->light = true;
            } else {
                *error = QStringLiteral("--theme must be dark, light or both, not \"%1\"").arg(value);
                return Request::Invalid;
            }
        }
    }
    if (!requested) {
        return Request::None;
    }
    return Request::Export;
}

int run(const Options &options)
{
    if (!QDir().mkpath(options.outputDir)) {
        printError(QStringLiteral("cannot create the output directory \"%1\"").arg(options.outputDir));
        return 1;
    }
    QList<GalleryShowcase::Theme> themes;
    if (options.dark) themes << GalleryShowcase::Theme::Dark;
    if (options.light) themes << GalleryShowcase::Theme::Light;

    int failures = 0;
    for (const GalleryShowcase::Theme theme : themes) {
        for (const GalleryShowcase::Showcase &showcase : GalleryShowcase::showcases()) {
            QString path;
            QString error;
            if (exportShowcase(showcase, theme, options, &path, &error)) {
                printLine(path);
            } else {
                printError(QStringLiteral("%1 (%2): %3").arg(showcase.widgetClass, showcase.variant, error));
                ++failures;
            }
        }
    }

    MainWindow window;
    window.prepareForExport();
    window.resize(kWindowSize);
    window.show();
    QCoreApplication::sendPostedEvents(); // layouts and polish; no timer events
    for (const GalleryShowcase::Theme theme : themes) {
        QStringList written;
        QString error;
        const bool ok = window.exportTabImages(options.outputDir, options.dpr,
                                               theme == GalleryShowcase::Theme::Dark, &written, &error);
        for (const QString &path : written) {
            printLine(path);
        }
        if (!ok) {
            printError(QStringLiteral("gallery %1").arg(error));
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

} // namespace GalleryExport
