// R-82 · sdk/tests/impex_file_utils.h
// ---------------------------------------------------------------------------
// 从 `sdk/tests/filestest.h` **原样搬出来**的三个函数（`TestUtil::prepareFile` /
// `restorePermissionsToReadAndWrite` / `impexTempFilesDir`），**一字未改**。
//
// 为什么要单独成一个头（R-82 实测的硬约束）：
//   `filestest.h` 现在会拉 `testutil.h`（`pkStringFromQString` / `diagnosticQImage` /
//   `MaskParent` / `compareQImages` 都住在那里），而 `testutil.h` 无条件 include
//   `<KoResource.h>` `<KoColorSpace.h>` `<kis_paint_device.h>` 等一堆 Krita 头
//   ——**零 Krita 库依赖的 `PkTestSupportSelfTest` 拿不到那些 include 目录**
//   （实测：`ninja PkTestSupportSelfTest` → `testutil.h:63: 'KoResource.h' file not found`）。
//   而 `PkTestSupportSelfTest` 需要 filestest.h 的真正理由只有一条：
//   它要**直接调**这三个函数（R-77 brief §3.4 的收尾证据，见该文件 :59-130）。
//   把这三个函数搬到一个只依赖 sdk/tests/compat 垫片面的小头里，
//   那条证据就能继续成立，而 `PkTestSupportSelfTest` 不必去够 `testutil.h`。
//
// 本头的 include 面 = **只**要 compat 垫片（QDir/QFile/QFileDevice/QFileInfo/
// QIODevice/QStandardPaths/QString/QDebug），**不要任何 Krita 头**——
// 这是它存在的全部意义，改它之前先想清楚这一点。
//
// 三个函数的两栈可观察行为由真 Qt 5.15.7 探针钉死（`sdk/tests/compat/` 那几个头的
// 头注里有逐条原始输出）。**断言、容差、行为一个字都没动。**
// ---------------------------------------------------------------------------
#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QStandardPaths>
#include <QString>

namespace TestUtil
{

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

} // namespace TestUtil
