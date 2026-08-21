/*
 *  SPDX-FileCopyrightText: 2026 Krita developers
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTPROVIDERFONTCONFIG_H
#define KOFONTPROVIDERFONTCONFIG_H

#include <fontconfig/fontconfig.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "PkFontProvider.h"

/**
 * @brief KoFontProviderFontconfig
 * PkFontProvider 的 fontconfig 参考适配器（S-08 Task 5，R-09/R-12 交接落地）。
 *
 * 零 Qt / 零 FreeType：只实现「拿到字体」端口（发现 / 枚举 / 匹配 / 回退 /
 * coversCodepoint），返回 FontHandle（路径 + face index），不返回 FT_Face。
 * 度量（ascent/descent/glyph 尺寸/hinting）与排版一律不管——那是 FreeType/
 * 文字排版层的职责，按端口边界裁决不属于本类。
 *
 * 实现要点（对照 PkFontProvider.h 类头注释）：
 *  - 族④（FcPatternAdd 一族 / FcPatternGet 一族）整族归零，换成 PkFontQuery / FontEntry。
 *  - 族①（FcConfigDestroy 等 4 个）只作为本类自己的资源管理出现，不暴露给调用方。
 *  - 族③（FcStrList 遍历）折叠进 fontDirectories()/FontEntry::languages。
 *  - 契约 I-1：sortedMatches() 内部做「非缩放位图字体 + pixelSize 不符」过滤。
 *  - 契约 I-6：枚举/匹配路径只填 handle/familyName/languages；bestMatch 路径只填
 *    familyName/postScriptName/weight/width/slant。
 *  - coversCodepoint() 的 FcCharSet 来自内部缓存：allFonts()/sortedMatches() 构建
 *    FontEntry 时顺手把 FC_CHARSET 副本缓存下来；未命中时按 FC_FILE/FC_INDEX
 *    懒加载一次（FcFontMatch）。
 */
class KoFontProviderFontconfig : public PkFontProvider
{
public:
    KoFontProviderFontconfig();
    ~KoFontProviderFontconfig() override;

    // ── ②配置与路径 ──────────────────────────────────────────────
    bool initialize(const PkString &configSearchPath) override;
    bool addFontFile(const PkString &path) override;
    bool addFontDirectory(const PkString &path) override;
    std::vector<PkString> fontDirectories() const override;
    bool rebuildFontSet() override;

    // ── ⑤匹配 ────────────────────────────────────────────────────
    std::vector<FontEntry> sortedMatches(const PkFontQuery &query) const override;
    bool bestMatch(const PkFontQuery &query, FontEntry *outEntry) const override;

    // ── ⑥字体集枚举 ──────────────────────────────────────────────
    std::vector<FontEntry> allFonts() const override;

    // ── ⑦字符集 ──────────────────────────────────────────────────
    bool coversCodepoint(const FontHandle &font, char32_t codepoint) const override;

private:
    // FC_FILE + FC_INDEX → FcCharSet 缓存键。
    static std::string charSetKey(const FontHandle &handle);
    // 从 FcPattern 读 <FC_FILE, FC_INDEX>，读不到返回 false。
    static bool handleFromPattern(FcPattern *p, FontHandle *out);
    // 枚举/匹配路径的 FontEntry（I-6 分工表：handle/familyName/languages），并缓存 charset。
    std::optional<FontEntry> entryFromPatternForEnumeration(FcPattern *p) const;
    // bestMatch 路径的 FontEntry（I-6 分工表：familyName/postScriptName/weight/width/slant）。
    static FontEntry entryFromPatternForBestMatch(FcPattern *p);
    // PkFontQuery → FcPattern（FC_FAMILY/FC_WEIGHT/FC_SLANT/FC_WIDTH/FC_PIXEL_SIZE/FC_LANG）。
    static FcPattern *patternFromQuery(const PkFontQuery &query);
    // 取/查 FcCharSet：命中缓存直接返回；未命中按 handle 懒加载一次再缓存。
    FcCharSet *charSetForHandle(const FontHandle &handle) const;

    FcConfig *m_config = nullptr;
    bool m_ownsConfig = false; ///< 为 false 时 m_config 来自 FcConfigGetCurrent()，不销毁。

    mutable std::mutex m_mutex;
    mutable std::unordered_map<std::string, FcCharSet *> m_charSets; ///< key=charSetKey，值为 FcCharSetCopy 副本，析构销毁
};

#endif // KOFONTPROVIDERFONTCONFIG_H
