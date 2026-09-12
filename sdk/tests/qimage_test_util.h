/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] qimage_test_util.h 阻塞登记（S-06 Task 9）
//   —— R-75 关闭（2026-09-12）
//
// **已关闭。关闭依据**：R-15（R-75 Task 1）交付了 PkImage 的文件 I/O
// （按路径构造 / load / save）＋ invertPixels，本文件需要的 `QImage ref(fullPath)`
// （文件加载）与 `image.save(...)` / `ref.save(...)`（文件写出）在 pk 栈上都有
// 等价物。原来那三处 `#ifndef KRITA_TESTSDK_PK_NATIVE` 守卫按「关闭条件：R-15
// 交付 PkImage 文件 I/O 后端口化」的登记意图被**打开**：checkQImageImpl /
// checkQImage / checkQImagePremultiplied / checkQImageExternal 四族现在在 pk 栈上
// 真参与编译。
//
// **打开守卫不改任何行为**：只删掉 `#ifndef`/`#endif` 这对预处理指令本身，
// 四族的代码体（含五族默认容差：checkQImage 族 fuzzy=0、fuzzyAlpha=-1→0、
// maxNumFailingPixels=0；compareQImages 族 0,0,0）**一个 token 未动**。
// 真 Qt 测试栈（kritatestsdk）不定义 KRITA_TESTSDK_PK_NATIVE，删掉 `#ifndef`
// 之后这段代码在真 Qt 栈上照旧被编译，行为一个字不变。
// compareChannels / compareChannelsPremultiplied / compareQImagesImpl /
// compareQImages / compareQImagesPremultiplied 本来就两栈共用，零改动。


#ifndef QIMAGE_TEST_UTIL_H
#define QIMAGE_TEST_UTIL_H

#ifdef FILES_OUTPUT_DIR

#include <QProcessEnvironment>
#include <QDir>
#include <QApplication>

