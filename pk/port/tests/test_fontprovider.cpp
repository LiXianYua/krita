// PkFontProvider 的测试。形态照抄 test_resourcestorage.cpp：用 R-11 的 PK_*
// 宏 + 真实的 PkTest::qExec 派发，PkTestBinder<T> 特化手写，不经
// pk_test_moc.py。
//
// FakeFontProvider 是内存里的假字体提供者：注册表按家族名分组，每条记录带
// 权重/语言/字符集覆盖（用一个码点集合表示）/PostScript 名/宽度/倾斜/
// 缩放性/固定像素大小。断言覆盖任务要求的三类行为：① 按家族名匹配、
// ② 匹配不到指定家族时落到候选列表里的通用回退家族、③ coversCodepoint()
// 字符集覆盖查询——外加候选排序（按权重距离，不是"谁先注册谁在前"这种会被
// 偷懒实现蒙混过去的顺序）、sortedMatches() 对非缩放位图字体的强制过滤
// 契约（评审 I-1）、bestMatch() 的真/假两支及其专属描述字段（评审 C-2：
// postScriptName/weight/width/slant）、allFonts 枚举、②配置与路径族的
// 几个方法。全部断言基于具体输入输出比对，不写恒真断言。
//
// 评审 I-5：PkFontQuery 曾经有一个 charset 字段（"query.charset 过滤"），
// 零调用点已删——本文件不再测这个字段，coversCodepoint() 仍然是逐码点
// 覆盖查询的唯一入口。
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

    // 评审 C-2：bestMatch() 路径需要的描述属性。
    std::string postScriptName;
    int width = 100;
    PkFontProvider::Slant slant = PkFontProvider::Slant::Normal;

    // 评审 I-1：sortedMatches() 的强制过滤契约需要的字段——非缩放位图字体
    // 且 pixelSize 与请求值不等时必须被排除在结果之外。
    bool scalable = true;
    double pixelSize = -1.0;
};

