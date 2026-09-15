#include "PkTestSupportSelfTest.h"

// 三个测试函数各自钉住机器的一环。**刻意不 include 任何 pk 头**——凡本文件能
// 编过的东西，都是 PkTestCompatAll.h 被 `-include` 强制激活带来的（这正是
// libs/**/tests 里那一大批「靠 Qt 传递 include 才编得过」的测试所依赖的机制）。
//
// R-77 增补：下面两个用例要**直接调** `sdk/tests/filestest.h` 里的真实函数，
// 所以必须 include 它（这是本文件唯一的 Krita 侧 include；它自己那套重量级
// include——testutil.h/KisDocument 一族——已在 R-77 用 `#ifndef
// KRITA_TESTSDK_PK_NATIVE` 挡在 pk 栈外，见 filestest.h 的端口化注释）。
// **这条与上面「刻意不 include」的取舍是刻意的**：那三行说的是「机器自测不要
// 靠自己的 include 去补垫片」，而这里要证的恰恰是「垫片面上那三个真函数跑得起来」
// ——那是 brief §3.4 点名要的收尾证据，代价是本 target 从此多一个 Krita 侧依赖面。
// 它仍然**零 Krita 库依赖**（只多了一个头），所以「红了就是机器坏了」这条性质
// 只被削弱一点：若垫片面塌了，本 target 会红在垫片上——这正是要暴露的。
#include "filestest.h"

void PkTestSupportSelfTest::testCompatMacroAliases()
{
    // QCOMPARE / QVERIFY / QVERIFY2 走 pk/test/compat/QTest 的别名——未做全量
    // sed 的真实测试源就是这种写法（pk/test/README.md §1 的 17 项 API 面）。
    // 字面量两侧都是 int，避开 PK_COMPARE 内部的 -Wsign-compare。
    QCOMPARE(1 + 1, 2);
    QVERIFY(true);
    QVERIFY2(2 > 1, "QVERIFY2 的消息参数必须能透传");
}

void PkTestSupportSelfTest::testForceIncludedScalarsAreActive()
{
    // qint32 是 pk/global 的标量族；qAbs 来自 pk/test/compat/QtGlobal（经
    // compat/QObject 传递）。两者本 TU 都没 include，能编过即证明聚合头
    // 被 -include 到了。
    qint32 value = -7;
    QCOMPARE(qAbs(value), 7);

    // ⚠ pkBound 返回 const T&（照抄 Qt 的 qBound 签名），实参是字面量时那个引用指
    // 向临时量，只在同一条 full-expression 内有效。PK_COMPARE 会把两个操作数拆到
    // 不同语句里求值，直接写 QCOMPARE(pkBound(0, 42, 10), 10) 读到的是悬垂值
    // （实测拿到 1）。pk/global/PkGlobal.h:150-157 明写「字面量调用先拷进具名变量」
    // ——这里照它做，顺带把这条语义钉在机器自测里。
    const int bounded = pkBound(0, 42, 10);
    QCOMPARE(bounded, 10);
}

void PkTestSupportSelfTest::testPreactivatedCompatTypesAreActive()
{
    // 只取 sizeof：证明类型完整可见（聚合头的 __has_include 预激活段生效），
    // 又不去赌各垫片成员函数的签名——那属于被测面，不属于机器。
    QVERIFY(sizeof(QRect) > 0);
    QVERIFY(sizeof(QString) > 0);
    QVERIFY(sizeof(QList<int>) > 0);

    // PkNamespace.h 的枚举族同样由聚合头预激活。
    Pk::Orientation orientation = Pk::Horizontal;
    QVERIFY(orientation == Pk::Horizontal);
}

