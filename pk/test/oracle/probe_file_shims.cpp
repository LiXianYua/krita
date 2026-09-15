// probe_file_shims.cpp —— R-77 Task 1 的真 Qt 语义探针（只读，不改任何东西，
// 除了自己在 temp 下造的探针文件）。
//
// 「Qt 的行为一律去问真 Qt，不许推断」。本探针把 sdk/tests/compat/ 那 5 个垫片
// 必须覆盖的每一个成员的真实语义，逐条在这里实测并原样打印。
//
// 构建/运行：见同目录 run_oracle.sh（macOS：真 Qt 5.15.7 在 CI 前缀里，
// 形如 $PREFIX/lib/QtCore.framework）。

#include <QtCore/QFile>
#include <QtCore/QFileDevice>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QLatin1String>
#include <QtCore/QCoreApplication>
#include <cstdio>
#include <cstdlib>

static void hdr(const char *s) { std::printf("\n===== %s =====\n", s); }

// QFileDevice::Permissions 用 int() 打，方便跟垫片常量逐位对照。
static int I(QFileDevice::Permissions p) { return static_cast<int>(p); }

static int g_argc = 1;
static char g_argv0[] = "probe_file_shims";
static char *g_argv[] = { g_argv0, nullptr };

int main()
{
    std::printf("Qt runtime version (qVersion()) = %s\n", qVersion());
    std::printf("QIODevice::NotOpen=%d ReadOnly=%d WriteOnly=%d ReadWrite=%d Append=%d\n",
                (int)QIODevice::NotOpen, (int)QIODevice::ReadOnly, (int)QIODevice::WriteOnly,
                (int)QIODevice::ReadWrite, (int)QIODevice::Append);

    // ---------------------------------------------------------------- 0. 常量值
    hdr("0. QFileDevice::Permissions 常量真值（垫片必须逐位相同）");
    std::printf("ReadOwner=%d WriteOwner=%d ExeOwner=%d\n", I(QFileDevice::ReadOwner), I(QFileDevice::WriteOwner), I(QFileDevice::ExeOwner));
    std::printf("ReadUser =%d WriteUser =%d ExeUser =%d\n", I(QFileDevice::ReadUser), I(QFileDevice::WriteUser), I(QFileDevice::ExeUser));
    std::printf("ReadGroup=%d WriteGroup=%d ExeGroup=%d\n", I(QFileDevice::ReadGroup), I(QFileDevice::WriteGroup), I(QFileDevice::ExeGroup));
    std::printf("ReadOther=%d WriteOther=%d ExeOther=%d\n", I(QFileDevice::ReadOther), I(QFileDevice::WriteOther), I(QFileDevice::ExeOther));
    {
        QFileDevice::Permissions all = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser;
        std::printf("(ReadOwner|WriteOwner|ReadUser|WriteUser) = %d  0o%o\n", I(all), I(all));
        std::printf("(int)(~QFileDevice::ReadOwner) [裸枚举取反] = %d\n", (int)(~QFileDevice::ReadOwner));
        std::printf("(int)(~all) [QFlags::operator~, 不掩码]     = %d  0x%x\n", (int)(~all), (unsigned)(int)(~all));
        std::printf("(all & QFileDevice::ReadOwner) = %d  (all | ReadGroup) = %d\n",
                    I(all & QFileDevice::ReadOwner), I(all | QFileDevice::ReadGroup));
    }

    // ---------------------------------------------------------------- 1. QDir 连带
    hdr("1. QDir::tempPath() / QDir::cleanPath()");
    std::printf("QDir::tempPath() = '%s'\n", QDir::tempPath().toUtf8().constData());
    std::printf("QDir::cleanPath(\"/a/b/../c//d/\") = '%s'\n", QDir::cleanPath(QStringLiteral("/a/b/../c//d/")).toUtf8().constData());
    std::printf("QDir::cleanPath(\"/a/../../b\") = '%s'\n", QDir::cleanPath(QStringLiteral("/a/../../b")).toUtf8().constData());
    std::printf("QDir(\"/x/y\").mkpath(\".\") = %d\n", (int)QDir(QStringLiteral("/tmp/pk_probe_mkpath/x/y")).mkpath(QStringLiteral(".")));
    std::printf("QDir::mkpath(\"/tmp/pk_probe_mkpath/x/y\") 后 QDir::exists = %d\n", (int)QDir(QStringLiteral("/tmp/pk_probe_mkpath/x/y")).exists());
    QDir(QStringLiteral("/tmp/pk_probe_mkpath")).removeRecursively();

    // ---------------------------------------------------------------- 2. QStandardPaths
    hdr("2. QStandardPaths::writableLocation(CacheLocation)");
    std::printf("CacheLocation 枚举值 = %d\n", (int)QStandardPaths::CacheLocation);
    std::printf("setTestModeEnabled(false) -> writableLocation = '%s'\n",
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().constData());
    QStandardPaths::setTestModeEnabled(true);
    std::printf("setTestModeEnabled(true)  -> writableLocation = '%s'\n",
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().constData());
    QStandardPaths::setTestModeEnabled(false);
    std::printf("再 setTestModeEnabled(false) -> writableLocation = '%s'\n",
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().constData());
    std::printf("program name / app name: qApp=%p\n", (void*)QCoreApplication::instance());

    // ---------------------------------------------------------------- 2b. 有 QCoreApplication 时
    // 真 Qt 的 CacheLocation 是「应用专属」目录；有 QCoreApplication 时 Qt 会追加
    // <org>/<app>。pk 测试栈没有 QCoreApplication（SIMPLE_MAIN_IMPL 不建），
    // 所以消费点看到的是上面那个**裸** cache 根。
    {
        QCoreApplication app(g_argc, g_argv);
        QCoreApplication::setOrganizationName("KritaOrg");
        QCoreApplication::setApplicationName("krita");
        std::printf("QCoreApplication 存在后 qApp=%p org='KritaOrg' app='krita'\n", (void*)QCoreApplication::instance());
        std::printf("  有 app 时 writableLocation(CacheLocation) = '%s'\n",
                    QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().constData());
    }

    // ---------------------------------------------------------------- 3. QFile::permissions(path) 不存在路径
    hdr("3. QFile::permissions() 对不存在的路径");
    {
        const QString nope = QStringLiteral("/tmp/pk_probe_does_not_exist_12345");
        std::printf("QFile(%s) 存在? %d\n", nope.toUtf8().constData(), (int)QFile::exists(nope));
        std::printf("static QFile::permissions(不存在) = %d\n", I(QFile::permissions(nope)));
        QFile f(nope);
        std::printf("非 static f.permissions() (未 open, 不存在) = %d\n", I(f.permissions()));
        std::printf("f.error() = %d   f.errorString() = '%s'\n", (int)f.error(), f.errorString().toUtf8().constData());
    }

    // ---------------------------------------------------------------- 4. QFile::setPermissions 语义
    hdr("4. QFile::setPermissions(path, perms) 返回值语义");
    {
        const QString p = QStringLiteral("/tmp/pk_probe_perm.txt");
        { QFile f(p); f.open(QIODevice::WriteOnly); f.write("hello", 5); f.close(); }
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        std::printf("设置成 0600 后 QFile::permissions = %d 0o%o\n", I(QFile::permissions(p)), I(QFile::permissions(p)));
        bool ok = QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
        std::printf("对存在文件 setPermissions 返回 = %d (1=成功)；之后 perms = %d 0o%o\n", (int)ok, I(QFile::permissions(p)), I(QFile::permissions(p)));
        bool bad = QFile::setPermissions(QStringLiteral("/tmp/pk_probe_no_such_file_98765"), QFileDevice::ReadOwner);
        std::printf("对不存在路径 setPermissions 返回 = %d (0=失败)\n", (int)bad);
        QFile::remove(p);
    }

    // ---------------------------------------------------------------- 5. QFile::remove 不存在路径
    hdr("5. QFile::remove(path) 对不存在路径");
    {
        std::printf("remove(不存在) 返回 = %d\n", (int)QFile::remove(QStringLiteral("/tmp/pk_probe_no_such_file_98765")));
        const QString p = QStringLiteral("/tmp/pk_probe_rm.txt");
        { QFile f(p); f.open(QIODevice::WriteOnly); f.close(); }
        std::printf("先造一个文件, exists=%d\n", (int)QFile::exists(p));
        std::printf("remove(存在) 返回 = %d\n", (int)QFile::remove(p));
        std::printf("remove 后 exists=%d\n", (int)QFile::exists(p));
    }

    // ---------------------------------------------------------------- 6. open(ReadWrite) 对不存在文件
    hdr("6. QFile(path).open(QIODevice::ReadWrite) 对不存在的文件");
    {
        const QString p = QStringLiteral("/tmp/pk_probe_rw_notexist.txt");
        QFile::remove(p);
        QFile f(p);
        bool ok = f.open(QIODevice::ReadWrite);
        std::printf("open(ReadWrite) 返回 = %d (1=成功 => 创建了)\n", (int)ok);
        std::printf("open 后 exists=%d  isOpen=%d  error()=%d  errorString()='%s'\n",
                    (int)QFile::exists(p), (int)f.isOpen(), (int)f.error(), f.errorString().toUtf8().constData());
        f.close();
        QFile::remove(p);

        const QString p2 = QStringLiteral("/tmp/pk_probe_ro_notexist.txt");
        QFile::remove(p2);
        QFile f2(p2);
        bool ok2 = f2.open(QIODevice::ReadOnly);
        std::printf("[对照] open(ReadOnly) 对不存在文件返回 = %d, error()=%d, errorString()='%s'\n",
                    (int)ok2, (int)f2.error(), f2.errorString().toUtf8().constData());
        f2.close();
        QFile::remove(p2);

        // 目录不可写时的失败形状（用一个只读目录）
        std::printf("QFileDevice::OpenError=%d ReadError=%d WriteError=%d FatalError=%d ResourceError=%d\n",
                    (int)QFileDevice::OpenError, (int)QFileDevice::ReadError, (int)QFileDevice::WriteError,
                    (int)QFileDevice::FatalError, (int)QFileDevice::ResourceError);
        std::printf("NoError=%d  PermissionsError=%d\n", (int)QFileDevice::NoError, (int)QFileDevice::PermissionsError);
    }

    // ---------------------------------------------------------------- 6b. open() 各模式矩阵
    hdr("6b. QFile::open() 各模式对「不存在 / 已存在」文件的行为");
    {
        struct { const char *name; QIODevice::OpenMode m; } modes[] = {
            {"ReadOnly",        QIODevice::ReadOnly},
            {"WriteOnly",       QIODevice::WriteOnly},
            {"ReadWrite",       QIODevice::ReadWrite},
            {"ReadWrite|Trunc", QIODevice::ReadWrite | QIODevice::Truncate},
            {"WriteOnly|Append",QIODevice::WriteOnly | QIODevice::Append},
            {"ReadWrite|NewOnly", QIODevice::ReadWrite | QIODevice::NewOnly},
        };
        for (int exist = 0; exist < 2; ++exist) {
            for (auto &mm : modes) {
                const QString p = QStringLiteral("/tmp/pk_probe_mode.txt");
                QFile::remove(p);
                if (exist) { QFile f0(p); f0.open(QIODevice::WriteOnly); f0.write("abc", 3); f0.close(); }
                QFile f(p);
                const bool ok = f.open(mm.m);
                QFile probe(p);
                const qint64 sz = ok ? f.size() : -1;
                std::printf("  %-20s exist=%d -> open=%-2d %s error=%d '%-30s' existsAfter=%d size=%d\n",
                            mm.name, exist, (int)ok, ok ? "OK " : "NG ",
                            (int)f.error(), f.errorString().toUtf8().constData(),
                            (int)QFile::exists(p), (int)sz);
                f.close();
                QFile::remove(p);
            }
        }
    }

    // ---------------------------------------------------------------- 7. QFileInfo 连带    // ---------------------------------------------------------------- 7. QFileInfo 连带
    hdr("7. QFileInfo::permissions() / absoluteFilePath() / path() / absolutePath()");
    {
        const QString p = QStringLiteral("/tmp/pk_probe_fi.txt");
        { QFile f(p); f.open(QIODevice::WriteOnly); f.write("x", 1); f.close(); }
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QFileInfo fi(p);
        std::printf("QFileInfo('%s').absoluteFilePath() = '%s'\n", p.toUtf8().constData(), fi.absoluteFilePath().toUtf8().constData());
        std::printf("QFileInfo.absolutePath() = '%s'  path() = '%s'\n", fi.absolutePath().toUtf8().constData(), fi.path().toUtf8().constData());
        std::printf("QFileInfo.permissions() = %d 0o%o\n", I(fi.permissions()), I(fi.permissions()));
        std::printf("QFileInfo.exists()=%d  isFile()=%d\n", (int)fi.exists(), (int)fi.isFile());
        // 相对路径的 absoluteFilePath 形状（消费点 filestest.h 传的是绝对还是相对？）
        QFileInfo rel(QStringLiteral("pk_probe_rel.txt"));
        std::printf("QFileInfo('pk_probe_rel.txt').absoluteFilePath() = '%s'\n", rel.absoluteFilePath().toUtf8().constData());
        QFile::remove(p);
    }

    // ---------------------------------------------------------------- 7b. QFileInfo::absoluteFilePath 边角 + permissions 对已存在文件
    hdr("7b. QFileInfo::absoluteFilePath() 边角 / permissions() 对已存在文件");
    {
        const char *afp[] = {"a/b/c.png", "a/../b", "", "/a/b/../c", "./x", "/a/b/", "x/./y", "..", ".",
                             "a/b/", "a//b", "//a", "/../a", "/a/./b", "a/b/..", "/a/b/..", "a/b/../", "/a/../../b"};
        for (const char *c : afp) {
            std::printf("  QFileInfo(\"%s\").absoluteFilePath() = \"%s\"\n", c,
                        QFileInfo(QString::fromUtf8(c)).absoluteFilePath().toUtf8().constData());
        }
        const QString p = QStringLiteral("/tmp/pk_probe_open_exist.txt");
        { QFile f(p); f.open(QIODevice::WriteOnly); f.write("x", 1); f.close(); }
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
        std::printf("文件 chmod 0640 后:\n");
        std::printf("  QFile::permissions(path)        = %d 0x%x\n", I(QFile::permissions(p)), I(QFile::permissions(p)));
        std::printf("  QFileInfo(path).permissions()   = %d 0x%x\n", I(QFileInfo(p).permissions()), I(QFileInfo(p).permissions()));
        QFile nf(p);   // 未 open
        std::printf("  QFile(path).permissions() (未 open) = %d 0x%x\n", I(nf.permissions()), I(nf.permissions()));
        std::printf("  QFile(path).fileName()            = '%s'\n", nf.fileName().toUtf8().constData());
        QFile::remove(p);
    }

    // ---------------------------------------------------------------- 8. QTemporaryFile    // ---------------------------------------------------------------- 8. QTemporaryFile
    hdr("8. QTemporaryFile 模板展开语义");
    {
        // 与 plugins/impex/exr/tests/kis_exr_test.cpp:54 同一形态
        QString tmpl = QDir::tempPath() + QLatin1String("/krita_XXXXXX") + QLatin1String(".exr");
        std::printf("模板字符串 = '%s'\n", tmpl.toUtf8().constData());
        QTemporaryFile tf(tmpl);
        std::printf("构造后 fileName() = '%s'  (未 open)\n", tf.fileName().toUtf8().constData());
        tf.setAutoRemove(true);
        bool ok = tf.open();
        std::printf("open() 返回 = %d  isOpen=%d  openMode=%d (ReadWrite=%d)\n", (int)ok, (int)tf.isOpen(), (int)tf.openMode(), (int)QIODevice::ReadWrite);
        std::printf("open 后 fileName() = '%s'  exists=%d\n", tf.fileName().toUtf8().constData(), (int)QFile::exists(tf.fileName()));
        const QString name = tf.fileName();
        tf.close();
        std::printf("close 后 exists=%d (autoRemove 在析构才删，不是 close 删)\n", (int)QFile::exists(name));
        std::printf("autoRemove=%d\n", (int)tf.autoRemove());
    }
    std::printf("作用域结束后（析构，autoRemove=true）:\n");
    {
        QTemporaryFile tf(QDir::tempPath() + QLatin1String("/krita_XXXXXX") + QLatin1String(".exr"));
        tf.setAutoRemove(true); tf.open();
        std::printf("  造出 '%s' exists=%d\n", tf.fileName().toUtf8().constData(), (int)QFile::exists(tf.fileName()));
        // 故意不 close，直接析构，验证 autoRemove 是否仍删
    }
    std::printf("  （上面那个 QTemporaryFile 已析构）\n");
    {
        QTemporaryFile tf(QDir::tempPath() + QLatin1String("/krita_XXXXXX") + QLatin1String(".exr"));
        tf.open();
        const QString n = tf.fileName();
        std::printf("autoRemove 默认 = %d，fileName='%s'\n", (int)tf.autoRemove(), n.toUtf8().constData());
        tf.setAutoRemove(false);
        tf.close();
        std::printf("setAutoRemove(false)+close 后 exists=%d（不删）\n", (int)QFile::exists(n));
        QFile::remove(n);
    }
    // 无模板参数的 QTemporaryFile
    {
        QTemporaryFile tf;
        tf.open();
        std::printf("无参 QTemporaryFile::open() fileName='%s'\n", tf.fileName().toUtf8().constData());
    }

    // ---------------------------------------------------------------- 9. QLatin1String 用量
    hdr("9. QLatin1String 在本用量表下需要什么");
    std::printf("QLatin1String(\"/krita_XXXXXX\") 可否与 QString operator+ : ");
    QString s = QDir::tempPath() + QLatin1String("/krita_XXXXXX") + QLatin1String(".exr");
    std::printf("'%s'\n", s.toUtf8().constData());
    std::printf("QLatin1String(\"abc\").size()=%d  data()='%s'\n", (int)QLatin1String("abc").size(), QLatin1String("abc").data());
    std::printf("QString(QLatin1String(\"abc\")) = '%s'\n", QString(QLatin1String("abc")).toUtf8().constData());

    // ---------------------------------------------------------------- 9b. FileError 全枚举
    hdr("9b. QFileDevice::FileError 全枚举真值（垫片 error() 的返回类型）");
    std::printf("NoError=%d ReadError=%d WriteError=%d FatalError=%d ResourceError=%d\n",
                (int)QFileDevice::NoError, (int)QFileDevice::ReadError, (int)QFileDevice::WriteError,
                (int)QFileDevice::FatalError, (int)QFileDevice::ResourceError);
    std::printf("OpenError=%d AbortError=%d TimeOutError=%d UnspecifiedError=%d RemoveError=%d\n",
                (int)QFileDevice::OpenError, (int)QFileDevice::AbortError, (int)QFileDevice::TimeOutError,
                (int)QFileDevice::UnspecifiedError, (int)QFileDevice::RemoveError);
    std::printf("RenameError=%d PositionError=%d ResizeError=%d PermissionsError=%d CopyError=%d\n",
                (int)QFileDevice::RenameError, (int)QFileDevice::PositionError, (int)QFileDevice::ResizeError,
                (int)QFileDevice::PermissionsError, (int)QFileDevice::CopyError);

    // ---------------------------------------------------------------- 9c. exe 位
    hdr("9c. permissions() 是否带 Exe{Owner,User,Group,Other}（判据① 只有 8 个常量）");
    {
        const QString p = QStringLiteral("/tmp/pk_probe_exec.sh");
        { QFile f(p); f.open(QIODevice::WriteOnly); f.write("#!/bin/sh", 9); f.close(); }
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
        std::printf("chmod 0755 后 QFile::permissions      = %d 0x%x\n", I(QFile::permissions(p)), I(QFile::permissions(p)));
        std::printf("chmod 0755 后 QFileInfo::permissions  = %d 0x%x\n", I(QFileInfo(p).permissions()), I(QFileInfo(p).permissions()));
        std::printf("  ExeOwner=%d ExeUser=%d ExeGroup=%d ExeOther=%d\n",
                    I(QFileDevice::ExeOwner), I(QFileDevice::ExeUser), I(QFileDevice::ExeGroup), I(QFileDevice::ExeOther));
        std::printf("  0755 若只映射 8 个 rw 常量，期望值 = %d 0x%x (即丢掉 0x%x)\n",
                    I(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ReadUser|QFileDevice::WriteUser|QFileDevice::ReadGroup|QFileDevice::ReadOther),
                    I(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ReadUser|QFileDevice::WriteUser|QFileDevice::ReadGroup|QFileDevice::ReadOther),
                    I(QFileDevice::ExeOwner|QFileDevice::ExeUser|QFileDevice::ExeGroup|QFileDevice::ExeOther));
        QFile::remove(p);
    }

    // ---------------------------------------------------------------- 9d. TMPDIR vs tempPath
    hdr("9d. QDir::tempPath() 与 $TMPDIR 的关系（realpath?）");
    std::printf("getenv(TMPDIR) = '%s'\n", getenv("TMPDIR") ? getenv("TMPDIR") : "(unset)");
    std::printf("QDir::tempPath() = '%s'\n", QDir::tempPath().toUtf8().constData());
    {
        char rp[4096];
        const char *t = getenv("TMPDIR");
        std::printf("realpath($TMPDIR) = '%s'\n", (t && realpath(t, rp)) ? rp : "(n/a)");
    }

    // ---------------------------------------------------------------- 9e. cleanPath 更多边角
    hdr("9e. QDir::cleanPath 边角（垫片要逐条对齐）");
    const char *cp[] = {"/a/b/../c//d/", "/a/../../b", "", ".", "..", "/", "//", "a/b", "./a/b",
                        "a/./b", "/a/b/", "/a/b//", "a//b", "../../a", "/../a", "a/..", "/a/..", "a/b/../.."};
    for (const char *c : cp) {
        std::printf("  cleanPath(\"%s\") = \"%s\"\n", c, QDir::cleanPath(QString::fromUtf8(c)).toUtf8().constData());
    }

    // ---------------------------------------------------------------- 10. 环境
    hdr("10. 环境");
    std::printf("HOME=%s\n", getenv("HOME") ? getenv("HOME") : "(null)");
    std::printf("QT_QPA_PLATFORM=%s\n", getenv("QT_QPA_PLATFORM") ? getenv("QT_QPA_PLATFORM") : "(unset)");
    std::printf("XDG_CACHE_HOME=%s\n", getenv("XDG_CACHE_HOME") ? getenv("XDG_CACHE_HOME") : "(unset)");

    std::printf("\nPROBE_DONE\n");
    return 0;
}
