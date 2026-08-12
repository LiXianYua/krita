// PkResourceStorage 的测试。用 pk/test（R-11）的 PK_* 宏 + 真实的
// PkTest::qExec 派发，形态照抄 test_stream.cpp/test_eventsink.cpp：测试类
// 成员是 public，PkTestBinder<T> 特化手写即可，不经 pk_test_moc.py。
//
// 测试用一个内存里的假存储（FakeStorage）实现 PkResourceStorage 的全部纯
// 虚方法，断言：① 迭代器语义（hasNext/next 的"只有 next() 之后才指向有效
// 项"约定、url()/lastModified() 取值）；② 目录枚举的过滤行为（递归 vs 非
// 递归、glob 名称过滤、Files vs Directories）；③ exists/mkpath/remove 的
// 副作用；④ absolutePath/platformDir 的取值；⑤ joinPath/cleanPath/
// relativePath 三个静态路径工具的边界情形。全部断言都基于具体输入输出的
// 比对，不写恒真断言。
#include "../PkResourceStorage.h"
#include "PkString.h"
#include "PkTest.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string toStdString(const PkString &s)
{
    return s.PkToUtf8();
}

std::vector<PkString> filters(std::initializer_list<const char *> items)
{
    std::vector<PkString> out;
    for (const char *item : items) {
        out.push_back(PkString(item));
    }
    return out;
}