// ---------------------------------------------------------------------------
// R-77（brief §3.4）：filestest.h 端口化出来的三个函数——真造文件、真改权限、
// 真删、断言**可观察结果**（不是「跑到了没报错」）。
//
// 断言刻意只钉「相对变化」而不是绝对位模式：新建文件的权限受 umask 影响
// （0644/0664/0600 都合法），钉死绝对值会在换 umask 的机器上假红。
// `restorePermissionsToReadAndWrite` 的效果则与 umask 无关——它把 8 个 rw 位
// **全部**点亮（filestest.h:190-203），所以那一条断言是确定的。
// 真 Qt 的每一条语义都有探针原始输出（task1-report.md §3.2）。
// ---------------------------------------------------------------------------
void PkTestSupportSelfTest::testFilestestPortedHelpersAreRunnable()
{
    // ① impexTempFilesDir()：这一行把四个垫片串在一起——
    //    QStandardPaths::writableLocation(CacheLocation) + QDir::cleanPath
    //    + QStringLiteral + QDir(path).mkpath(".")。
    //    真 Qt 语义（探针 §1）：`QDir(p).mkpath(".")` **创建 p 自己**，且返回值
    //    为真之后 p 确实 exists。所以这里真正钉住的是「目录被造出来了」。
    const PkString dir = TestUtil::impexTempFilesDir();
    QVERIFY(!dir.isEmpty());
    QVERIFY(QDir(dir).exists());

    const PkString filePath = dir + "r77_selftest_perms.txt";

    // 干净起点：上一次运行若留下同名文件先删掉（QFile::remove 对不存在路径返回
    // false，不检查返回值）。
    QFile::remove(filePath);
    const QFileInfo info(filePath);
    QVERIFY(!info.exists());

    // ② 真造文件：prepareFile(false,false) 对**不存在**的路径走
    //    `QFile::open(QIODevice::ReadWrite)` 这一支——真 Qt 语义是**创建**
    //    （探针 §6）。R-77 前这条路径在 pk 栈上根本走不通（垫片不存在）。
    TestUtil::prepareFile(info, false, false);
    QVERIFY(info.exists());
    QVERIFY(QFile::permissions(filePath).testFlag(QFileDevice::ReadUser));

    // ③ 真改权限（去掉写）：prepareFile(true,false) 清掉 4 个 Write 位。
    TestUtil::prepareFile(info, true, false);
    {
        const QFileDevice::Permissions p = QFile::permissions(filePath);
        QVERIFY(!p.testFlag(QFileDevice::WriteUser));
        QVERIFY(!p.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!p.testFlag(QFileDevice::WriteGroup));
        QVERIFY(!p.testFlag(QFileDevice::WriteOther));
        QVERIFY(p.testFlag(QFileDevice::ReadUser)); // 读位没被连带清掉
    }

    // ④ restorePermissionsToReadAndWrite：8 个 rw 位全部点亮（与 umask 无关）。
    TestUtil::restorePermissionsToReadAndWrite(info);
    {
        const QFileDevice::Permissions p = QFile::permissions(filePath);
        QVERIFY(p.testFlag(QFileDevice::ReadUser));
        QVERIFY(p.testFlag(QFileDevice::ReadOwner));
        QVERIFY(p.testFlag(QFileDevice::ReadGroup));
        QVERIFY(p.testFlag(QFileDevice::ReadOther));
        QVERIFY(p.testFlag(QFileDevice::WriteUser));
        QVERIFY(p.testFlag(QFileDevice::WriteOwner));
        QVERIFY(p.testFlag(QFileDevice::WriteGroup));
        QVERIFY(p.testFlag(QFileDevice::WriteOther));
    }

    // ⑤ 真改权限（去掉读）：prepareFile(false,true) 清掉 4 个 Read 位。
    TestUtil::prepareFile(info, false, true);
    {
        const QFileDevice::Permissions p = QFile::permissions(filePath);
        QVERIFY(!p.testFlag(QFileDevice::ReadUser));
        QVERIFY(!p.testFlag(QFileDevice::ReadOwner));
        QVERIFY(!p.testFlag(QFileDevice::ReadGroup));
        QVERIFY(!p.testFlag(QFileDevice::ReadOther));
    }
    TestUtil::restorePermissionsToReadAndWrite(info);

    // ⑥ 真删：QFile::remove 对存在的文件返回 true、对**不存在**的返回 false
    //    （真 Qt 探针 §5 实测 1 / 0）——两次调用把两个分支都钉住。
    QVERIFY(QFile::remove(filePath));
    QVERIFY(!info.exists());
    QVERIFY(!QFile::remove(filePath));
}

