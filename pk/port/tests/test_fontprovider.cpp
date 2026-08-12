// PkFontProvider 的测试。形态照抄 test_resourcestorage.cpp：用 R-11 的 PK_*
// 宏 + 真实的 PkTest::qExec 派发，PkTestBinder<T> 特化手写，不经
// pk_test_moc.py。
//
// FakeFontProvider 是内存里的假字体提供者：注册表按家族名分组，每条记录带
// 权重/语言/字符集覆盖（用一个码点集合表示）。断言覆盖任务要求的三类行为：
// ① 按家族名匹配、② 匹配不到指定家族时落到候选列表里的通用回退家族、
// ③ 字符集覆盖查询——外加候选排序（按权重距离，不是"谁先注册谁在前"这种
// 会被偷懒实现蒙混过去的顺序）、query.charset 过滤、bestMatch 的
// 真/假两支、allFonts 枚举、②配置与路径族的几个方法。全部断言基于具体输入
// 输出比对，不写恒真断言。
#include "../PkFontProvider.h"
#include "PkString.h"
#include "PkTest.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

std::string toStdString(const PkString &s)
{
    return s.PkToUtf8();
}

struct FakeFontRecord
{
    std::string family;
    PkFontProvider::FontHandle handle;
    int weight;
    std::vector<std::string> languages;
    std::set<char32_t> charset;
};

// 内存里的假字体提供者。sortedMatches() 的匹配/回退/排序/charset 过滤逻辑
// 是测试要验证的核心行为，全部手写、不调用真实 fontconfig。
class FakeFontProvider : public PkFontProvider
{
public:
    void registerFont(const std::string &family, const std::string &filePath, int faceIndex,
                       int weight, std::vector<std::string> languages = {},
                       std::set<char32_t> charset = {})
    {
        FakeFontRecord rec;
        rec.family = family;
        rec.handle.filePath = PkString(filePath.c_str());
        rec.handle.faceIndex = faceIndex;
        rec.weight = weight;
        rec.languages = std::move(languages);
        rec.charset = std::move(charset);
        m_records.push_back(std::move(rec));
    }

    bool initialize(const PkString &configSearchPath) override
    {
        m_configSearchPath = toStdString(configSearchPath);
        m_initialized = true;
        return true;
    }

    bool addFontFile(const PkString &path) override
    {
        m_addedFiles.push_back(toStdString(path));
        return true;
    }

    bool addFontDirectory(const PkString &path) override
    {
        m_dirs.push_back(toStdString(path));
        return true;
    }

    std::vector<PkString> fontDirectories() const override
    {
        std::vector<PkString> out;
        for (const std::string &d : m_dirs) {
            out.push_back(PkString(d.c_str()));
        }
        return out;
    }

    bool rebuildFontSet() override
    {
        ++m_rebuildCount;
        return true;
    }

    std::vector<FontEntry> sortedMatches(const PkFontQuery &query) const override
    {
        // 依次尝试 query.families 里的每个候选标识符,第一个在注册表里有
        // 记录的家族就是命中的家族——这就是"回退":调用方把通用家族排在
        // 候选列表后面,前面的具体家族找不到时自然落到它。
        const FakeFontRecord *matchedFamily = nullptr;
        std::string matchedFamilyName;
        for (const PkString &wanted : query.families) {
            const std::string wantedStd = toStdString(wanted);
            for (const FakeFontRecord &rec : m_records) {
                if (rec.family == wantedStd) {
                    matchedFamily = &rec;
                    matchedFamilyName = wantedStd;
                    break;
                }
            }
            if (matchedFamily) {
                break;
            }
        }
        if (!matchedFamily) {
            return {};
        }

        std::vector<const FakeFontRecord *> candidates;
        for (const FakeFontRecord &rec : m_records) {
            if (rec.family == matchedFamilyName) {
                candidates.push_back(&rec);
            }
        }

        if (query.charset != 0) {
            std::vector<const FakeFontRecord *> filtered;
            for (const FakeFontRecord *rec : candidates) {
                if (rec->charset.count(query.charset) > 0) {
                    filtered.push_back(rec);
                }
            }
            candidates = std::move(filtered);
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                          [&](const FakeFontRecord *a, const FakeFontRecord *b) {
                              return std::abs(a->weight - query.weight) < std::abs(b->weight - query.weight);
                          });

        std::vector<FontEntry> out;
        for (const FakeFontRecord *rec : candidates) {
            FontEntry entry;
            entry.handle = rec->handle;
            entry.familyName = PkString(rec->family.c_str());
            for (const std::string &lang : rec->languages) {
                entry.languages.push_back(PkString(lang.c_str()));
            }
            out.push_back(std::move(entry));
        }
        return out;
    }