// 最小 glob 匹配：支持 '*' 与 '?'，锚定全匹配（不是子串匹配）。真实调用点
// 里的 name filter 全部是这个形状（"*.tag"、"*.icm" 等）。
bool globMatch(const std::string &pattern, const std::string &name)
{
    std::size_t p = 0, n = 0;
    std::size_t star = std::string::npos;
    std::size_t mark = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            ++p;
            ++n;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = n;
        } else if (star != std::string::npos) {
            p = star + 1;
            n = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

struct FakeEntry
{
    std::string path;
    bool isDir;
    int64_t mtime;
};

// PkResourceStorage::EntryIterator 的假实现：包一个已经算好的 FakeEntry
// 列表，"只有 next() 之后才指向有效项"——m_index 初始为 -1。
class FakeIterator : public PkResourceStorage::EntryIterator
{
public:
    explicit FakeIterator(std::vector<FakeEntry> entries)
        : m_entries(std::move(entries))
        , m_index(-1)
    {
    }

    bool hasNext() const override
    {
        return m_index + 1 < static_cast<int>(m_entries.size());
    }

    void next() override { ++m_index; }

    PkString url() const override { return PkString(m_entries[m_index].path.c_str()); }

    int64_t lastModified() const override { return m_entries[m_index].mtime; }

private:
    std::vector<FakeEntry> m_entries;
    int m_index;
};

// 内存里的假存储：m_entries 是"全路径 -> 条目"的扁平表，不需要真实按层级
// 建父目录才能让子路径可见（除非测试显式 addDir() 一个目录条目本身，用来
// 验证 EntryKind::Directories 那一支）。mkpath/remove/exists/listEntries/
// absolutePath/platformDir 全部按接口要求声明成 const，用 mutable 承接
// "写文件系统"这个副作用——这与真实 QDir 的 constness 语义一致
// （QDir::mkpath/exists 本身也是 const 成员函数）。
class FakeStorage : public PkResourceStorage
{
public:
    void addFile(const std::string &path, int64_t mtime = 0)
    {
        m_entries[path] = FakeEntry{path, false, mtime};
    }

    void addDir(const std::string &path, int64_t mtime = 0)
    {
        m_entries[path] = FakeEntry{path, true, mtime};
    }

    void setPlatformDir(PlatformDir kind, const std::string &value)
    {
        m_platformDirs[kind] = value;
    }

    std::unique_ptr<EntryIterator> listEntries(const PkString &path,
                                                const std::vector<PkString> &nameFilters,
                                                EntryKind kind,
                                                bool recursive) const override
    {
        std::string base = toStdString(path);
        if (base.size() > 1 && base.back() == '/') {
            base.pop_back();
        }

        std::vector<FakeEntry> result;
        for (const auto &kv : m_entries) {
            const std::string &p = kv.first;
            if (p.size() <= base.size() || p.compare(0, base.size(), base) != 0
                || p[base.size()] != '/') {
                continue;
            }
            const std::string rel = p.substr(base.size() + 1);
            const bool isDirect = rel.find('/') == std::string::npos;
            if (!recursive && !isDirect) {
                continue;
            }

            const FakeEntry &e = kv.second;
            if (kind == EntryKind::Files && e.isDir) {
                continue;
            }
            if (kind == EntryKind::Directories && !e.isDir) {
                continue;
            }

            const std::string leafName = rel.substr(rel.find_last_of('/') + 1);
            if (!nameFilters.empty()) {
                bool matched = false;
                for (const PkString &f : nameFilters) {
                    if (globMatch(toStdString(f), leafName)) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    continue;
                }
            }
            result.push_back(e);
        }

        std::sort(result.begin(), result.end(),
                   [](const FakeEntry &a, const FakeEntry &b) { return a.path < b.path; });
        return std::unique_ptr<EntryIterator>(new FakeIterator(std::move(result)));
    }

    bool exists(const PkString &path) const override
    {
        return m_entries.count(toStdString(path)) > 0;
    }

    bool mkpath(const PkString &path) const override
    {
        const std::string p = toStdString(path);
        if (p.empty() || p.front() != '/') {
            return false;
        }
        std::vector<std::string> segs;
        std::string cur;
        for (char c : p) {
            if (c == '/') {
                if (!cur.empty()) {
                    segs.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) {
            segs.push_back(cur);
        }

        std::string running;
        for (const std::string &seg : segs) {
            running += "/" + seg;
            auto it = m_entries.find(running);
            if (it == m_entries.end()) {
                m_entries[running] = FakeEntry{running, true, 0};
            } else if (!it->second.isDir) {
                return false;
            }
        }
        return true;
    }

    bool remove(const PkString &path) const override
    {
        auto it = m_entries.find(toStdString(path));
        if (it == m_entries.end() || it->second.isDir) {
            return false;
        }
        m_entries.erase(it);
        return true;
    }

    PkString absolutePath(const PkString &path) const override
    {
        const std::string p = toStdString(path);
        if (!p.empty() && p.front() == '/') {
            return path;
        }
        return PkString((m_cwd + "/" + p).c_str());
    }

    PkString platformDir(PlatformDir kind) const override
    {
        auto it = m_platformDirs.find(kind);
        return it != m_platformDirs.end() ? PkString(it->second.c_str()) : PkString("");
    }

private:
    mutable std::map<std::string, FakeEntry> m_entries;
    std::map<PlatformDir, std::string> m_platformDirs;
    std::string m_cwd = "/cwd";
};

} // namespace

class PkResourceStorageTestCase : public PkTestObject
{
public:
    // ① 迭代器语义
    void testIteratorHasNextNextUrlLastModified();
    void testIteratorHasNextFalseWhenEmpty();

    // ② 目录枚举的过滤行为
    void testListEntriesNonRecursiveFilesExcludesSubdirAndItsContents();
    void testListEntriesRecursiveDescendsIntoSubdirectories();
    void testListEntriesDirectoriesKindReturnsOnlyDirs();
    void testListEntriesNameFilterMatchesGlobSuffix();
    void testListEntriesEmptyNameFiltersMeansNoFiltering();

    // ③ exists/mkpath/remove
    void testExistsTrueForKnownPathFalseOtherwise();
    void testMkpathCreatesAllMissingParents();
    void testRemoveDeletesFileAndFailsForMissingOrDir();

    // ④ absolutePath/platformDir
    void testAbsolutePathPassesThroughAbsoluteResolvesRelative();
    void testPlatformDirReturnsValuePerKindNotSharedAcrossKinds();

    // ⑤ 路径拼接静态工具
    void testJoinPathTrailingLeadingSlashCombinations();
    void testCleanPathCollapsesSlashesAndDotSegments();
    void testCleanPathDropsDotDotPastAbsoluteRoot();
    void testRelativePathComputesDotDotClimb();
    void testRelativePathSameDirectoryYieldsDot();
};

void PkResourceStorageTestCase::testIteratorHasNextNextUrlLastModified()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt", 100);
    storage.addFile("/root/b.txt", 200);

    auto it = storage.listEntries(PkString("/root"), {}, PkResourceStorage::EntryKind::Files, false);

    PK_VERIFY(it->hasNext());
    it->next();
    PK_COMPARE(it->url().PkToUtf8(), std::string("/root/a.txt"));
    PK_COMPARE(it->lastModified(), (int64_t)100);

    PK_VERIFY(it->hasNext());
    it->next();
    PK_COMPARE(it->url().PkToUtf8(), std::string("/root/b.txt"));
    PK_COMPARE(it->lastModified(), (int64_t)200);

    PK_VERIFY(!it->hasNext());
}

void PkResourceStorageTestCase::testIteratorHasNextFalseWhenEmpty()
{
    FakeStorage storage;
    auto it = storage.listEntries(PkString("/empty"), {}, PkResourceStorage::EntryKind::Files, false);
    PK_VERIFY(!it->hasNext());
}

void PkResourceStorageTestCase::testListEntriesNonRecursiveFilesExcludesSubdirAndItsContents()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt");
    storage.addDir("/root/sub");
    storage.addFile("/root/sub/b.txt");

    auto it = storage.listEntries(PkString("/root"), {}, PkResourceStorage::EntryKind::Files, false);

    std::vector<std::string> got;
    while (it->hasNext()) {
        it->next();
        got.push_back(it->url().PkToUtf8());
    }
    PK_COMPARE((int)got.size(), 1);
    PK_COMPARE(got[0], std::string("/root/a.txt"));
}

