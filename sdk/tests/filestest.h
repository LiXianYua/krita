/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] filestest.h 阻塞登记（S-06 Task 9）
//
// 本文件不进薄壳，保留 Qt 类型。testFiles() 用 QFile/QFileDevice/QFileInfo/
// QStandardPaths/QTemporaryFile 做文件 I/O（PkImage 无文件 I/O），且依赖
// KisDocument/KisImportExportManager（libs/ui + libs/importexport，壳闭包外）。
// PATTERN-1 五处 qApp->processEvents() 已按 sdk/tests/README.md 删除；
// TestUtil::testFiles 默认容差 fuzzy=0、maxNumFailingPixels=0 保持原样。
// 关闭条件：PkImage 文件 I/O（R-15）+ 文档导入导出进壳。

#ifndef FILESTEST
#define FILESTEST

// ---------------------------------------------------------------------------
// R-77：pk 分支端口化。**Qt 栈（未定义 KRITA_TESTSDK_PK_NATIVE）的路径逐字不变**
// ——下面每一处 `#ifndef` 包住的都是「pk 栈拿不到、且 pk 分支用不着」的东西，
// Qt 栈展开后的 include 清单与相对顺序与原文件**完全相同**（做法同 :21-23 对
// testui.h 的既有守卫）。分三类：
//
//  1) Krita 侧的重量级头（`testutil.h` / `kis_debug.h` / `Kis*` / `KoColorSpace`）：
//     KisDocument 一族住在 libs/ui + libs/importexport，在壳闭包外、也不在
//     `kritatestsdk_pk` 的 include 目录表上；只有 `testFiles()` 与四个 `testXxx()`
//     用得到，而那些函数整段归 Qt 栈（见下面的函数级守卫）。
//  2) brief §3.3 点名的 4 个**零使用** include（`QTemporaryFile` / `QApplication` /
//     `kaboutdata.h` / `klocalizedstring.h`）——用量表实测：函数体零使用。
//     注：`<klocalizedstring.h>` 就算不删也能在 pk 栈解析（`kritatestsdk_pk` 的
//     目录表里有 libs/flake/flake/noqt-compat/klocalizedstring.h），删它是 §3.3
//     的清单要求，不是编译需要。
//  3) 文件 I/O 垫片面本身（`QDir` / `QFileInfo` / `QFile` / `QFileDevice` /
//     `QIODevice` / `QStandardPaths`）**两栈都要**，不加守卫——这正是本 Task
//     交付的 sdk/tests/compat/ 垫片族。
//
// `#ifdef Q_OS_UNIX` 那一段（`<unistd.h>`）**刻意一个字不动**：pk 树里
// `Q_OS_UNIX` 全树未定义（实测），所以两栈都会跳过它，端口化后的三个函数也不
// 直接用 POSIX 调用（它们走 QFile/QDir/QStandardPaths 垫片）。
// ---------------------------------------------------------------------------
#include <testutil.h>
#include "testui.h"

#include <QDir>

#ifndef KRITA_TESTSDK_PK_NATIVE
#include <kaboutdata.h>
#include <klocalizedstring.h>
#endif

#include <kis_debug.h>

#include <KisImportExportManager.h>

#include <KisDocument.h>
#include <KisDocumentRegistry.h>
#include <kis_image.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

#ifndef KRITA_TESTSDK_PK_NATIVE
#include <QTemporaryFile>
#endif
#include <QFileInfo>
#ifndef KRITA_TESTSDK_PK_NATIVE
#include <QApplication>
#endif
#include <QFile>
#include <QFileDevice>
#include <QIODevice>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#   include <unistd.h>
#endif