// 内存里的假字体提供者。sortedMatches() 的匹配/回退/排序/charset 过滤逻辑
// 是测试要验证的核心行为，全部手写、不调用真实 fontconfig。
class FakeFontProvider : public PkFontProvider
{
public:
    void registerFont(const std::string &family, const std::string &filePath, int faceIndex,
                       int weight, std::vector<std::string> languages = {},
                       std::set<char32_t> charset = {}, std::string postScriptName = "",
                       int width = 100, PkFontProvider::Slant slant = PkFontProvider::Slant::Normal,
                       bool scalable = true, double pixelSize = -1.0)
    {
        FakeFontRecord rec;
        rec.family = family;
        rec.handle.filePath = PkString(filePath.c_str());
        rec.handle.faceIndex = faceIndex;
        rec.weight = weight;
        rec.languages = std::move(languages);
        rec.charset = std::move(charset);
        rec.postScriptName = std::move(postScriptName);
        rec.width = width;
        rec.slant = slant;
        rec.scalable = scalable;
        rec.pixelSize = pixelSize;
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

        // 评审 I-1：sortedMatches() 的强制过滤契约——非缩放位图字体且请求了
        // 具体 pixelSize、候选自身像素大小又与请求值不等时，必须在返回前
        // 排除，不能留给调用方再过滤一遍。
        if (query.pixelSize >= 0) {
            std::vector<const FakeFontRecord *> filtered;
            for (const FakeFontRecord *rec : candidates) {
                if (!rec->scalable && rec->pixelSize != query.pixelSize) {
                    continue;
                }
                filtered.push_back(rec);
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
            // 评审 C-2：bestMatch() 路径需要的描述属性，sortedMatches()/
            // allFonts() 路径的消费者不读这几个字段，但既然有值就一并带上
            // ——两条路径共用同一个结构体本来就是本任务的既有设计。
            entry.postScriptName = PkString(rec->postScriptName.c_str());
            entry.weight = rec->weight;
            entry.width = rec->width;
            entry.slant = rec->slant;
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
                                          double pixelSize = -1.0)
{
    PkFontProvider::PkFontQuery q;
    for (const char *f : families) {
        q.families.push_back(PkString(f));
    }
    q.weight = weight;
    q.pixelSize = pixelSize;
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

    // 评审 I-1：sortedMatches() 强制过滤契约——非缩放位图字体且请求了具体
    // pixelSize、候选自身像素大小与请求值不等时必须被排除。
    void testSortedMatchesFiltersNonScalableBitmapFontsWithMismatchedPixelSize();

    // bestMatch
    void testBestMatchReturnsFirstSortedCandidate();
    void testBestMatchReturnsFalseWhenNoMatch();

    // 评审 C-2：bestMatch() 唯一调用点（getCssDataForPostScriptName()）要读
    // 的 5 个字段里，familyName 之外的 postScriptName/weight/width/slant
    // 必须能从结果里拿到。
    void testBestMatchReturnsPostScriptNameWeightWidthSlant();

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

// 评审 I-1：KoFontRegistry.cpp:465-470 用 FC_SCALABLE/FC_PIXEL_SIZE 跳过
// "非缩放且 pixelSize 不等于请求值"的位图字体——这是 sortedMatches() 的强制
// 过滤契约，不是可选优化。两个同家族候选：一个可缩放（矢量字体，任何请求
// 像素大小都该留下）、一个不可缩放且固定 12px（位图字体，只有请求正好 12px
// 才该留下）。
void PkFontProviderTestCase::testSortedMatchesFiltersNonScalableBitmapFontsWithMismatchedPixelSize()
{
    FakeFontProvider provider;
    provider.registerFont("Sans", "/fonts/sans-scalable.ttf", 0, 400, {}, {},
                           /*postScriptName=*/"", /*width=*/100, PkFontProvider::Slant::Normal,
                           /*scalable=*/true, /*pixelSize=*/-1.0);
    provider.registerFont("Sans", "/fonts/sans-bitmap-12px.bdf", 0, 400, {}, {},
                           /*postScriptName=*/"", /*width=*/100, PkFontProvider::Slant::Normal,
                           /*scalable=*/false, /*pixelSize=*/12.0);

    // 请求 16px：位图字体（固定 12px）不满足，矢量字体不受影响,必须被排除。
    std::vector<PkFontProvider::FontEntry> matches16 = provider.sortedMatches(familyQuery({"Sans"}, 400, 16.0));
    PK_COMPARE((int)matches16.size(), 1);
    PK_COMPARE(matches16[0].handle.filePath.PkToUtf8(), std::string("/fonts/sans-scalable.ttf"));

    // 请求 12px：位图字体正好匹配，两个候选都该留下。
    std::vector<PkFontProvider::FontEntry> matches12 = provider.sortedMatches(familyQuery({"Sans"}, 400, 12.0));
    PK_COMPARE((int)matches12.size(), 2);
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

// 评审 C-2：bestMatch() 唯一调用点 getCssDataForPostScriptName()
// （KoFontRegistry.cpp:1197-1234）从匹配结果读 FC_FAMILY/FC_POSTSCRIPT_NAME/
// FC_WEIGHT/FC_WIDTH/FC_SLANT，且从不读 FC_FILE/FC_INDEX——FontEntry 此前
// 只有 familyName + handle，5 个要读的字段缺 4 个。
void PkFontProviderTestCase::testBestMatchReturnsPostScriptNameWeightWidthSlant()
{
    FakeFontProvider provider;
    provider.registerFont("Helvetica Neue", "/fonts/helv-bold-italic.ttf", 0, 700, {}, {},
                           /*postScriptName=*/"HelveticaNeue-BoldItalic", /*width=*/75,
                           PkFontProvider::Slant::Italic);

    PkFontProvider::FontEntry entry;
    PK_VERIFY(provider.bestMatch(familyQuery({"Helvetica Neue"}), &entry));
    PK_COMPARE(entry.familyName.PkToUtf8(), std::string("Helvetica Neue"));
    PK_COMPARE(entry.postScriptName.PkToUtf8(), std::string("HelveticaNeue-BoldItalic"));
    PK_COMPARE(entry.weight, 700);
    PK_COMPARE(entry.width, 75);
    PK_VERIFY(entry.slant == PkFontProvider::Slant::Italic);
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

    // 评审 I-4：FakeFontProvider::allFonts()（同真实实现按 §1.5 分工表的约定）
    // 不填 weight/width/slant——这几个字段必须落回越界哨兵，而不是「像真的」
    // 的 400/100/Normal。这条断言就是锁「未填」与「读到的确实是默认值」在
    // 类型层面可区分：改回旧的「看起来合法」默认值，这里就会变绿变红。
    PK_COMPARE(all[0].weight, -1);
    PK_COMPARE(all[0].width, -1);
    PK_VERIFY(all[0].slant == PkFontProvider::Slant::Unknown);
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
            {"testSortedMatchesFiltersNonScalableBitmapFontsWithMismatchedPixelSize",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)
                     ->testSortedMatchesFiltersNonScalableBitmapFontsWithMismatchedPixelSize();
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
            {"testBestMatchReturnsPostScriptNameWeightWidthSlant",
             [](PkTestObject *o) {
                 static_cast<PkFontProviderTestCase *>(o)->testBestMatchReturnsPostScriptNameWeightWidthSlant();
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
    static int count() { return 14; }

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
