// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>

// #46: fixed-point numbers shown by the widgets go through internal::formatFixed() (and
// internal::scaleLabelText()), which never print a negative zero. This test reads the library
// sources at run time and fails on any QString::number(..., 'f', ...) or .arg(..., 'f', ...)
// outside src/internal/NumberFormat.*, so a new call site cannot bring "-0.0" back.
class tst_FixedFormatUsage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noRawFixedFormattingInSources();
};

void tst_FixedFormatUsage::noRawFixedFormattingInSources()
{
    // Within one statement (no ';' in between): the call, then a 'f' format argument
    const QRegularExpression raw(QStringLiteral(R"((QString::number|\.arg)\s*\([^;]*?'f')"));
    QStringList hits;
    int files = 0;
    QDirIterator it(QStringLiteral(QIW_SOURCE_DIR), {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).completeBaseName() == QLatin1String("NumberFormat")) {
            continue;
        }
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
        const QString text = QString::fromUtf8(file.readAll());
        ++files;
        auto matches = raw.globalMatch(text);
        while (matches.hasNext()) {
            const auto m = matches.next();
            const int line = text.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
            hits << QStringLiteral("%1:%2").arg(QDir(QStringLiteral(QIW_SOURCE_DIR)).relativeFilePath(path)).arg(line);
        }
    }
    QVERIFY2(files > 10, qPrintable(QStringLiteral("only %1 source files found in %2").arg(files).arg(QStringLiteral(QIW_SOURCE_DIR))));
    QVERIFY2(hits.isEmpty(), qPrintable(QStringLiteral("fixed-point formatting outside internal/NumberFormat (use formatFixed()): %1")
                                            .arg(hits.join(QStringLiteral(", ")))));
}

QTEST_MAIN(tst_FixedFormatUsage)
#include "tst_FixedFormatUsage.moc"
