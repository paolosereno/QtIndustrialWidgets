// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QtCore/QString>

// Headless export of the documentation images:
//   QtIndustrialWidgetsGallery --export-gallery <output-dir> [--dpr 1|2] [--theme dark|light|both]
// See docs/gallery_images.md.
namespace GalleryExport {

struct Options {
    QString outputDir;
    int dpr{2};
    bool dark{true};
    bool light{false};
};

enum class Request { None, Export, Invalid };

/// Reads the command line. Call it before creating the QApplication: in export mode the caller
/// sets QT_QPA_PLATFORM (offscreen, if unset) and QT_SCALE_FACTOR (= dpr) first.
Request parseArguments(int argc, char *argv[], Options *options, QString *error);

/// Writes every widget showcase and every gallery tab; prints one line per file.
/// Returns 0 on success, non-zero if any image fails (the message names the widget or tab).
int run(const Options &options);

} // namespace GalleryExport
