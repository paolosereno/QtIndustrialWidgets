// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #274: the application font of every test, named instead of left to the platform. The offscreen platform asks for
// the generic family "Sans Serif": fontconfig (Linux) resolves it to DejaVu Sans, while the font database of the
// offscreen platform on Windows, with the DejaVu files of the Linux job in QT_QPA_FONTDIR and no fontconfig, resolves
// it to DejaVu Math TeX Gyre. Linked into every test registered by add_dpr_tests() (tests/CMakeLists.txt)
#include <QtCore/QCoreApplication>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>

namespace {
void setTestApplicationFont()
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr) return; // QTEST_GUILESS_MAIN
    QFont font(QStringLiteral("DejaVu Sans"));
    font.setPointSizeF(9.0);
    QGuiApplication::setFont(font); // before any widget: QApplication::font() is this font
}
} // namespace

Q_COREAPP_STARTUP_FUNCTION(setTestApplicationFont)
