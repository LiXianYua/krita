#pragma once

#include <vector>

// PkString 归 R-01（pk/string），已交付、零 Qt 依赖、纯 C++17。与
// PkStream.h/PkResourceStorage.h 只前置声明不同：本头文件里 PkFontQuery/
// FontHandle/FontEntry 三个结构体都拿 PkString 当**值成员**（不是函数参数/
// 返回值），值成员要求编译期知道完整大小，前置声明做不到,所以这里直接
// include——PkString.h 本身零 Qt/零 fontconfig/零 FreeType 依赖,不违反本
// 任务「端口头不得 include Krita/fontconfig/FreeType 头」的硬约束。
#include "PkString.h"

// PkFontProvider —— 零 Qt / 零 fontconfig / 零 FreeType 依赖的「拿到字体」端口。
//
// ────────────────────────────────────────────────────────────────────────
// 端口边界（已裁决，不要扩）
// ────────────────────────────────────────────────────────────────────────
// 管：发现（配置与已知字体目录）/ 枚举（系统全部字体）/ 匹配（按 PkFontQuery
//     排序候选、取最佳单个）/ 回退（把候选家族链里的通用家族——如
//     "sans-serif"——放进同一条 PkFontQuery.families 里再查一次,不是独立方法）。
//
// **不管度量**（ascent/descent/glyph 尺寸/hinting 等 FreeType 概念一概不在
// 这个类里）。理由：端口判据是"这个能力在不同平台/场景下有没有不同实现"，
// 而度量走 FreeType，四个目标平台（fontconfig 桌面 / Android `fonts.xml` 或
// NDK `AFont` / DirectWrite / CoreText）拿到字体文件之后走的是**同一份**
// FreeType 调用完成度量——按判据它不该在端口里。度量在"拿到字体"之后进行，
// 不经过这个端口。**这不是遗漏，是裁决，后面谁想给这个类"补全"度量方法，
// 先看这段。**
//
// ────────────────────────────────────────────────────────────────────────
// 端口不返回 FT_Face
// ────────────────────────────────────────────────────────────────────────
// 返回的是字体文件标识（FontHandle：路径 + face index，ttc/otc 合集字体的
// 子字体序号）。FreeType 打开（FT_New_Face）在端口之外，由调用方
// （KoFontRegistry 的 Pk 化版本）拿到 FontHandle 后自己做——这样四个平台
// 实现都能满足同一个端口形状。
//
// 评审 M-5：**不是所有 KoFontRegistry 侧调用者都不改一行**——
// `KoFFWWSConverter::addFontFromPattern`（`KoFFWWSConverter.cpp:331-337`）
// 要把整个 `FcCharSet` 存下来喂 `addSupportedLanguagesByFile()`，本端口只
// 给了逐码点的 `coversCodepoint()`（族⑦，见 `sortedMatches()`/
// `coversCodepoint()` 注释），没有"整个字符集"这个粒度——这一处调用方必须
// 重构（要么改成逐码点查询驱动 `addSupportedLanguagesByFile()`，要么改
// `addSupportedLanguagesByFile()` 的输入形状去接受一个能逐码点查询的谓词，
// 具体做法留给消费本端口的批次决定）。"调用者一行不改"只对走
// `FT_New_Face(FontHandle)` 这条主干路径的调用点成立。
//
// ────────────────────────────────────────────────────────────────────────
// 范围口径（实测，见 .superpowers/sdd/R-12/task-5-report.md 的逐处核验）
// ────────────────────────────────────────────────────────────────────────
// 素材：libs/flake/text/KoFontRegistry.cpp（124 处 Fc* 调用）+
// KoFFWWSConverter.cpp（22 处）+ KoFontLibraryResourceUtils.h（12 处，全部是
// RAII 模板的 destroy 函数指针实参）+ KoFFWWSConverter.h（2 处，类型别名）。
//
// **不是 1:1 映射 fontconfig 的全部函数**。覆盖的三族——
//   ②配置与路径：FcConfigCreate/FcConfigParseAndLoad/FcConfigSetCurrent/
//     FcConfigGetCurrent/FcConfigAppFontAddDir/FcConfigAppFontAddFile/
//     FcConfigGetFontDirs/FcConfigBuildFonts（8 个不同函数，12 处调用）
//   ⑤匹配：FcConfigSubstitute/FcDefaultSubstitute/FcFontSort/FcFontMatch
//     （4 个不同函数，8 处调用）
//   ⑥字体集枚举：FcObjectSetBuild/FcFontList（2 个不同函数，2 处调用）
// 外加两个不在上面三族计数里、但确有真实调用点、没有材料证明可以砍掉的：
//   ⑦字符集：FcCharSetHasChar（facesForCSSValues() 逐字符回退匹配用）
//   ⑧语言集：FcPatternGetLangSet + FcLangSetGetLangs（WWS 家族归并用）
//
// **明确排除，登记不是遗漏**：
//   ①初始化/生命周期：FcConfigDestroy/FcPatternDestroy/FcFontSetDestroy/
//     FcCharSetDestroy 这 4 个——全仓零直接调用，只作为 RAII 模板的 destroy
//     函数指针实参出现（KoFontLibraryResourceUtils.h:89-92）。具体实现的资源
//     生命周期是它自己的实现细节，端口不需要暴露"销毁"方法。
//   ③目录列表迭代 FcStrList（FcStrListFirst/Next/Done/Create，10 处）：唯一
//     用途是遍历 FcConfigGetFontDirs() 的结果（KoFontRegistry.cpp:150-162）
//     和 FcPatternGetLangSet() 的结果（KoFFWWSConverter.cpp:320-323）——两处
//     都被折叠进对应的端口方法直接返回 vector，不单独暴露一个 Fc 风格的链表
//     迭代器类型。
//   ④模式构造与属性读写（FcPatternCreate/AddString/AddInteger/AddDouble/
//     AddWeak/GetString/GetInteger/GetDouble/GetBool/GetCharSet/Hash，32 处，
//     占比最大的一族）：**整族归零**，换成 PkFontQuery（7 个固定属性——
//     评审 I-5 删掉了零调用点的 charset 字段后从 8 个收窄为 7 个）。
//   FcFontRenderPrepare：零命中，不复刻 pattern-merge 语义——sortedMatches()
//     返回的每个 FontEntry 就是候选本身，不做"与最终 pattern 合并"这一步。
//   FcWeightToOpenType/FcWeightFromOpenType（KoFontRegistry.cpp:395,1222）：
//     纯数学分段线性插值，无 I/O 无状态，不是端口能力——调用方需要时自己
//     内联实现，不进这个类。
//   FcPatternHash（KoFontRegistry.cpp:406,422）：KoFontRegistry 自己拿它做
//     候选结果的缓存键，是调用方的实现细节，不是"拿到字体"本身需要的能力。
//
// **「按家族名查找」是必需路径，不是可选功能**：.kra 的文字图层只记 CSS
// font-family 家族名字符串，全仓无任何字体内嵌机制——所以按名匹配 + 回退
// 必须在端口里，不能砍。
class PkFontProvider
{
public:
    // 替代 libs/flake/text/KoFontRegistry.h:98
    // `static QFont::Style slantMode(FT_FaceSP)` 的返回类型——原方法只为了
    // 返回 normal/italic/oblique 三态，就把整个 QtGui 拖进了公开接口。
    // 来源：FC_SLANT 的三个取值 FC_SLANT_ROMAN/ITALIC/OBLIQUE
    // （KoFontRegistry.cpp:388-393 facesForCSSValues()，:1227-1228
    // getCssDataForPostScriptName()）。
    // 评审 I-4：Unknown 只用在 FontEntry::slant 的默认值（结果侧「未填」的
    // 哨兵）——PkFontQuery::slant（查询侧）永远不该是 Unknown，调用方总要
    // 明确指定查询条件，默认仍是 Normal。
    enum class Slant { Normal, Italic, Oblique, Unknown };