// ---------------------------------------------------------------------------
// R-77：QTemporaryFile 垫片。唯一真实调用点 plugins/impex/exr/tests/kis_exr_test.cpp:54
// 走的是 **Qt 栈**，pk 栈里没有别的消费者 ⇒ 不在这里跑就完全没被执行过。
// 三条语义全部探针实测（报告 §3.2 §8）：
//   * open() **之前** fileName() 是空串；
//   * open() = open(QIODevice::ReadWrite)，展开 `XXXXXX`、**保留后缀**；
//   * close() **不删**文件，删除只发生在**析构**（autoRemove 默认 true）。
// 这一条同时把 §3.1 的「连带」项 `QDir::tempPath()` 与 `QLatin1String` 钉在
// 收尾路径上（`QLatin1String` 由 pk/string/compat/QString 的
// `#define QLatin1String PkString` 提供，即 brief 说的「或等效」）。
// ---------------------------------------------------------------------------
void PkTestSupportSelfTest::testTemporaryFileShimCreatesAndAutoremoves()
{
    PkString created;
    {
        QTemporaryFile tmp(QDir::tempPath() + QLatin1String("/r77_selftest_XXXXXX")
                           + QLatin1String(".png"));
        tmp.setAutoRemove(true);

        QVERIFY(tmp.fileName().isEmpty()); // 真 Qt：open() 前是空串

        QVERIFY(tmp.open());
        created = tmp.fileName();
        QVERIFY(!created.isEmpty());
        QVERIFY(QFileInfo(created).exists());

        tmp.close();
        QVERIFY(QFileInfo(created).exists()); // 真 Qt：close 不删
    }
    // 真 Qt：autoRemove 默认 true ⇒ 析构时删除，作用域一过文件就没了。
    QVERIFY(!created.isEmpty());
    QVERIFY(!QFileInfo(created).exists());
}

// ---------------------------------------------------------------------------
// R-77：`QDir::cleanPath` / `QFileInfo::absoluteFilePath` 的**探针契约**逐条钉在
// 收尾路径上。两者共用 `sdk/tests/compat/PkPathClean.h` 的同一份算法，差别只有
// 「尾斜杠保不保留」一条（探针 §9e 丢、§7b 保留）。
//
// 为什么值得单列一张表：这两个函数是 `impexTempFilesDir()`（被上面那个用例走通）
// 与 `prepareFile()` 一族 `absoluteFilePath()` 的**唯一落点**，而端到端只覆盖了
// 一两条路径形态。表里的 18 + 6 条全是 `pk/test/oracle/probe_file_shims.cpp` 在真
// Qt 5.15.7 上**实测**出来的字符串，不是从 Qt 文档推的——「Qt 的行为一律去问真 Qt」。
// 任何一条对不上就是垫片漂了，这里当场红。
// ---------------------------------------------------------------------------
void PkTestSupportSelfTest::testPathCleanProbeContract()
{
    struct Case { const char *in; const char *out; };
    // 探针 §9e 全部 18 条（keepTrailingSlash = false）。
    static const Case cleanCases[] = {
        { "/a/b/../c//d/", "/a/c/d" },   { "/a/../../b", "/../b" },
        { "",              ""      },    { ".",          "."     },
        { "..",            ".."    },    { "/",          "/"     },
        { "//",            "/"     },    { "a/b",        "a/b"   },
        { "./a/b",         "a/b"   },    { "a/./b",      "a/b"   },
        { "/a/b/",         "/a/b"  },    { "/a/b//",     "/a/b"  },
        { "a//b",          "a/b"   },    { "../../a",    "../../a" },
        { "/../a",         "/../a" },    { "a/..",       "."     },
        { "/a/..",         "/"     },    { "a/b/../..",  "."     },
    };
    for (const Case &c : cleanCases) {
        const PkString got = QDir::cleanPath(PkString(c.in));
        QVERIFY2(got == PkString(c.out), c.in);
    }

    // 探针 §7b 里的相对/绝对形态（expectation 里 cwd 由 QDir::currentPath() 现取）。
    const PkString cwd = QDir::currentPath();
    QVERIFY(!cwd.isEmpty());
    QVERIFY2(QFileInfo(PkString("/a/b/../c")).absoluteFilePath() == PkString("/a/c"),
             "/a/b/../c");
    QVERIFY2(QFileInfo(PkString("//a")).absoluteFilePath() == PkString("/a"), "//a");
    QVERIFY2(QFileInfo(PkString("/../a")).absoluteFilePath() == PkString("/../a"), "/../a");
    QVERIFY2(QFileInfo(PkString("/a/../../b")).absoluteFilePath() == PkString("/../b"),
             "/a/../../b");
    QVERIFY2(QFileInfo(PkString("")).absoluteFilePath() == PkString(""), "(empty)");
    // 尾斜杠**保留**（这条是 cleanPath 与 absoluteFilePath 唯一的分歧点）。
    QVERIFY2(QFileInfo(PkString("/a/b/")).absoluteFilePath() == PkString("/a/b/"), "/a/b/");
    QVERIFY2(QFileInfo(PkString("a/b/..")).absoluteFilePath() == PkString(cwd + "/a"), "a/b/..");
    QVERIFY2(QFileInfo(PkString("a/b/../")).absoluteFilePath() == PkString(cwd + "/a/"),
             "a/b/../");
}

SIMPLE_TEST_MAIN(PkTestSupportSelfTest)