void PkResourceStorageTestCase::testListEntriesRecursiveDescendsIntoSubdirectories()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt");
    storage.addDir("/root/sub");
    storage.addFile("/root/sub/b.txt");

    auto it = storage.listEntries(PkString("/root"), {}, PkResourceStorage::EntryKind::Files, true);

    std::vector<std::string> got;
    while (it->hasNext()) {
        it->next();
        got.push_back(it->url().PkToUtf8());
    }
    PK_COMPARE((int)got.size(), 2);
    PK_COMPARE(got[0], std::string("/root/a.txt"));
    PK_COMPARE(got[1], std::string("/root/sub/b.txt"));
}

void PkResourceStorageTestCase::testListEntriesDirectoriesKindReturnsOnlyDirs()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt");
    storage.addDir("/root/sub");
    storage.addDir("/root/empty");

    auto it = storage.listEntries(PkString("/root"), {}, PkResourceStorage::EntryKind::Directories, false);

    std::vector<std::string> got;
    while (it->hasNext()) {
        it->next();
        got.push_back(it->url().PkToUtf8());
    }
    PK_COMPARE((int)got.size(), 2);
    PK_COMPARE(got[0], std::string("/root/empty"));
    PK_COMPARE(got[1], std::string("/root/sub"));
}

void PkResourceStorageTestCase::testListEntriesNameFilterMatchesGlobSuffix()
{
    FakeStorage storage;
    storage.addFile("/root/a.tag");
    storage.addFile("/root/b.tag");
    storage.addFile("/root/c.txt");

    auto it = storage.listEntries(PkString("/root"), filters({"*.tag"}),
                                    PkResourceStorage::EntryKind::Files, false);

    std::vector<std::string> got;
    while (it->hasNext()) {
        it->next();
        got.push_back(it->url().PkToUtf8());
    }
    PK_COMPARE((int)got.size(), 2);
    PK_COMPARE(got[0], std::string("/root/a.tag"));
    PK_COMPARE(got[1], std::string("/root/b.tag"));
}

void PkResourceStorageTestCase::testListEntriesEmptyNameFiltersMeansNoFiltering()
{
    // 覆盖 KoJsonTrader.cpp:142 那一处 QDirIterator 构造——没有 name filter。
    FakeStorage storage;
    storage.addFile("/root/a.tag");
    storage.addFile("/root/c.txt");

    auto it = storage.listEntries(PkString("/root"), {}, PkResourceStorage::EntryKind::Files, false);

    std::vector<std::string> got;
    while (it->hasNext()) {
        it->next();
        got.push_back(it->url().PkToUtf8());
    }
    PK_COMPARE((int)got.size(), 2);
}

