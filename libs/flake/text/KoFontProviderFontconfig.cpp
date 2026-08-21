/*
 *  SPDX-FileCopyrightText: 2026 Krita developers
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontProviderFontconfig.h"

#include <cstdlib>
#include <optional>

KoFontProviderFontconfig::KoFontProviderFontconfig() = default;

KoFontProviderFontconfig::~KoFontProviderFontconfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &pair : m_charSets) {
        FcCharSetDestroy(pair.second);
    }
    m_charSets.clear();
    if (m_ownsConfig && m_config) {
        FcConfigDestroy(m_config);
    }
}

// ── ②配置与路径 ──────────────────────────────────────────────────

bool KoFontProviderFontconfig::initialize(const PkString &configSearchPath)
{
    FcConfig *config = FcConfigCreate();
    if (!config) {
        return false;
    }

    // 原 KoFontRegistry::Private ctor（:124-136）：FONTCONFIG_PATH 未设时把探测到的
    // 配置根交给 fontconfig。configSearchPath 为空 = 无需覆盖（env 已设或调用方没给）。
    const std::string path = configSearchPath.PkToUtf8();
    if (!path.empty()) {
        const char *envPath = getenv("FONTCONFIG_PATH");
        if (!envPath || *envPath == '\0') {
            setenv("FONTCONFIG_PATH", path.c_str(), 1);
        }
    }

    if (!FcConfigParseAndLoad(config, nullptr, FcTrue)) {
        // 容错路径：解析失败退回当前全局配置（对应原 FcConfigGetCurrent 回退）。
        FcConfigDestroy(config);
        config = FcConfigGetCurrent();
        if (!config) {
            return false;
        }
        m_config = config;
        m_ownsConfig = false;
    } else {
        FcConfigSetCurrent(config);
        m_config = config;
        m_ownsConfig = true;
    }
    return true;
}

bool KoFontProviderFontconfig::addFontFile(const PkString &path)
{
    if (!m_config) {
        return false;
    }
    const std::string utf8 = path.PkToUtf8();
    return FcConfigAppFontAddFile(m_config, reinterpret_cast<const FcChar8 *>(utf8.c_str())) != FcFalse;
}

bool KoFontProviderFontconfig::addFontDirectory(const PkString &path)
{
    if (!m_config) {
        return false;
    }
    const std::string utf8 = path.PkToUtf8();
    return FcConfigAppFontAddDir(m_config, reinterpret_cast<const FcChar8 *>(utf8.c_str())) != FcFalse;
}

std::vector<PkString> KoFontProviderFontconfig::fontDirectories() const
{
    std::vector<PkString> result;
    if (!m_config) {
        return result;
    }
    FcStrList *list = FcConfigGetFontDirs(m_config);
    if (!list) {
        return result;
    }
    FcStrListFirst(list);
    for (FcChar8 *dir = FcStrListNext(list); dir; dir = FcStrListNext(list)) {
        result.emplace_back(reinterpret_cast<const char *>(dir));
    }
    FcStrListDone(list);
    return result;
}

bool KoFontProviderFontconfig::rebuildFontSet()
{
    if (!m_config) {
        return false;
    }
    return FcConfigBuildFonts(m_config) != FcFalse;
}

// ── ⑤匹配 ────────────────────────────────────────────────────────

std::vector<PkFontProvider::FontEntry> KoFontProviderFontconfig::sortedMatches(const PkFontQuery &query) const
{
    std::vector<FontEntry> result;
    if (!m_config) {
        return result;
    }

    FcPattern *pattern = patternFromQuery(query);
    if (!pattern) {
        return result;
    }
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult matchResult = FcResultNoMatch;
    FcCharSet *charSet = nullptr;
    FcFontSet *fontSet = FcFontSort(FcConfigGetCurrent(), pattern, FcFalse, &charSet, &matchResult);
    if (charSet) {
        FcCharSetDestroy(charSet);
    }
    FcPatternDestroy(pattern);
    if (!fontSet) {
        return result;
    }

    for (int j = 0; j < fontSet->nfont; j++) {
        // 契约 I-1（PkFontProvider.h）：非缩放位图字体、且请求了像素大小、且候选
        // 像素大小不符 → 剔除。与原始 facesForCSSValues 的 FC_SCALABLE/FC_PIXEL_SIZE
        // 判定一致。
        FcBool isScalable = FcTrue;
        if (FcPatternGetBool(fontSet->fonts[j], FC_SCALABLE, 0, &isScalable) != FcResultMatch) {
            isScalable = FcTrue;
        }
        double fontPixelSize = 0.0;
        if (FcPatternGetDouble(fontSet->fonts[j], FC_PIXEL_SIZE, 0, &fontPixelSize) != FcResultMatch) {
            fontPixelSize = 0.0;
        }
        if (query.pixelSize >= 0 && !isScalable && fontPixelSize != query.pixelSize) {
            continue;
        }

        if (std::optional<FontEntry> entry = entryFromPatternForEnumeration(fontSet->fonts[j])) {
            result.push_back(std::move(*entry));
        }
    }
    FcFontSetDestroy(fontSet);
    return result;
}

bool KoFontProviderFontconfig::bestMatch(const PkFontQuery &query, FontEntry *outEntry) const
{
    if (!outEntry || query.families.empty()) {
        return false;
    }

    // 语义（PkFontProvider.h）：这条路径按 PostScript 名精确匹配，families[0] 承载
    // PostScript 名（对应原 getCssDataForPostScriptName 的 FC_POSTSCRIPT_NAME）。
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        return false;
    }
    const std::string postScript = query.families.front().PkToUtf8();
    FcPatternAddString(pattern, FC_POSTSCRIPT_NAME, reinterpret_cast<const FcChar8 *>(postScript.c_str()));
    FcDefaultSubstitute(pattern);

    FcResult result = FcResultNoMatch;
    FcPattern *match = FcFontMatch(FcConfigGetCurrent(), pattern, &result);
    FcPatternDestroy(pattern);
    if (!match || result == FcResultNoMatch) {
        if (match) {
            FcPatternDestroy(match);
        }
        return false;
    }
    *outEntry = entryFromPatternForBestMatch(match);
    FcPatternDestroy(match);
    return true;
}

// ── ⑥字体集枚举 ──────────────────────────────────────────────────

std::vector<PkFontProvider::FontEntry> KoFontProviderFontconfig::allFonts() const
{
    std::vector<FontEntry> result;
    if (!m_config) {
        return result;
    }

    // 对应原 reloadConverter 的 FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX,
    // FC_LANG, FC_CHARSET) + FcFontList。
    FcObjectSet *objectSet = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, FC_LANG, FC_CHARSET, nullptr);
    if (!objectSet) {
        return result;
    }
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        FcObjectSetDestroy(objectSet);
        return result;
    }
    FcFontSet *fontSet = FcFontList(m_config, pattern, objectSet);
    FcPatternDestroy(pattern);
    FcObjectSetDestroy(objectSet);
    if (!fontSet) {
        return result;
    }

    for (int j = 0; j < fontSet->nfont; j++) {
        if (std::optional<FontEntry> entry = entryFromPatternForEnumeration(fontSet->fonts[j])) {
            result.push_back(std::move(*entry));
        }
    }
    FcFontSetDestroy(fontSet);
    return result;
}

// ── ⑦字符集 ──────────────────────────────────────────────────────

bool KoFontProviderFontconfig::coversCodepoint(const FontHandle &font, char32_t codepoint) const
{
    FcCharSet *charset = charSetForHandle(font);
    if (!charset) {
        return false;
    }
    return FcCharSetHasChar(charset, codepoint) != FcFalse;
}

// ── private helpers ───────────────────────────────────────────────

std::string KoFontProviderFontconfig::charSetKey(const FontHandle &handle)
{
    std::string key = handle.filePath.PkToUtf8();
    key += '#';
    key += std::to_string(handle.faceIndex);
    return key;
}

bool KoFontProviderFontconfig::handleFromPattern(FcPattern *pattern, FontHandle *out)
{
    FcChar8 *fileValue = nullptr;
    if (FcPatternGetString(pattern, FC_FILE, 0, &fileValue) != FcResultMatch) {
        return false;
    }
    int indexValue = 0;
    if (FcPatternGetInteger(pattern, FC_INDEX, 0, &indexValue) != FcResultMatch) {
        return false;
    }
    out->filePath = PkString(reinterpret_cast<const char *>(fileValue));
    out->faceIndex = indexValue;
    return true;
}

std::optional<PkFontProvider::FontEntry> KoFontProviderFontconfig::entryFromPatternForEnumeration(FcPattern *pattern) const
{
    FontEntry entry;
    if (!handleFromPattern(pattern, &entry.handle)) {
        return std::nullopt;
    }

    FcChar8 *familyValue = nullptr;
    if (FcPatternGetString(pattern, FC_FAMILY, 0, &familyValue) == FcResultMatch) {
        entry.familyName = PkString(reinterpret_cast<const char *>(familyValue));
    }

    // 族③ FcStrList 折叠：FC_LANG 的 FcLangSet → languages 列表。
    FcLangSet *langSet = nullptr;
    if (FcPatternGetLangSet(pattern, FC_LANG, 0, &langSet) == FcResultMatch && langSet) {
        if (FcStrList *langList = FcStrListCreate(FcLangSetGetLangs(langSet))) {
            FcStrListFirst(langList);
            for (FcChar8 *lang = FcStrListNext(langList); lang; lang = FcStrListNext(langList)) {
                entry.languages.emplace_back(reinterpret_cast<const char *>(lang));
            }
            FcStrListDone(langList);
        }
    }

    // 顺手把 FC_CHARSET 副本缓存下来，供 coversCodepoint 用（避免每次都 FcFontMatch）。
    FcCharSet *charset = nullptr;
    if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &charset) == FcResultMatch && charset) {
        const std::string key = charSetKey(entry.handle);
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_charSets.find(key) == m_charSets.end()) {
            m_charSets.emplace(key, FcCharSetCopy(charset));
        }
    }
    return entry;
}

PkFontProvider::FontEntry KoFontProviderFontconfig::entryFromPatternForBestMatch(FcPattern *pattern)
{
    FontEntry entry;
    FcChar8 *value = nullptr;
    if (FcPatternGetString(pattern, FC_FAMILY, 0, &value) == FcResultMatch) {
        entry.familyName = PkString(reinterpret_cast<const char *>(value));
    }
    if (FcPatternGetString(pattern, FC_POSTSCRIPT_NAME, 0, &value) == FcResultMatch) {
        entry.postScriptName = PkString(reinterpret_cast<const char *>(value));
    }
    int intValue = 0;
    if (FcPatternGetInteger(pattern, FC_WEIGHT, 0, &intValue) == FcResultMatch) {
        entry.weight = FcWeightToOpenType(intValue);
    }
    if (FcPatternGetInteger(pattern, FC_WIDTH, 0, &intValue) == FcResultMatch) {
        entry.width = intValue;
    }
    if (FcPatternGetInteger(pattern, FC_SLANT, 0, &intValue) == FcResultMatch) {
        if (intValue == FC_SLANT_ITALIC) {
            entry.slant = Slant::Italic;
        } else if (intValue == FC_SLANT_OBLIQUE) {
            entry.slant = Slant::Oblique;
        } else {
            entry.slant = Slant::Normal;
        }
    }
    return entry;
}

FcPattern *KoFontProviderFontconfig::patternFromQuery(const PkFontQuery &query)
{
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        return nullptr;
    }

    // 族④归零：原 facesForCSSValues 的 FcPatternAdd* 逐字段换成 PkFontQuery 字段。
    // families 是优先级列表，全部加进 FC_FAMILY 让 fontconfig 原生多族匹配（与原始
    // 多值 FC_FAMILY 行为一致；原 FcPatternAddWeak(FC_FAMILY, "sans-serif") 的回退
    // 语义 = 调用方把回退族放在列表末尾）。
    for (const PkString &family : query.families) {
        const std::string familyUtf8 = family.PkToUtf8();
        FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(familyUtf8.c_str()));
    }

    // OpenType weight → fontconfig weight（FcWeightFromOpenType 是 fontconfig 的
    // 纯换算，适配器实现内部可以用，不暴露给调用方）。
    FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(query.weight));

    if (query.slant == Slant::Italic) {
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    } else if (query.slant == Slant::Oblique) {
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_OBLIQUE);
    } else {
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
    }

    FcPatternAddInteger(pattern, FC_WIDTH, query.width);
    if (query.pixelSize >= 0) {
        FcPatternAddDouble(pattern, FC_PIXEL_SIZE, query.pixelSize);
    }
    if (!query.lang.isEmpty()) {
        const std::string langUtf8 = query.lang.PkToUtf8();
        FcPatternAddString(pattern, FC_LANG, reinterpret_cast<const FcChar8 *>(langUtf8.c_str()));
    }
    return pattern;
}

FcCharSet *KoFontProviderFontconfig::charSetForHandle(const FontHandle &handle) const
{
    const std::string key = charSetKey(handle);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_charSets.find(key);
        if (it != m_charSets.end()) {
            return it->second;
        }
    }

    // 未命中：按 FC_FILE/FC_INDEX 查一次再缓存（懒加载）。
    if (!m_config) {
        return nullptr;
    }
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        return nullptr;
    }
    const std::string file = handle.filePath.PkToUtf8();
    FcPatternAddString(pattern, FC_FILE, reinterpret_cast<const FcChar8 *>(file.c_str()));
    FcPatternAddInteger(pattern, FC_INDEX, handle.faceIndex);
    FcResult matchResult = FcResultNoMatch;
    FcPattern *match = FcFontMatch(FcConfigGetCurrent(), pattern, &matchResult);
    FcPatternDestroy(pattern);
    if (!match || matchResult == FcResultNoMatch) {
        if (match) {
            FcPatternDestroy(match);
        }
        return nullptr;
    }
    FcCharSet *charset = nullptr;
    if (FcPatternGetCharSet(match, FC_CHARSET, 0, &charset) == FcResultMatch && charset) {
        FcCharSet *copy = FcCharSetCopy(charset);
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto inserted = m_charSets.emplace(key, copy);
        if (!inserted.second) {
            // 并发下别的线程先插了同样的 key：丢弃本次副本，返回缓存里的那个。
            FcCharSetDestroy(copy);
            FcPatternDestroy(match);
            return inserted.first->second;
        }
        FcPatternDestroy(match);
        return copy;
    }
    FcPatternDestroy(match);
    return nullptr;
}
