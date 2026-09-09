/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <KConfig>
#include <KConfigGroup>
#include <QKeySequence>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>

#include <PkConfigGroup.h>
#include <PkString.h>

#include <KoToolFactoryBase.h>
#include <KoToolRegistry.h>

#include "selection_tools.h"
#include "selection_tool_cursor.h"

class SelectionToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registeredFactoryShortcutsMatchQt515Parser();
    void registeredFactoryShortcutsRetainNativeChords();
    void thresholdFallbackMatchesKConfig();
    void cursorPayloadOracleMatchesAssets();
};

void SelectionToolsTest::registeredFactoryShortcutsMatchQt515Parser()
{
    registerSelectionTools();

    KoToolFactoryBase *rectangular =
        KoToolRegistry::instance()->value(PkString("KisToolSelectRectangular"));
    KoToolFactoryBase *elliptical =
        KoToolRegistry::instance()->value(PkString("KisToolSelectElliptical"));
    QVERIFY(rectangular);
    QVERIFY(elliptical);

    QCOMPARE(rectangular->shortcut()[0],
             QKeySequence(QStringLiteral("Ctrl+R"))[0]);
    QCOMPARE(elliptical->shortcut()[0],
             QKeySequence(QStringLiteral("J"))[0]);
}

void SelectionToolsTest::registeredFactoryShortcutsRetainNativeChords()
{
    registerSelectionTools();

    KoToolFactoryBase *rectangular =
        KoToolRegistry::instance()->value(PkString("KisToolSelectRectangular"));
    KoToolFactoryBase *elliptical =
        KoToolRegistry::instance()->value(PkString("KisToolSelectElliptical"));
    QVERIFY(rectangular);
    QVERIFY(elliptical);

    QCOMPARE(rectangular->shortcut()[0],
             selectionToolShortcutChord(SelectionToolKind::Rectangular));
    QCOMPARE(elliptical->shortcut()[0],
             selectionToolShortcutChord(SelectionToolKind::Elliptical));
}

void SelectionToolsTest::thresholdFallbackMatchesKConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    KConfig reference(directory.filePath(QStringLiteral("selectionrc")),
                      KConfig::SimpleConfig);
    KConfigGroup referenceGroup(&reference, QStringLiteral("selection-oracle"));
    PkConfigGroup nativeGroup(PkString("selection-oracle"));
    nativeGroup.deleteGroup();

    QCOMPARE(readSelectionThreshold(nativeGroup, 8),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 8)));
    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.writeEntry(PkString("fuzziness"), 37);
    referenceGroup.writeEntry("fuzziness", 37);
    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.writeEntry(PkString("threshold"), 61);
    referenceGroup.writeEntry("threshold", 61);
    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.deleteGroup();
}

void SelectionToolsTest::cursorPayloadOracleMatchesAssets()
{
    for (const auto &descriptor : selectionToolCursorDescriptors()) {
        QFile asset(QStringLiteral(SELECTIONTOOLS_SOURCE_DIR) + QLatin1Char('/') +
                    QString::fromUtf8(descriptor.assetPath.data(),
                                      static_cast<int>(descriptor.assetPath.size())));
        QVERIFY2(asset.open(QIODevice::ReadOnly), descriptor.name.data());
        const QByteArray bytes = asset.readAll();
        QCOMPARE(static_cast<std::size_t>(bytes.size()), descriptor.byteCount);
        QCOMPARE(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString(),
                 std::string(descriptor.sha256));
        QVERIFY(descriptor.hotspotX >= 0);
        QVERIFY(descriptor.hotspotY >= 0);
    }
}

SIMPLE_TEST_MAIN(SelectionToolsTest)

#include "selection_tools_test.moc"