    // 一次「拿到字体」查询的条件。族④（32 处 FcPatternAdd*/FcPatternGet*）
    // 归零后的替代品——**七个固定属性，不做 1:1 映射**（评审 I-5：原先的
    // charset 字段零调用点，已删，从八个收窄为七个）。
    struct PkFontQuery
    {
        // 优先级排序的候选标识符列表：通常是家族名，第一顺位没找到就试下一个
        // ——调用方把通用回退家族（如 "sans-serif"）也放进这个列表里，「回退」
        // 因此不是独立的一个方法，是同一条 sortedMatches()/bestMatch() 路径
        // 用一个更宽的 query 再查一次。
        // 来源：FC_FAMILY，KoFontRegistry.cpp:366-370（每个候选家族一次
        // FcPatternAddString）+ :372-378（FcPatternAddWeak 追加的
        // "sans-serif" 回退家族，同款用法见 :213-218 fallbackFont()）。
        //
        // 唯一例外：getCssDataForPostScriptName()（KoFontRegistry.cpp:1197-
        // 1234）用的是 FC_POSTSCRIPT_NAME 而不是 FC_FAMILY——PostScript 名是
        // 精确标识符，语义上和"家族名模糊匹配"不是一回事。查询侧 7 字段
        // 预算里没有单独的 postscript 字段，这里的处理是把它也塞进
        // families[0]：
        // families 在本结构体里统一表示"要精确/模糊匹配的标识符"，具体查的
        // 是 FC_FAMILY 还是 FC_POSTSCRIPT_NAME 由实现按调用形态决定（比如只
        // 有一个候选、且调用方经由 bestMatch() 而不是 sortedMatches() 时，
        // 实现可以选择按 postscript name 精确匹配）。**这是本任务查询侧 7
        // 字段预算内的收窄，不是遗漏**——已在任务报告里标注为判断，不是
        // 实测。结果侧不受此收窄影响：FontEntry::postScriptName（评审 C-2）
        // 是独立字段，bestMatch() 命中后原样把匹配到的 PostScript 名还给
        // 调用方。
        std::vector<PkString> families;

