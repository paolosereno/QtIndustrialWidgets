// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// Every public widget must have a showcase in examples/gallery/Showcases.cpp, the list the
// documentation image export (--export-gallery) renders. A new widget comes with its showcase.
#include <QtTest/QtTest>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtWidgets/QWidget>

#include "Showcases.h"

#include <memory>

class tst_GalleryShowcases : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyPublicWidgetHasAShowcase();
    void showcasesBuildTheirWidget_data();
    void showcasesBuildTheirWidget();
    void fileNamesAreStableAndUnique();
};

namespace {

// Widget classes declared in the public headers, read from the source tree at run time
QStringList publicWidgetClasses()
{
    const QDir dir(QStringLiteral(QIW_PUBLIC_HEADER_DIR));
    const QRegularExpression declaration(
        QStringLiteral(R"(class\s+QTINDUSTRIALWIDGETS_EXPORT\s+(\w+)\s*:\s*public\s+QWidget\b)"));
    QStringList classes;
    for (const QString &name : dir.entryList({QStringLiteral("*.h")}, QDir::Files, QDir::Name)) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll());
        auto it = declaration.globalMatch(text);
        while (it.hasNext()) {
            classes << it.next().captured(1);
        }
    }
    return classes;
}

} // namespace

void tst_GalleryShowcases::everyPublicWidgetHasAShowcase()
{
    const QStringList classes = publicWidgetClasses();
    QVERIFY2(classes.size() >= 11, qPrintable(QStringLiteral("only %1 widget classes found in %2")
                                                  .arg(classes.size()).arg(QStringLiteral(QIW_PUBLIC_HEADER_DIR))));
    QSet<QString> covered;
    for (const GalleryShowcase::Showcase &s : GalleryShowcase::showcases()) {
        covered.insert(s.widgetClass);
    }
    QStringList missing;
    for (const QString &c : classes) {
        if (!covered.contains(c)) {
            missing << c;
        }
    }
    QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("no showcase in examples/gallery/Showcases.cpp for: %1")
                                               .arg(missing.join(QStringLiteral(", ")))));
}

void tst_GalleryShowcases::showcasesBuildTheirWidget_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("dark");
    const auto &all = GalleryShowcase::showcases();
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        for (bool dark : {true, false}) {
            QTest::newRow(qPrintable(all[static_cast<size_t>(i)].fileStem(dark ? GalleryShowcase::Theme::Dark
                                                                                 : GalleryShowcase::Theme::Light)))
                << i << dark;
        }
    }
}

void tst_GalleryShowcases::showcasesBuildTheirWidget()
{
    QFETCH(int, index);
    QFETCH(bool, dark);
    const GalleryShowcase::Showcase &s = GalleryShowcase::showcases()[static_cast<size_t>(index)];
    std::unique_ptr<QWidget> widget(s.create(dark ? GalleryShowcase::Theme::Dark : GalleryShowcase::Theme::Light));
    QVERIFY(widget);
    QCOMPARE(QString::fromLatin1(widget->metaObject()->className()),
             QStringLiteral("QtIndustrialWidgets::") + s.widgetClass);
    const QSize size = s.size.isValid() ? s.size : widget->sizeHint();
    QVERIFY(size.width() > 0 && size.height() > 0);
    // Showcases are rendered at their widget's minimum size or larger (#44)
    const QSize min = widget->minimumSizeHint();
    QVERIFY2(size.width() >= min.width() && size.height() >= min.height(),
             qPrintable(QStringLiteral("%1 is rendered at %2x%3, below its minimumSizeHint %4x%5")
                            .arg(s.fileStem(GalleryShowcase::Theme::Dark)).arg(size.width()).arg(size.height()).arg(min.width()).arg(min.height())));
}

void tst_GalleryShowcases::fileNamesAreStableAndUnique()
{
    const QRegularExpression kebab(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    QSet<QString> stems;
    for (const GalleryShowcase::Showcase &s : GalleryShowcase::showcases()) {
        for (const GalleryShowcase::Theme theme : {GalleryShowcase::Theme::Dark, GalleryShowcase::Theme::Light}) {
            const QString stem = s.fileStem(theme);
            QVERIFY2(kebab.match(stem).hasMatch(), qPrintable(stem));
            QVERIFY2(!stems.contains(stem), qPrintable(QStringLiteral("duplicate file name %1").arg(stem)));
            stems.insert(stem);
        }
    }
    QCOMPARE(GalleryShowcase::kebabCase(QStringLiteral("SevenSegmentDisplay")), QStringLiteral("seven-segment-display"));
}

QTEST_MAIN(tst_GalleryShowcases)
#include "tst_GalleryShowcases.moc"