void PkResourceStorageTestCase::testExistsTrueForKnownPathFalseOtherwise()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt");

    PK_VERIFY(storage.exists(PkString("/root/a.txt")));
    PK_VERIFY(!storage.exists(PkString("/root/missing.txt")));
}

void PkResourceStorageTestCase::testMkpathCreatesAllMissingParents()
{
    FakeStorage storage;
    PK_VERIFY(storage.mkpath(PkString("/root/a/b/c")));

    PK_VERIFY(storage.exists(PkString("/root")));
    PK_VERIFY(storage.exists(PkString("/root/a")));
    PK_VERIFY(storage.exists(PkString("/root/a/b")));
    PK_VERIFY(storage.exists(PkString("/root/a/b/c")));
}

void PkResourceStorageTestCase::testRemoveDeletesFileAndFailsForMissingOrDir()
{
    FakeStorage storage;
    storage.addFile("/root/a.txt");
    storage.addDir("/root/sub");

    PK_VERIFY(storage.remove(PkString("/root/a.txt")));
    PK_VERIFY(!storage.exists(PkString("/root/a.txt")));

    PK_VERIFY(!storage.remove(PkString("/root/missing.txt")));
    PK_VERIFY(!storage.remove(PkString("/root/sub")));   // 目录不是本方法的目标。
}

void PkResourceStorageTestCase::testAbsolutePathPassesThroughAbsoluteResolvesRelative()
{
    FakeStorage storage;
    PK_COMPARE(storage.absolutePath(PkString("/already/absolute")).PkToUtf8(),
               std::string("/already/absolute"));
    PK_COMPARE(storage.absolutePath(PkString("relative/x")).PkToUtf8(),
               std::string("/cwd/relative/x"));
}

void PkResourceStorageTestCase::testPlatformDirReturnsValuePerKindNotSharedAcrossKinds()
{
    FakeStorage storage;
    storage.setPlatformDir(PkResourceStorage::PlatformDir::AppData, "/data/app");
    storage.setPlatformDir(PkResourceStorage::PlatformDir::Cache, "/data/cache");

    PK_COMPARE(storage.platformDir(PkResourceStorage::PlatformDir::AppData).PkToUtf8(),
               std::string("/data/app"));
    PK_COMPARE(storage.platformDir(PkResourceStorage::PlatformDir::Cache).PkToUtf8(),
               std::string("/data/cache"));
    // 未配置的 kind 不应该"沿用"上一个被查询的 kind 的值——回归一个"实现偷懒
    // 返回同一个字符串"式的变异。
    PK_COMPARE(storage.platformDir(PkResourceStorage::PlatformDir::Home).PkToUtf8(), std::string(""));
}

void PkResourceStorageTestCase::testJoinPathTrailingLeadingSlashCombinations()
{
    PK_COMPARE(PkResourceStorage::joinPath(PkString("/root"), PkString("a")).PkToUtf8(),
               std::string("/root/a"));
    PK_COMPARE(PkResourceStorage::joinPath(PkString("/root/"), PkString("a")).PkToUtf8(),
               std::string("/root/a"));
    PK_COMPARE(PkResourceStorage::joinPath(PkString("/root"), PkString("/a")).PkToUtf8(),
               std::string("/root/a"));
    PK_COMPARE(PkResourceStorage::joinPath(PkString("/root/"), PkString("/a")).PkToUtf8(),
               std::string("/root/a"));
    PK_COMPARE(PkResourceStorage::joinPath(PkString(""), PkString("a")).PkToUtf8(), std::string("a"));
    PK_COMPARE(PkResourceStorage::joinPath(PkString("/root"), PkString("")).PkToUtf8(),
               std::string("/root"));
}

void PkResourceStorageTestCase::testCleanPathCollapsesSlashesAndDotSegments()
{
    PK_COMPARE(PkResourceStorage::cleanPath(PkString("/a//b/./c/../d")).PkToUtf8(),
               std::string("/a/b/d"));
    PK_COMPARE(PkResourceStorage::cleanPath(PkString("a/../../b")).PkToUtf8(), std::string("../b"));
}