        // OpenType weight（1–1000）。来源：FC_WEIGHT，KoFontRegistry.cpp:395
        // （FcPatternAddInteger + FcWeightFromOpenType 换算，换算本身不进
        // 端口，调用方传入换算后的 OpenType 值）。
        int weight = 400;

        // 来源：FC_SLANT，KoFontRegistry.cpp:388-393。
        Slant slant = Slant::Normal;

        // OpenType width（100 = normal）。来源：FC_WIDTH，
        // KoFontRegistry.cpp:396。
        int width = 100;

        // 点数（<0 表示未指定）。来源：KoCSSFontInfo::size——调用方在换算出
        // pixelSize 之前的原始点数，端口本身不用它做任何计算，只是让调用方
        // 不必自己另开一个字段保留原始输入。
        double size = -1.0;

        // 像素大小（<0 表示未指定）。来源：FC_PIXEL_SIZE，
        // KoFontRegistry.cpp:398-399
        // `pixelSize = info.size * (qMin(xRes, yRes) / 72.0)`——DPI 换算在
        // 端口之外，调用方传入换算结果。
        double pixelSize = -1.0;

        // 语言标签提示（空串表示不限制）。来源：facesForCSSValues() 的
        // language 形参（KoFontRegistry.h:55/107），配合族⑧
        // FcPatternGetLangSet/FcLangSetGetLangs 对候选语言集的读取。
        //
        // 评审 I-5：**仅用于候选排序，实现不得据此过滤**——不能写成"过滤
        // 或排序二选一由实现决定"这种把行为选择权交给实现的措辞（端口契约
        // 里不允许"实现可以/应该"这类模糊表述，调用方无法依赖任何一种）。
        // 排序：同语言的候选排前面；过滤：绝对不行，因为一个字体完全可能
        // 不带 lang 提示对应的语言标签、却仍然是该语言的正确候选（fontconfig
        // 的语言标签本身就是不完整的元数据，不是"这个字体能不能显示这个
        // 语言"的可靠判据）。
        PkString lang;
    };