namespace TestUtil {

inline QString fetchExternalDataFileName(const QString relativeFileName)
{
    static QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    static QString unittestsDataDirPath = "KRITA_UNITTESTS_DATA_DIR";

    QString path;
    if (!env.contains(unittestsDataDirPath)) {
        warnKrita << "Environment variable" << unittestsDataDirPath << "is not set";
        return QString();
    } else {
        path = env.value(unittestsDataDirPath, "");
    }

    QString filename  =
        path +
        '/' +
        relativeFileName;

    return filename;
}

inline QString fetchDataFileLazy(const QString relativeFileName, bool externalTest = false)
{
    if (externalTest) {
        return fetchExternalDataFileName(relativeFileName);
    } else {
        QString filename  =
            QString(FILES_DATA_DIR) +
            '/' +
            relativeFileName;

        if (QFileInfo(filename).exists()) {
            return filename;
        }

        filename  =
            QString(FILES_DEFAULT_DATA_DIR) +
            '/' +
            relativeFileName;

        if (QFileInfo(filename).exists()) {
            return filename;
        }

        filename  =
            QFileInfo(qApp->applicationFilePath()).absolutePath() +
            "/" +
            relativeFileName;

        if (QFileInfo(filename).exists()) {
            return filename;
        }

        filename  =
            QFileInfo(qApp->applicationFilePath()).absolutePath() +
            "/data/" +
            relativeFileName;

        if (QFileInfo(filename).exists()) {
            return filename;
        }
    }

    return QString();
}

// quint8 arguments are automatically converted into int
inline bool compareChannels(int ch1, int ch2, int fuzzy)
{
    return qAbs(ch1 - ch2) <= fuzzy;
}

inline bool compareChannelsPremultiplied(int ch1, int alpha1, int ch2, int alpha2, int fuzzy, int fuzzyAlpha)
{
    return qAbs(ch1 * alpha1 - ch2 * alpha2) / 255 <= fuzzy * qMax(1, fuzzyAlpha);
}


inline bool compareQImagesImpl(QPoint & pt, const QImage & image1, const QImage & image2, int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0, bool showDebug = true, bool premultipliedMode = false)
{
    //     QTime t;
    //     t.start();

    const int w1 = image1.width();
    const int h1 = image1.height();
    const int w2 = image2.width();
    const int h2 = image2.height();
    const int bytesPerLine = image1.bytesPerLine();

    if (w1 != w2 || h1 != h2) {
        pt.setX(-1);
        pt.setY(-1);
        qDebug() << "Images have different sizes" << image1.size() << image2.size();
        return false;
    }

    int numFailingPixels = 0;

    for (int y = 0; y < h1; ++y) {
        const QRgb * const firstLine = reinterpret_cast<const QRgb *>(image2.scanLine(y));
        const QRgb * const secondLine = reinterpret_cast<const QRgb *>(image1.scanLine(y));

        if (memcmp(firstLine, secondLine, bytesPerLine) != 0) {
            for (int x = 0; x < w1; ++x) {
                const QRgb a = firstLine[x];
                const QRgb b = secondLine[x];

                bool same = false;

                if (!premultipliedMode) {
                    same =
                            compareChannels(qRed(a), qRed(b), fuzzy) &&
                            compareChannels(qGreen(a), qGreen(b), fuzzy) &&
                            compareChannels(qBlue(a), qBlue(b), fuzzy);
                } else {
                    same =
                            compareChannelsPremultiplied(qRed(a), qAlpha(a), qRed(b), qAlpha(b), fuzzy, fuzzyAlpha) &&
                            compareChannelsPremultiplied(qGreen(a), qAlpha(a), qGreen(b), qAlpha(b), fuzzy, fuzzyAlpha) &&
                            compareChannelsPremultiplied(qBlue(a), qAlpha(a), qBlue(b), qAlpha(b), fuzzy, fuzzyAlpha);
                }
                const bool sameAlpha = compareChannels(qAlpha(a), qAlpha(b), fuzzyAlpha);
                const bool bothTransparent = sameAlpha && qAlpha(a)==0;

                if (!bothTransparent && (!same || !sameAlpha)) {
                    pt.setX(x);
                    pt.setY(y);
                    numFailingPixels++;

                    if (showDebug) {
                        qDebug() << " Different at" << pt
                                 << "source" << qRed(a) << qGreen(a) << qBlue(a) << qAlpha(a)
                                 << "dest" << qRed(b) << qGreen(b) << qBlue(b) << qAlpha(b)
                                 << "fuzzy" << fuzzy
                                 << "fuzzyAlpha" << fuzzyAlpha
                                 << "(" << numFailingPixels << "of" << maxNumFailingPixels << "allowed )";
                    }

                    if (numFailingPixels > maxNumFailingPixels) {
                        return false;
                    }
                }
            }
        }
    }
    //     qDebug() << "compareQImages time elapsed:" << t.elapsed();
    //    qDebug() << "Images are identical";
    return true;
}

inline bool compareQImages(QPoint & pt, const QImage & image1, const QImage & image2, int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0, bool showDebug = true)
{
    return compareQImagesImpl(pt, image1, image2, fuzzy, fuzzyAlpha, maxNumFailingPixels, showDebug, false);
}

inline bool compareQImagesPremultiplied(QPoint & pt, const QImage & image1, const QImage & image2, int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0, bool showDebug = true)
{
    return compareQImagesImpl(pt, image1, image2, fuzzy, fuzzyAlpha, maxNumFailingPixels, showDebug, true);
}

// ---------------------------------------------------------------------------
// R-65 Task 3 · L2 守卫（brief §1.6 L2）
//
// 这一族（checkQImageImpl / checkQImage / checkQImagePremultiplied /
// checkQImageExternal）**从磁盘读参考图**：`QImage ref(fullPath)` 是文件加载，
// 失败时还有 `image.save(...)` / `ref.save(...)` 文件写出。pk 栈的 `QImage`
// 是 `PkImage`（`pk/image/compat/QImage` 的 `#define QImage PkImage`），而
// **PkImage 没有文件 I/O**（无按路径构造、无 save）——那是 R-15 明确排除、
// 归 impex S 任务的。
//
// 因此**只守卫这一族（真的需要文件 I/O 的路径）**：pk 栈上它不参与编译，
// 需要它的 target 编不过 ⇒ 登记为被挡（本目录 6 个：kis_gradient_painter_test
// 用 checkQImageExternal，kis_async_merger_test / kis_group_layer_test /
// kis_marker_painter_test / kis_scanline_fill_test / kis_onion_skin_compositor_test
// 用 checkQImage / ReferenceImageChecker）。
//
// **没有把整个比对族降级成恒真**：上面 compareChannels /
// compareChannelsPremultiplied / compareQImagesImpl / compareQImages /
// compareQImagesPremultiplied **一个字不动**——它们是**内存内**比对（两个
// 已构造的 image 逐行 memcmp + 通道容差），pk 栈上照常工作，语义与真 Qt 逐字相同。
// 真 Qt 测试栈（kritatestsdk）不定义 KRITA_TESTSDK_PK_NATIVE，本文件行为一个字不变。
//
// R-75：守卫已按上面的关闭条件**打开**（R-15 / R-75 Task 1 交付了 PkImage 文件
// I/O）。原 `#ifndef KRITA_TESTSDK_PK_NATIVE` / `#endif` 只被删掉，四族代码体
// （含默认容差）零 diff；真 Qt 栈上这段代码本来就在 `#ifndef` 的「真」分支里，
// 删掉守卫后照旧编译、行为一个字不变。
inline bool checkQImageImpl(bool externalTest,
                            const QImage &srcImage, const QString &testName,
                            const QString &prefix, const QString &name,
                            int fuzzy, int fuzzyAlpha, int maxNumFailingPixels, bool premultipliedMode)
{
    QImage image = srcImage.convertToFormat(QImage::Format_ARGB32);

    if (fuzzyAlpha == -1) {
        fuzzyAlpha = fuzzy;
    }

    QString filename(prefix + "_" + name + ".png");
    QString dumpName(prefix + "_" + name + "_expected.png");

    const QString standardPath =
        testName + '/' +
        prefix + '/' + filename;

    QString fullPath = fetchDataFileLazy(standardPath, externalTest);

    if (fullPath.isEmpty() || !QFileInfo(fullPath).exists()) {
        // Try without the testname subdirectory
        fullPath = fetchDataFileLazy(prefix + '/' +
                                     filename,
                                     externalTest);
    }

    if (fullPath.isEmpty() || !QFileInfo(fullPath).exists()) {
        // Try without the prefix subdirectory
        fullPath = fetchDataFileLazy(testName + '/' +
                                     filename,
                                     externalTest);
    }

    if (!QFileInfo(fullPath).exists()) {
        fullPath = "";
    }

    bool canSkipExternalTest = fullPath.isEmpty() && externalTest;
    QImage ref(fullPath);

    bool valid = true;
    QPoint t;
    if(!compareQImagesImpl(t, image, ref, fuzzy, fuzzyAlpha, maxNumFailingPixels, true, premultipliedMode)) {
        bool saveStandardResults = true;

        if (canSkipExternalTest) {
            static QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            static QString writeUnittestsVar = "KRITA_WRITE_UNITTESTS";

            int writeUnittests = env.value(writeUnittestsVar, "0").toInt();
            if (writeUnittests) {
                QString path = fetchExternalDataFileName(standardPath);

                QFileInfo pathInfo(path);
                QDir directory;
                directory.mkpath(pathInfo.path());

                qDebug() << "--- Saving reference image:" << name << path;
                image.save(path);
                saveStandardResults = false;

            } else {
                qDebug() << "--- External image not found. Skipping..." << name;
            }
        } else {
            qDebug() << "--- Wrong image:" << name;
            valid = false;
        }

        if (saveStandardResults) {
            image.save(QString(FILES_OUTPUT_DIR) + '/' + filename);
            ref.save(QString(FILES_OUTPUT_DIR) + '/' + dumpName);
        }
    }

    return valid;
}

inline bool checkQImage(const QImage &image, const QString &testName,
                        const QString &prefix, const QString &name,
                        int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImageImpl(false, image, testName,
                           prefix, name,
                           fuzzy, fuzzyAlpha, maxNumFailingPixels, false);
}

inline bool checkQImagePremultiplied(const QImage &image, const QString &testName,
                                     const QString &prefix, const QString &name,
                                     int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImageImpl(false, image, testName,
                           prefix, name,
                           fuzzy, fuzzyAlpha, maxNumFailingPixels, true);
}


inline bool checkQImageExternal(const QImage &image, const QString &testName,
                                const QString &prefix, const QString &name,
                                int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImageImpl(true, image, testName,
                           prefix, name,
                           fuzzy, fuzzyAlpha, maxNumFailingPixels, false);
}

}

#endif // FILES_OUTPUT_DIR

#endif // QIMAGE_TEST_UTIL_H

