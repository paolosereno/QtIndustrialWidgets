// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>

#include <functional>
#include <vector>

class QWidget;

// Showcase configurations of every widget: the single source used by the documentation image
// export (--export-gallery) and by tst_GalleryShowcases, which checks that every public widget
// has at least one.
namespace GalleryShowcase {

enum class Theme { Dark, Light };

struct Showcase {
    QString widgetClass; ///< Class name in namespace QtIndustrialWidgets, e.g. "RadialGauge"
    QString variant;     ///< Kebab-case variant, e.g. "warning"
    QSize size;          ///< Logical size of the rendered widget; QSize() means the widget's sizeHint()
    /// Creates the configured widget (no parent). It must not need the event loop: the export
    /// renders it right away, so timers (blinking, peak decay) never fire and the output is fixed.
    std::function<QWidget *(Theme)> create;

    /// File name without extension, e.g. "radial-gauge-warning" (+ "-light" for the light theme)
    [[nodiscard]] QString fileStem(Theme theme) const;
};

/// All showcases, in a stable order.
const std::vector<Showcase> &showcases();

/// Background the showcase images are rendered on (the gallery's group-box colour).
QColor backgroundColor(Theme theme);

/// "RadialGauge" -> "radial-gauge"
QString kebabCase(const QString &className);

} // namespace GalleryShowcase