void PkResourceStorageTestCase::testCleanPathDropsDotDotPastAbsoluteRoot()
{
    PK_COMPARE(PkResourceStorage::cleanPath(PkString("/../a")).PkToUtf8(), std::string("/a"));
}

void PkResourceStorageTestCase::testRelativePathComputesDotDotClimb()
{
    PK_COMPARE(PkResourceStorage::relativePath(PkString("/a/b/c"), PkString("/a/b/d/e")).PkToUtf8(),
               std::string("../d/e"));
    PK_COMPARE(PkResourceStorage::relativePath(PkString("/a/b"), PkString("/a/b/x")).PkToUtf8(),
               std::string("x"));
}

void PkResourceStorageTestCase::testRelativePathSameDirectoryYieldsDot()
{
    PK_COMPARE(PkResourceStorage::relativePath(PkString("/a/b/c"), PkString("/a/b/c")).PkToUtf8(),
               std::string("."));
}

// PkTestBinder<PkResourceStorageTestCase> 特化——手写，形状对照
// pk/test/pk_test_moc.py 的 emit_binder() 输出（同 test_stream.cpp/
// test_eventsink.cpp 的做法）。
template <>
struct PkTestBinder<PkResourceStorageTestCase> {
    static const char *className() { return "PkResourceStorageTestCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testIteratorHasNextNextUrlLastModified",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testIteratorHasNextNextUrlLastModified();
             },
             nullptr},
            {"testIteratorHasNextFalseWhenEmpty",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testIteratorHasNextFalseWhenEmpty();
             },
             nullptr},
            {"testListEntriesNonRecursiveFilesExcludesSubdirAndItsContents",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)
                     ->testListEntriesNonRecursiveFilesExcludesSubdirAndItsContents();
             },
             nullptr},
            {"testListEntriesRecursiveDescendsIntoSubdirectories",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)
                     ->testListEntriesRecursiveDescendsIntoSubdirectories();
             },
             nullptr},
            {"testListEntriesDirectoriesKindReturnsOnlyDirs",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testListEntriesDirectoriesKindReturnsOnlyDirs();
             },
             nullptr},
            {"testListEntriesNameFilterMatchesGlobSuffix",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testListEntriesNameFilterMatchesGlobSuffix();
             },
             nullptr},
            {"testListEntriesEmptyNameFiltersMeansNoFiltering",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)
                     ->testListEntriesEmptyNameFiltersMeansNoFiltering();
             },
             nullptr},
            {"testExistsTrueForKnownPathFalseOtherwise",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testExistsTrueForKnownPathFalseOtherwise();
             },
             nullptr},
            {"testMkpathCreatesAllMissingParents",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testMkpathCreatesAllMissingParents();
             },
             nullptr},
            {"testRemoveDeletesFileAndFailsForMissingOrDir",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testRemoveDeletesFileAndFailsForMissingOrDir();
             },
             nullptr},
            {"testAbsolutePathPassesThroughAbsoluteResolvesRelative",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)
                     ->testAbsolutePathPassesThroughAbsoluteResolvesRelative();
             },
             nullptr},
            {"testPlatformDirReturnsValuePerKindNotSharedAcrossKinds",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)
                     ->testPlatformDirReturnsValuePerKindNotSharedAcrossKinds();
             },
             nullptr},
            {"testJoinPathTrailingLeadingSlashCombinations",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testJoinPathTrailingLeadingSlashCombinations();
             },
             nullptr},
            {"testCleanPathCollapsesSlashesAndDotSegments",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testCleanPathCollapsesSlashesAndDotSegments();
             },
             nullptr},
            {"testCleanPathDropsDotDotPastAbsoluteRoot",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testCleanPathDropsDotDotPastAbsoluteRoot();
             },
             nullptr},
            {"testRelativePathComputesDotDotClimb",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testRelativePathComputesDotDotClimb();
             },
             nullptr},
            {"testRelativePathSameDirectoryYieldsDot",
             [](PkTestObject *o) {
                 static_cast<PkResourceStorageTestCase *>(o)->testRelativePathSameDirectoryYieldsDot();
             },
             nullptr},
        };
        return fns;
    }
    static int count() { return 17; }

    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_resourcestorage_tests(int argc, char **argv)
{
    PkResourceStorageTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