    bool bestMatch(const PkFontQuery &query, FontEntry *outEntry) const override
    {
        std::vector<FontEntry> matches = sortedMatches(query);
        if (matches.empty()) {
            return false;
        }
        *outEntry = matches.front();
        return true;
    }

    std::vector<FontEntry> allFonts() const override
    {
        std::vector<FontEntry> out;
        for (const FakeFontRecord &rec : m_records) {
            FontEntry entry;
            entry.handle = rec.handle;
            entry.familyName = PkString(rec.family.c_str());
            for (const std::string &lang : rec.languages) {
                entry.languages.push_back(PkString(lang.c_str()));
            }
            out.push_back(std::move(entry));
        }
        return out;
    }

    bool coversCodepoint(const FontHandle &font, char32_t codepoint) const override
    {
        const std::string wantedPath = toStdString(font.filePath);
        for (const FakeFontRecord &rec : m_records) {
            if (toStdString(rec.handle.filePath) == wantedPath && rec.handle.faceIndex == font.faceIndex) {
                return rec.charset.count(codepoint) > 0;
            }
        }
        return false;
    }

    bool initialized() const { return m_initialized; }
    std::string configSearchPath() const { return m_configSearchPath; }
    const std::vector<std::string> &addedFiles() const { return m_addedFiles; }
    int rebuildCount() const { return m_rebuildCount; }

private:
    std::vector<FakeFontRecord> m_records;
    std::vector<std::string> m_dirs;
    std::vector<std::string> m_addedFiles;
    std::string m_configSearchPath;
    bool m_initialized = false;
    int m_rebuildCount = 0;
};

PkFontProvider::PkFontQuery familyQuery(std::initializer_list<const char *> families, int weight = 400,
                                          char32_t charset = 0)
{
    PkFontProvider::PkFontQuery q;
    for (const char *f : families) {
        q.families.push_back(PkString(f));
    }
    q.weight = weight;
    q.charset = charset;
    return q;
}

} // namespace

class PkFontProviderTestCase : public PkTestObject
{
public:
    // ① 按家族名匹配
    void testSortedMatchesByFamilyNameReturnsRegisteredFont();
    void testSortedMatchesReturnsEmptyWhenFamilyUnknown();

    // ② 匹配不到指定家族时的回退
    void testSortedMatchesFallsBackToLaterFamilyInList();
    void testSortedMatchesReturnsEmptyWhenNoFamilyMatchesAndNoFallbackRegistered();

    // 候选排序：按权重距离，不是注册顺序
    void testSortedMatchesOrdersCandidatesByWeightDistance();

    // ③ 字符集覆盖查询
    void testCoversCodepointTrueForRegisteredCharAndFalseForOther();
    void testSortedMatchesFiltersByRequestedCharsetInQuery();

    // bestMatch
    void testBestMatchReturnsFirstSortedCandidate();
    void testBestMatchReturnsFalseWhenNoMatch();

    // 枚举
    void testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont();

    // ②配置与路径
    void testInitializeRecordsConfigSearchPath();
    void testAddFontDirectoryReflectedInFontDirectories();
    void testRebuildFontSetReturnsTrueAndCanBeCalledRepeatedly();
};

void PkFontProviderTestCase::testSortedMatchesByFamilyNameReturnsRegisteredFont()
{
    FakeFontProvider provider;
    provider.registerFont("Arial", "/fonts/arial.ttf", 0, 400);

    std::vector<PkFontProvider::FontEntry> matches = provider.sortedMatches(familyQuery({"Arial"}));

    PK_COMPARE((int)matches.size(), 1);
    PK_COMPARE(matches[0].handle.filePath.PkToUtf8(), std::string("/fonts/arial.ttf"));
    PK_COMPARE(matches[0].handle.faceIndex, 0);
}

void PkFontProviderTestCase::testSortedMatchesReturnsEmptyWhenFamilyUnknown()
{
    FakeFontProvider provider;
    provider.registerFont("Arial", "/fonts/arial.ttf", 0, 400);

    std::vector<PkFontProvider::FontEntry> matches = provider.sortedMatches(familyQuery({"NoSuchFamily"}));
    PK_VERIFY(matches.empty());
}

void PkFontProviderTestCase::testSortedMatchesFallsBackToLaterFamilyInList()
{
    FakeFontProvider provider;
    provider.registerFont("sans-serif", "/fonts/fallback.ttf", 0, 400);

    // 调用方把找不到的具体家族排在前面,通用回退家族排在后面——这条路径
    // 对应真实实现里 FcPatternAddWeak(p, FC_FAMILY, "sans-serif", true) 追加
    // 的兜底候选(KoFontRegistry.cpp:372-378)。
    std::vector<PkFontProvider::FontEntry> matches =
        provider.sortedMatches(familyQuery({"RequestedButMissing", "sans-serif"}));

    PK_COMPARE((int)matches.size(), 1);
    PK_COMPARE(matches[0].handle.filePath.PkToUtf8(), std::string("/fonts/fallback.ttf"));
}