    // 字体文件标识：路径 + face index（ttc/otc 合集字体的子字体序号）。
    // 端口不返回 FT_Face，理由见类头注释。来源：FC_FILE/FC_INDEX，
    // KoFontRegistry.cpp:52-68 本地函数 getFontFileEntry()（把 FcPattern 读
    // 成 <fileName, fontIndex> 对，逐个匹配/枚举调用点都要这一步）+
    // KoFFWWSConverter::FontFileEntry（既有先例，同形状）。
    struct FontHandle
    {
        PkString filePath;
        int faceIndex = 0;
    };

    // 一条匹配/枚举结果：字体文件标识 + 供调用方做 WWS 家族归并或按
    // PostScript 名精确匹配用的描述属性。**归并逻辑本身不在本端口范围内**
    // （那是 KoFFWWSConverter 的事，R-12 只出接口），这里只出要读的原始
    // 字段。
    //
    // 评审 I-6：这个结构体被 sortedMatches()/allFonts() 与 bestMatch() 两条
    // 路径共用，但两条路径实际读的字段不是同一组——**哪些字段在哪条路径上
    // 有意义，钉死在这里，不写成"由具体实现决定"**：
    //   - sortedMatches()/allFonts() 路径（WWS 家族归并，
    //     KoFFWWSConverter::addFontFromPattern()）：读 handle、familyName、
    //     languages。不读 postScriptName/weight/width/slant——归并只关心
    //     "这个文件属于哪个家族、支持哪些语言"。
    //   - bestMatch() 路径（getCssDataForPostScriptName()，
    //     KoFontRegistry.cpp:1197-1234）：读 familyName、postScriptName、
    //     weight、width、slant。**不读 handle（FC_FILE/FC_INDEX 从不被这个
    //     调用点使用）、不读 languages**——这条路径要的是 CSS 层面的描述
    //     属性，不是文件位置。
    struct FontEntry
    {
        FontHandle handle;

        // 来源：FC_FAMILY。KoFontRegistry.cpp:252-257 reloadConverter()：
        // FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, FC_LANG, FC_CHARSET,
        // …) 喂给 FcFontList()，结果逐个传给
        // KoFFWWSConverter::addFontFromPattern() 做归并。
        PkString familyName;

        // 该字体支持的语言标签列表。来源：FC_LANG →
        // FcPatternGetLangSet + FcLangSetGetLangs（族⑧），
        // KoFFWWSConverter.cpp:315-323 addFontFromPattern()。
        std::vector<PkString> languages;

        // ── 评审 C-2：bestMatch() 路径专用的描述属性 ──────────────────
        // getCssDataForPostScriptName()（KoFontRegistry.cpp:1197-1234，经
        // libs/psdutils/cos/psd_text_data_converter.cpp:858 可达）从匹配
        // 结果读的是 FC_FAMILY/FC_POSTSCRIPT_NAME/FC_WEIGHT/FC_WIDTH/
        // FC_SLANT，且从不读 FC_FILE/FC_INDEX——这四个字段补全 bestMatch()
        // 唯一来源调用点实际需要的返回信息（此前 FontEntry 只有 familyName
        // + FontHandle，5 个要读的字段缺 4 个，唯一提供的 FontHandle 那处
        // 根本不用）。

