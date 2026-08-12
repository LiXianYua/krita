// R-12 评审 C-1 探针：QDir::cleanPath 在越过根的 ".." 上的真实行为，
// 外加 filePath()/absoluteFilePath() 对绝对路径 leaf 的行为（I-2）、
// relativeFilePath() 在 target 是 base 祖先时的尾部斜杠行为（M-2）。
// 与 pk/port/probe/probe_qiodevice.cpp 同规格：链真 Qt，产物不留在源树里，
// 不参与任何构建，不是测试。
//
// Build:
//   QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
//   g++ -fPIC -std=c++17 pk/port/probe/probe_qdir.cpp -o /tmp/pkport-probe/probe_qdir \
//     -I$QT/include/x86_64-linux-gnu/qt5 -I$QT/include/x86_64-linux-gnu/qt5/QtCore \
//     -L$QT/lib/x86_64-linux-gnu -lQt5Core
//   LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu /tmp/pkport-probe/probe_qdir
//   LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu ldd /tmp/pkport-probe/probe_qdir | grep -i qt
//
// 实测环境：Qt 5.15.13（运行时与编译期一致），g++ 13.3.0。

#include <QDir>
#include <QString>

#include <cstdio>

static void cp(const char *in)
{
    printf("cleanPath(%-16s) = \"%s\"\n", in, QDir::cleanPath(QString::fromUtf8(in)).toUtf8().constData());
}

static void fp(const char *dir, const char *name)
{
    printf("QDir(%s).filePath(%s) = \"%s\"\n", dir, name,
           QDir(QString::fromUtf8(dir)).filePath(QString::fromUtf8(name)).toUtf8().constData());
}

static void afp(const char *dir, const char *name)
{
    printf("QDir(%s).absoluteFilePath(%s) = \"%s\"\n", dir, name,
           QDir(QString::fromUtf8(dir)).absoluteFilePath(QString::fromUtf8(name)).toUtf8().constData());
}

static void rel(const char *base, const char *target)
{
    printf("QDir(%s).relativeFilePath(%s) = \"%s\"\n", base, target,
           QDir(QString::fromUtf8(base)).relativeFilePath(QString::fromUtf8(target)).toUtf8().constData());
}

int main()
{
    printf("---- cleanPath: 越过根的 \"..\"（C-1 核心）----\n");
    cp("/..");
    cp("/../..");
    cp("/../a");
    cp("/a/../..");

    printf("\n---- cleanPath: 既有形态回归（确认没改坏）----\n");
    cp("//");
    cp("/");
    cp(".");
    cp("");
    cp("../..");
    cp("a/./b");
    cp("a/b/../..");

    printf("\n---- filePath / absoluteFilePath：leaf 为绝对路径（I-2）----\n");
    fp("/root", "/a");
    afp("/root", "/a");
    fp("/root/", "/a");
    afp("/root/", "/a");

    printf("\n---- filePath：joinPath() 与真 Qt 的 7 处未登记分歧（评审 M-1）----\n");
    fp("", "a");
    fp("", "");
    fp("/root/", "");
    fp("/root/", "a/");
    fp("/root/", "./a");
    fp("/root/", "../a");
    fp("/root/", "a/b");

    printf("\n---- relativeFilePath：target 是 base 的祖先（M-2 尾部斜杠）----\n");
    rel("/a/b/c", "/a");
    rel("/a/b", "/a");
    rel("/a/b/c", "/a/b/c");
    rel("/a/b/c", "/a/b/d/e");
    rel("/a/b", "/a/b/x");
    rel("/a/b/c", "/x/y");

    return 0;
}