void PkFontProviderTestCase::testSortedMatchesReturnsEmptyWhenNoFamilyMatchesAndNoFallbackRegistered()
{
    FakeFontProvider provider;
    provider.registerFont("Arial", "/fonts/arial.ttf", 0, 400);

    std::vector<PkFontProvider::FontEntry> matches = provider.sortedMatches(familyQuery({"NoSuchFamily"}));
    PK_VERIFY(matches.empty());
}

void PkFontProviderTestCase::testSortedMatchesOrdersCandidatesByWeightDistance()
{
    FakeFontProvider provider;
    provider.registerFont("Sans", "/fonts/sans-400.ttf", 0, 400);
    provider.registerFont("Sans", "/fonts/sans-700.ttf", 0, 700);
    provider.registerFont("Sans", "/fonts/sans-300.ttf", 0, 300);

    std::vector<PkFontProvider::FontEntry> matches = provider.sortedMatches(familyQuery({"Sans"}, 650));

    PK_COMPARE((int)matches.size(), 3);
    // weight=650 离 700 最近(距离50) < 400(距离250) < 300(距离350)。
    PK_COMPARE(matches[0].handle.filePath.PkToUtf8(), std::string("/fonts/sans-700.ttf"));
    PK_COMPARE(matches[1].handle.filePath.PkToUtf8(), std::string("/fonts/sans-400.ttf"));
    PK_COMPARE(matches[2].handle.filePath.PkToUtf8(), std::string("/fonts/sans-300.ttf"));
}

void PkFontProviderTestCase::testCoversCodepointTrueForRegisteredCharAndFalseForOther()
{
    FakeFontProvider provider;
    provider.registerFont("Sans", "/fonts/sans.ttf", 0, 400, {}, {U'A', U'B'});

    PkFontProvider::FontHandle handle;
    handle.filePath = PkString("/fonts/sans.ttf");
    handle.faceIndex = 0;

    PK_VERIFY(provider.coversCodepoint(handle, U'A'));
    PK_VERIFY(!provider.coversCodepoint(handle, U'Z'));
}

void PkFontProviderTestCase::testSortedMatchesFiltersByRequestedCharsetInQuery()
{
    FakeFontProvider provider;
    // 两个字体同一家族、同权重,只有一个覆盖 U+4E2D("中")。对应
    // facesForCSSValues() 逐 grapheme 回退匹配时用 FcCharSetHasChar 从共享
    // 候选列表里挑出真正能显示该字符的字体(KoFontRegistry.cpp:474,490)。
    provider.registerFont("Sans", "/fonts/latin-only.ttf", 0, 400, {}, {U'A'});
    provider.registerFont("Sans", "/fonts/cjk.ttf", 0, 400, {}, {U'A', U'中'});

    std::vector<PkFontProvider::FontEntry> matches = provider.sortedMatches(familyQuery({"Sans"}, 400, U'中'));

    PK_COMPARE((int)matches.size(), 1);
    PK_COMPARE(matches[0].handle.filePath.PkToUtf8(), std::string("/fonts/cjk.ttf"));
}

void PkFontProviderTestCase::testBestMatchReturnsFirstSortedCandidate()
{
    FakeFontProvider provider;
    provider.registerFont("Sans", "/fonts/sans-400.ttf", 0, 400);
    provider.registerFont("Sans", "/fonts/sans-700.ttf", 0, 700);

    PkFontProvider::FontEntry entry;
    PK_VERIFY(provider.bestMatch(familyQuery({"Sans"}, 700), &entry));
    PK_COMPARE(entry.handle.filePath.PkToUtf8(), std::string("/fonts/sans-700.ttf"));
}

void PkFontProviderTestCase::testBestMatchReturnsFalseWhenNoMatch()
{
    FakeFontProvider provider;
    provider.registerFont("Arial", "/fonts/arial.ttf", 0, 400);

    PkFontProvider::FontEntry entry;
    PK_VERIFY(!provider.bestMatch(familyQuery({"NoSuchFamily"}), &entry));
}