        // 来源：FC_POSTSCRIPT_NAME。PostScript 名是精确标识符，语义上和
        // "家族名模糊匹配"不是一回事，8 字段查询预算里没有单独的 postscript
        // 查询字段（见 PkFontQuery::families 注释），但结果侧必须能把它
        // 还给调用方。
        PkString postScriptName;

        // 来源：FC_WEIGHT，同 PkFontQuery::weight 的取值空间（OpenType
        // weight，1–1000）。
        //
        // 评审 I-4：默认值是**越界哨兵**，不是「像真的」的取值——
        // sortedMatches()/allFonts() 路径按 I-6 分工表不填这个字段，此前的
        // 默认值 400 恰好是合法的 Regular weight，调用方若误读会拿到「每个
        // 字体都是 Regular」这种静默错数据，编译期/运行期都不会有任何信号。
        // 1–1000 是合法取值空间，-1 落在区间外，误用可被检出（对照
        // PkFontQuery::size/pixelSize 的 -1.0 哨兵，口径一致）。
        int weight = -1;

        // 来源：FC_WIDTH，同 PkFontQuery::width 的取值空间（OpenType
        // width，100 = normal）。评审 I-4：同 weight，默认值是越界哨兵
        // （合法取值空间为正整数，-1 落在区间外），不是「像真的」的 100。
        int width = -1;

        // 来源：FC_SLANT。评审 I-4：默认值是 Unknown 哨兵，不是「像真的」的
        // Normal——sortedMatches()/allFonts() 路径不填这个字段（见 I-6 分工
        // 表），Unknown 让「没读」和「读到的确实是 Normal」在类型层面可区分。
        Slant slant = Slant::Unknown;
    };

    PkFontProvider();
    virtual ~PkFontProvider();

    // ── ②配置与路径 ──────────────────────────────────────────────────

    // 来源：FcConfigCreate + FcConfigParseAndLoad + FcConfigSetCurrent +
    // FcConfigGetCurrent，KoFontRegistry.cpp:113-143 Private 构造函数——
    // "探测 fonts.conf 在哪、解析加载、设为当前配置"这一整段折叠成一次调用。
    // configSearchPath 对应原代码里 `FONTCONFIG_PATH` 环境变量 / `/etc/fonts`
    // 探测那一段（:124-136）——**这正是 Android 上必须换掉的部分**：
    // Android 没有 `/etc/fonts`，具体实现可以把这个参数解释成平台自己的字体
    // 配置根（或直接忽略，走内置默认表）。返回 false 表示初始化失败
    // （对应原代码 `FcConfigParseAndLoad` 返回假时回退到 FcConfigGetCurrent()
    // 的那条容错路径——是否容错、容错到什么程度由具体实现决定）。
    virtual bool initialize(const PkString &configSearchPath) = 0;

    // 来源：FcConfigAppFontAddFile，KoFontRegistry.cpp:1236-1245
    // `addFontFilePathToRegistry()`。原方法是 `private`，注释写"Right now
    // only used by unittests"——按上级任务判断，这正是 Android 从 apk 里
    // 注册字体所需的入口，本端口把它提到 public。
    virtual bool addFontFile(const PkString &path) = 0;

    // 来源：FcConfigAppFontAddDir，KoFontRegistry.cpp:1247-1256
    // `addFontFileDirectoryToRegistry()`，同上从 private 提到 public 的理由。
    virtual bool addFontDirectory(const PkString &path) = 0;

    // 来源：FcConfigGetFontDirs + 族③ FcStrList 遍历（10 处调用，唯一用途
    // 就是遍历这一个 API 的结果），KoFontRegistry.cpp:150-162。族③不是独立
    // 能力，折叠进这一个返回 vector 的方法，不单独暴露链表迭代器类型。
    virtual std::vector<PkString> fontDirectories() const = 0;

    // 来源：FcConfigBuildFonts，KoFontRegistry.cpp:282-288 `updateConfig()`
    // ——字体目录发生变化后触发重新扫描；返回值对应原调用是否成功触发了
    // 重建（原代码里只有成功才会 reloadConverter()）。
    virtual bool rebuildFontSet() = 0;