namespace TestUtil
{

#ifdef KRITA_TESTSDK_PK_NATIVE
using ImpexTestString = PkString;
#define FILESTEST_VERIFY(statement) PK_VERIFY(statement)
#define FILESTEST_FAIL(message) PK_FAIL(message)
#define FILESTEST_SKIP(message) PK_SKIP(message)
#else
using ImpexTestString = QString;
#define FILESTEST_VERIFY(statement) QVERIFY(statement)
#define FILESTEST_FAIL(message) QFAIL(message)
#define FILESTEST_SKIP(message) QSKIP(message)
#endif

inline PkString impexApiString(const PkString &text)
{
    return text;
}

// R-77：`QString` 在 pk 栈下是 `#define QString PkString`，所以这个重载展开后与
// 上面那个 `impexApiString(const PkString&)` **签名相同**（redefinition）。它在
// Qt 栈下才是真正独立的第二个重载，故只给 Qt 栈。pk 分支没有任何调用点用它
// （三个端口化函数都不用）。
#ifndef KRITA_TESTSDK_PK_NATIVE
inline PkString impexApiString(const QString &text)
{
    return pkStringFromQString(text);
}
#endif

// ===========================================================================
// R-82 · 函数级守卫已全部撤除（本文件现在**两栈同源**，一个字节不差）。
//
// R-77 当初把这五个函数整段判给 Qt 栈，理由是「它们依赖 KisDocument /
// KisImportExportManager / KoColorSpace，**壳闭包外**」。R-82 判定阶段实测：
// **那个前提不成立** —— `libs/impex` 的 KisDocument/KisImportExportManager/
// KisDocumentRegistry 已经是 pk 类型（Qt 只在 `$<LINK_ONLY:>` 里），
// Qt-free 的编译面 `kritaimpex_noqt_compile_interface` 今天已被一个非 GUI 程序
// 用着并跑绿：`sdk/smoke/paint_smoke`（S-14 VERIFIED）实测
//   构建 exit=0 · 运行 `RESULT=done failures=0` ·
//   强判据 `nm -u -C paint_smoke | grep -E '\bQ[A-Z][A-Za-z0-9_]*\b'` = **0 命中 / 分母 555**。
// 同一份 include 清单（`<KisDocument.h>` `<KisDocumentRegistry.h>` `<kis_image.h>`
// `<KoColorSpaceRegistry.h>`）正是本文件要的那一族。原始输出见
// `WT/.superpowers/sdd/R-82/evidence/paint-smoke-run.log` 与
// `…/evidence/qt-probe-qfileinfo-qdir-qbuffer.txt` 一族。
//
// 端口化所需的垫片（都在 sdk/tests/，逐条有真 Qt 5.15.7 探针原始输出）：
//   `QDir::entryInfoList()` · `QFileInfo::{fileName,isDir,isHidden}()` ·
//   `QBuffer` · `QStandardPaths`/`QFile`/`QFileDevice`（R-77 交付）·
//   `TestUtil::{pkStringFromQString,diagnosticQImage}`（R-82 补的 pk 分支）
// **断言、容差、跳过条件一律未改**——只换了类型面。
// ===========================================================================
void testFiles(const QString& _dirname, const QStringList& exclusions, const QString &resultSuffix = QString(), int fuzzy = 0, int maxNumFailingPixels = 0, bool showDebug = true)
{
    QDir dirSources(_dirname);
    QStringList failuresFileInfo;
    QStringList failuresDocImage;
    QStringList failuresCompare;

    Q_FOREACH (QFileInfo sourceFileInfo, dirSources.entryInfoList()) {
        qDebug() << sourceFileInfo.fileName();
        if (exclusions.indexOf(sourceFileInfo.fileName()) > -1) {
            continue;
        }
        if (!sourceFileInfo.isHidden() && !sourceFileInfo.isDir()) {
            QFileInfo resultFileInfo(QString(FILES_DATA_DIR) + "/results/" + sourceFileInfo.fileName() + resultSuffix + ".png");

            if (!resultFileInfo.exists()) {
                failuresFileInfo << resultFileInfo.fileName();
                continue;
            }

            KisDocument *doc = KisDocumentRegistry::instance()->createDocument();

            KisImportExportManager manager(doc);
            doc->setFileBatchMode(true);

            KisImportExportErrorCode status = manager.importDocument(
                pkStringFromQString(sourceFileInfo.absoluteFilePath()), PkString());
            Q_UNUSED(status);

            if (!doc->image()) {
                failuresDocImage << sourceFileInfo.fileName();
                continue;
            }

            const PkString id = doc->image()->colorSpace()->id();
            if (id != "GRAYA" && id != "GRAYAU16" && id != "RGBA" && id != "RGBA16") {
                dbgKrita << "Images need conversion";
                doc->image()->convertImageColorSpace(KoColorSpaceRegistry::instance()->rgb8(),
                                                    KoColorConversionTransformation::IntentAbsoluteColorimetric,
                                                    KoColorConversionTransformation::NoOptimization);
                doc->image()->waitForDone();
            }

            // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
            // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
            // 保险动作；S-06 按模式删除。
            doc->image()->waitForDone();
            QImage sourceImage = diagnosticQImage(
                doc->image()->projection()->convertToQImage(0, doc->image()->bounds()));



            QImage resultImage(resultFileInfo.absoluteFilePath());
            resultImage.convertTo(QImage::Format_ARGB32);
            sourceImage.convertTo(QImage::Format_ARGB32);

            QPoint pt;

            if (!TestUtil::compareQImages(pt, resultImage, sourceImage, fuzzy, fuzzy, maxNumFailingPixels, showDebug)) {
                // R-82：去掉原式末尾的 `.toLatin1()`。真 Qt 5.15.7 探针实测
                // （evidence/qt-probe-qfileinfo-qdir-qbuffer.txt 的 probe3 段）：
                //   `QString + QByteArray` 走的正是 latin1 隐式转换，去掉 `.toLatin1()`
                //   后拼出的串与保留它**逐字节相同**（探针 `s == t` → 1，且两者打印一致）。
                // pk 栈上 `PkString` 没有 `operator+(const PkByteArray&)`（编不过），
                // 去掉后两栈**同源**、行为不变。
                failuresCompare << sourceFileInfo.fileName() + ": " + QString("Pixel (%1,%2) has different values").arg(pt.x()).arg(pt.y());
                sourceImage.save(sourceFileInfo.fileName() + ".png");
                resultImage.save(resultFileInfo.fileName() + ".expected.png");
                continue;
            }

            delete doc;
        }
    }
    if (failuresCompare.isEmpty() && failuresDocImage.isEmpty() && failuresFileInfo.isEmpty()) {
        return;
    }
    qWarning() << "Comparison failures: " << failuresCompare;
    qWarning() << "No image failures: " << failuresDocImage;
    qWarning() << "No comparison image: " <<  failuresFileInfo;

    FILESTEST_FAIL("Failed testing files");
}


// ---------------------------------------------------------------------------
// R-77：下面三个函数**两栈同源**（pk 栈也编译）。函数体自 R-77 起只依赖
// sdk/tests/compat/ 的垫片面；**断言、容差、行为一律未改**——它们本来就是纯文件
// 权限操作，R-77 之前之所以编不了，只是因为这些垫片当时还不存在。
// 真 Qt 语义逐条探针实测（task1-report.md §3.2），改动前先看那份原始输出。
// ---------------------------------------------------------------------------
void prepareFile(QFileInfo sourceFileInfo, bool removePermissionToWrite, bool removePermissionToRead)
{

    QFileDevice::Permissions permissionsBefore;
    if (sourceFileInfo.exists()) {
        permissionsBefore = QFile::permissions(sourceFileInfo.absoluteFilePath());
        qDebug() << "prepareFile permissions before:" << permissionsBefore;
    } else {
        QFile file(sourceFileInfo.absoluteFilePath());
        bool opened = file.open(QIODevice::ReadWrite);
        if (!opened) {
            qDebug() << "The file cannot be opened/created: " << file.error() << file.errorString();
        }
        permissionsBefore = file.permissions();
        file.close();
    }
    QFileDevice::Permissions permissionsNow = permissionsBefore;
    if (removePermissionToRead) {
        permissionsNow = permissionsBefore &
                (~QFileDevice::ReadUser & ~QFileDevice::ReadOwner
                 & ~QFileDevice::ReadGroup & ~QFileDevice::ReadOther);
    }
    if (removePermissionToWrite) {
        permissionsNow = permissionsBefore &
                (~QFileDevice::WriteUser & ~QFileDevice::WriteOwner
                 & ~QFileDevice::WriteGroup & ~QFileDevice::WriteOther);
    }

    bool success = QFile::setPermissions(sourceFileInfo.absoluteFilePath(), permissionsNow);
    if (!success) {
        qWarning() << "prepareFile(): Failed to set permission of file" << sourceFileInfo.absoluteFilePath()
                   << "from" << permissionsBefore << "to" << permissionsNow;
    }
}

void restorePermissionsToReadAndWrite(QFileInfo sourceFileInfo)
{
    QFileDevice::Permissions permissionsNow = sourceFileInfo.permissions();
    QFileDevice::Permissions permissionsAfter = permissionsNow
            | (QFileDevice::ReadUser | QFileDevice::ReadOwner
            | QFileDevice::ReadGroup | QFileDevice::ReadOther)
            | (QFileDevice::WriteUser | QFileDevice::WriteOwner
            | QFileDevice::WriteGroup | QFileDevice::WriteOther);
    bool success = QFile::setPermissions(sourceFileInfo.absoluteFilePath(), permissionsAfter);
    if (!success) {
        qWarning() << "restorePermissionsToReadAndWrite(): Failed to set permission of file" << sourceFileInfo.absoluteFilePath()
                   << "from" << permissionsNow << "to" << permissionsAfter;
    }
}

const QString &impexTempFilesDir() {
    static const QString s_path = []() {
        const QString path = QDir::cleanPath(
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/impex_test") + '/';
        QDir(path).mkpath(QStringLiteral("."));
        return path;
    }();
    return s_path;
}


// ---------------------------------------------------------------------------
// R-77：从此处到 namespace 收尾的四个 testXxx() 全部依赖 KisDocument /
// KisImportExportManager / KoColorSpace / KoColorSpaceRegistry（壳闭包外），
// **整段留 Qt 栈**。段内断言、容差、`#ifdef Q_OS_WIN` 分支一律未动。
// ---------------------------------------------------------------------------
void testImportFromWriteonly(const ImpexTestString &mimetype)
{
#ifdef Q_OS_WIN
    /// on Windows one cannot create a write-only file, so just skip this test
    /// (but keep it compiled to avoid compilation issues)
    FILESTEST_SKIP("Cannot test write-only file on Windows.");
#endif

#ifdef Q_OS_UNIX
    if (geteuid() == 0) {
        FILESTEST_SKIP("Test is being run as root; removing read permission has no effect.");
    }
#endif

    QString writeonlyFilename = impexTempFilesDir() + "writeonlyFile.txt";
    QFileInfo sourceFileInfo(writeonlyFilename);

    prepareFile(sourceFileInfo, false, true);

    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();

    KisImportExportManager manager(doc);
    doc->setFileBatchMode(true);

    KisImportExportErrorCode status = manager.importDocument(
        pkStringFromQString(sourceFileInfo.absoluteFilePath()), impexApiString(mimetype));
    qDebug() << "import result = " << diagnosticQString(status.errorMessage());

    QString failMessage = "";
    bool fail = false;

    if (status == ImportExportCodes::FileFormatIncorrect) {
        qDebug() << "Make sure you set the correct mimetype in the test case.";
        failMessage = "Incorrect status.";
        fail = true;
    }

    // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
    // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
    // 保险动作；S-06 按模式删除。

    if (doc->image()) {
        doc->image()->waitForDone();
    }

    delete doc;

    if (fail || status.isOk()) {
        qDebug() << "The file permission is:" << QFile::permissions(sourceFileInfo.absoluteFilePath());
    }

    restorePermissionsToReadAndWrite(sourceFileInfo);

    FILESTEST_VERIFY(!status.isOk());
    if (fail) {
        FILESTEST_FAIL(failMessage.toUtf8().constData());
    }

}


void testExportToReadonly(const ImpexTestString &mimetype)
{
#ifdef Q_OS_UNIX
    if (geteuid() == 0) {
        FILESTEST_SKIP("Test is being run as root; removing write permission has no effect.");
    }
#endif

    QString readonlyFilename = impexTempFilesDir() + "readonlyFile.txt";

    QFileInfo sourceFileInfo(readonlyFilename);
    prepareFile(sourceFileInfo, true, false);

    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();

    KisImportExportManager manager(doc);
    doc->setFileBatchMode(true);

    KisImportExportErrorCode status = ImportExportCodes::OK;
    QString failMessage = "";
    bool fail = false;

    {
    MaskParent p;
    qDebug() << "testExportToReadonly image:"
             << static_cast<const void *>(doc->image().data());

    doc->setCurrentImage(p.image);

    bool result = doc->exportDocumentSync(
        pkStringFromQString(sourceFileInfo.absoluteFilePath()), impexApiString(mimetype).toUtf8());
    status = result ? ImportExportCodes::OK : ImportExportCodes::Failure;

    qDebug() << "export result = " << diagnosticQString(status.errorMessage());

    // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
    // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
    // 保险动作；S-06 按模式删除。

    if (doc->image()) {
        doc->image()->waitForDone();
    }

    }
    delete doc;

    if (status.isOk()) {
        qDebug() << "The file permission is:" << QFile::permissions(sourceFileInfo.absoluteFilePath());
    }

    restorePermissionsToReadAndWrite(sourceFileInfo);

    FILESTEST_VERIFY(!status.isOk());
    if (fail) {
        FILESTEST_FAIL(failMessage.toUtf8().constData());
    }
}



void testImportIncorrectFormat(const ImpexTestString &mimetype)
{
    QString incorrectFormatFilename = impexTempFilesDir() + "incorrectFormatFile.txt";
    QFileInfo sourceFileInfo(incorrectFormatFilename);

    prepareFile(sourceFileInfo, false, false);

    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();

    KisImportExportManager manager(doc);
    doc->setFileBatchMode(true);

    KisImportExportErrorCode status = manager.importDocument(
        pkStringFromQString(sourceFileInfo.absoluteFilePath()), impexApiString(mimetype));
    qDebug() << "import result = " << diagnosticQString(status.errorMessage());

    // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
    // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
    // 保险动作；S-06 按模式删除。

    if (doc->image()) {
        doc->image()->waitForDone();
    }

    delete doc;

    FILESTEST_VERIFY(!status.isOk());
    FILESTEST_VERIFY(status == KisImportExportErrorCode(ImportExportCodes::FileFormatIncorrect)
            || status == KisImportExportErrorCode(ImportExportCodes::ErrorWhileReading)); // in case the filter doesn't know if it can't read or just parse

}


void testExportToColorSpace(const ImpexTestString &mimetype, const KoColorSpace* space, KisImportExportErrorCode expected)
{
    QString colorspaceFilename = impexTempFilesDir() + "colorspace.txt";

    QFileInfo sourceFileInfo(colorspaceFilename);
    prepareFile(sourceFileInfo, true, true);
    restorePermissionsToReadAndWrite(sourceFileInfo);

    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();

    KisImportExportManager manager(doc);
    doc->setFileBatchMode(true);

    KisImportExportErrorCode statusExport = ImportExportCodes::OK;
    KisImportExportErrorCode statusImport = ImportExportCodes::OK;

    QString failMessage = "";
    bool fail = false;

    {
    MaskParent p;

    doc->setCurrentImage(p.image);
    doc->image()->convertImageColorSpace(space, KoColorConversionTransformation::Intent::IntentPerceptual, KoColorConversionTransformation::ConversionFlag::Empty);
    doc->image()->waitForDone();

    bool result = doc->exportDocumentSync(
        pkStringFromQString(colorspaceFilename), impexApiString(mimetype).toUtf8());
    statusExport = result ? ImportExportCodes::OK : ImportExportCodes::Failure;

    statusImport = manager.importDocument(
        pkStringFromQString(colorspaceFilename), impexApiString(mimetype));
    if (!(statusImport == ImportExportCodes::OK)) {
        fail = true;
        failMessage = "Incorrect status";
    }

    bool mismatch = (*(doc->image()->colorSpace()) != *space) || (doc->image()->colorSpace()->profile() != space->profile());
    if (mismatch) {
        qDebug() << "Document color space = "
                 << diagnosticQString((doc->image()->colorSpace())->id());
        qDebug() << "Saved color space = " << diagnosticQString(space->id());
        fail = true;
        failMessage = "Mismatch of color spaces";
    }

    // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
    // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
    // 保险动作；S-06 按模式删除。

    if (doc->image()) {
        doc->image()->waitForDone();
    }

    }
    delete doc;

    QFile::remove(colorspaceFilename);

    if (fail) {
        FILESTEST_FAIL(failMessage.toUtf8().constData());
    }

    FILESTEST_VERIFY(statusExport.isOk());
    FILESTEST_VERIFY(statusExport == expected);
}

#undef FILESTEST_VERIFY
#undef FILESTEST_FAIL
#undef FILESTEST_SKIP

}
#endif