void PkFontProviderTestCase::testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont()
{
    FakeFontProvider provider;
    provider.registerFont("Arial", "/fonts/arial.ttf", 0, 400, {"en", "de"});
    provider.registerFont("NotoSansCJK", "/fonts/noto.ttf", 3, 400, {"zh-Hans"});

    std::vector<PkFontProvider::FontEntry> all = provider.allFonts();
    PK_COMPARE((int)all.size(), 2);

    PK_COMPARE(all[0].familyName.PkToUtf8(), std::string("Arial"));
    PK_COMPARE((int)all[0].languages.size(), 2);
    PK_COMPARE(all[0].languages[0].PkToUtf8(), std::string("en"));
    PK_COMPARE(all[0].languages[1].PkToUtf8(), std::string("de"));

    PK_COMPARE(all[1].familyName.PkToUtf8(), std::string("NotoSansCJK"));
    PK_COMPARE(all[1].handle.faceIndex, 3);
    PK_COMPARE((int)all[1].languages.size(), 1);
    PK_COMPARE(all[1].languages[0].PkToUtf8(), std::string("zh-Hans"));
}

void PkFontProviderTestCase::testInitializeRecordsConfigSearchPath()
{
    FakeFontProvider provider;
    PK_VERIFY(!provider.initialized());
    PK_VERIFY(provider.initialize(PkString("/etc/fonts")));
    PK_VERIFY(provider.initialized());
    PK_COMPARE(provider.configSearchPath(), std::string("/etc/fonts"));
}

void PkFontProviderTestCase::testAddFontDirectoryReflectedInFontDirectories()
{
    FakeFontProvider provider;
    PK_VERIFY(provider.addFontDirectory(PkString("/data/fonts")));
    PK_VERIFY(provider.addFontFile(PkString("/data/fonts/extra.ttf")));

    std::vector<PkString> dirs = provider.fontDirectories();
    PK_COMPARE((int)dirs.size(), 1);
    PK_COMPARE(dirs[0].PkToUtf8(), std::string("/data/fonts"));
    PK_COMPARE((int)provider.addedFiles().size(), 1);
    PK_COMPARE(provider.addedFiles()[0], std::string("/data/fonts/extra.ttf"));
}

void PkFontProviderTestCase::testRebuildFontSetReturnsTrueAndCanBeCalledRepeatedly()
{
    FakeFontProvider provider;
    PK_VERIFY(provider.rebuildFontSet());
    PK_VERIFY(provider.rebuildFontSet());
    PK_COMPARE(provider.rebuildCount(), 2);
}

// PkTestBinder<PkFontProviderTestCase> 特化——手写,形状对照
// pk/test/pk_test_moc.py 的 emit_binder() 输出(同 test_stream.cpp/
// test_eventsink.cpp/test_resourcestorage.cpp 的做法)。
template <>
struct PkTestBinder<PkFontProviderTestCase> {
    static const char *className() { return "PkFontProviderTestCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testSortedMatchesByFamilyNameReturnsRegisteredFont",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testSortedMatchesByFamilyNameReturnsRegisteredFont();
             },
             nullptr},
            {"testSortedMatchesReturnsEmptyWhenFamilyUnknown",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testSortedMatchesReturnsEmptyWhenFamilyUnknown();
             },
             nullptr},
            {"testSortedMatchesFallsBackToLaterFamilyInList",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testSortedMatchesFallsBackToLaterFamilyInList();
             },
             nullptr},
            {"testSortedMatchesReturnsEmptyWhenNoFamilyMatchesAndNoFallbackRegistered",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)
                     ->testSortedMatchesReturnsEmptyWhenNoFamilyMatchesAndNoFallbackRegistered();
             },
             nullptr},
            {"testSortedMatchesOrdersCandidatesByWeightDistance",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testSortedMatchesOrdersCandidatesByWeightDistance();
             },
             nullptr},
            {"testCoversCodepointTrueForRegisteredCharAndFalseForOther",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testCoversCodepointTrueForRegisteredCharAndFalseForOther();
             },
             nullptr},
            {"testSortedMatchesFiltersByRequestedCharsetInQuery",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testSortedMatchesFiltersByRequestedCharsetInQuery();
             },
             nullptr},
            {"testBestMatchReturnsFirstSortedCandidate",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testBestMatchReturnsFirstSortedCandidate();
             },
             nullptr},
            {"testBestMatchReturnsFalseWhenNoMatch",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testBestMatchReturnsFalseWhenNoMatch();
             },
             nullptr},
            {"testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)
                     ->testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont();
             },
             nullptr},
            {"testInitializeRecordsConfigSearchPath",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testInitializeRecordsConfigSearchPath();
             },
             nullptr},
            {"testAddFontDirectoryReflectedInFontDirectories",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testAddFontDirectoryReflectedInFontDirectories();
             },
             nullptr},
            {"testRebuildFontSetReturnsTrueAndCanBeCalledRepeatedly",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testRebuildFontSetReturnsTrueAndCanBeCalledRepeatedly();
             },
             nullptr},
        };
        return fns;
    }
    static int count() { return 13; }

    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_fontprovider_tests(int argc, char **argv)
{
    PkFontProviderTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