    // ── ⑤匹配 ────────────────────────────────────────────────────────

    // 按 query 排序返回候选字体列表（最佳匹配在前，允许为空）。来源：
    // FcConfigSubstitute + FcDefaultSubstitute + FcFontSort，
    // KoFontRegistry.cpp:401-402,430（主匹配路径 facesForCSSValues()）
    // 以及 :213-223 `fallbackFont()`（`families` 只给 "sans-serif" 时的
    // 兜底路径——如前所述，回退不是独立方法，是用更宽的 query 再调一次这
    // 同一个方法）。**不复刻 FcFontRenderPrepare 的 pattern-merge 语义**
    // （零命中）：结果里的每个 FontEntry 就是候选本身。返回结果只保证
    // handle/familyName/languages 有意义（见 FontEntry 类头注释 I-6 的分工
    // 表），postScriptName/weight/width/slant 是否填充由实现决定。
    //
    // 评审 I-1（契约，实现必须遵守）：KoFontRegistry.cpp:465-470 用
    // FC_SCALABLE/FC_PIXEL_SIZE 跳过"非缩放且 pixelSize 不等于请求值"的
    // 位图字体（这是修正 fontconfig 一个已知偏差的过滤，不是可选优化）。
    // **实现必须在返回前自己内部做这层过滤**：候选是非缩放位图字体、且
    // `query.pixelSize >= 0`、且候选自身的像素大小与 `query.pixelSize` 不
    // 相等时，不得出现在返回结果里——调用方拿到的列表已经是过滤后的最终
    // 候选，不需要（也没有材料支撑）自己再做一遍这层判断。
    virtual std::vector<FontEntry> sortedMatches(const PkFontQuery &query) const = 0;

    // 单个最佳匹配，返回 false 表示无匹配（对应 FcResultNoMatch）。来源：
    // FcFontMatch + FcDefaultSubstitute，KoFontRegistry.cpp:1200-1207
    // `getCssDataForPostScriptName()`——按 PostScript 名找最接近的字体（见
    // `PkFontQuery::families` 注释里关于 postscript name 的收窄说明）。
    // 这条路径的唯一调用点读 familyName/postScriptName/weight/width/slant，
    // 从不读 handle/languages（见 FontEntry 类头注释 I-6 的分工表）——
    // 评审 C-2：此前 FontEntry 只有 familyName + handle，服务不了这个调用点
    // 实际要读的 5 个字段。
    virtual bool bestMatch(const PkFontQuery &query, FontEntry *outEntry) const = 0;

    // ── ⑥字体集枚举 ──────────────────────────────────────────────────

    // 枚举系统全部已知字体。来源：FcObjectSetBuild + FcFontList，
    // KoFontRegistry.cpp:252-253 `reloadConverter()`——结果喂给
    // KoFFWWSConverter 做 WWS 家族归并（归并逻辑不在本端口范围内，只出这一
    // 组原始字段）。
    virtual std::vector<FontEntry> allFonts() const = 0;

    // ── ⑦字符集 ──────────────────────────────────────────────────────

    // 这个字体是否覆盖给定码点。来源：FcCharSetHasChar，
    // KoFontRegistry.cpp:474,490——facesForCSSValues() 逐 grapheme 回退匹配
    // 时用它从一个共享的候选列表里挑出真正能显示该 grapheme 的字体，不是
    // 每个 grapheme 都重新排序一次候选。调用方典型用法：先调
    // sortedMatches() 拿到候选列表一次（PkFontQuery 没有单独的 charset
    // 约束字段，评审 I-5：零调用点，已删），之后对每个 grapheme 的首个码点
    // 循环调这个方法测试各候选。
    virtual bool coversCodepoint(const FontHandle &font, char32_t codepoint) const = 0;
};
