#include "PkMimeDatabase.h"

#include <vector>

namespace {

// 一条表项：mimeType / 未翻译英文 description / 全部后缀（首选在前）。
struct PkMimeEntry {
    const char *mimeType;
    const char *description;
    std::vector<const char *> suffixes;
};

// fillMimeData()：内部静态初始化，不导出（匿名命名空间，本 TU 之外不可见）。
// 逐字节抄 libs/koplugin/KisMimeDatabase.cpp 168-353 行 fillMimeData() 的 37
// 条数据——文件里第 323-326 行的 jp2 那一条从头到尾是注释掉的
// （`//        mimeType.mimeType = "image/jp2";` 等四行），从未执行
// `s_mimeDatabase << mimeType`，所以真实运行时的表是 37 条，不是 38 条；
// 本次转录用脚本从源文件里正则抽取三元组（mimeType/description/suffixes）
// 而非手抄，抽取结果与人工核对完全一致，详见任务报告「表核对方法」一节。
const std::vector<PkMimeEntry> &fillMimeData()
{
    static const std::vector<PkMimeEntry> table = {
        {"image/x-gimp-brush", "Gimp Brush", {"gbr", "vbr"}},
        {"image/x-gimp-brush-animated", "Gimp Image Hose Brush", {"gih"}},
        {"image/x-adobe-brushlibrary", "Adobe Brush Library", {"abr"}},
        {"application/x-krita-paintoppreset", "Krita Brush Preset", {"kpp"}},
        {"application/x-mypaint-brush", "MyPaint Brush", {"myb"}},
        {"application/x-krita-assistant", "Krita Assistant", {"paintingassistant"}},
        {"image/x-r32", "R32 Heightmap", {"r32"}},
        {"image/x-r16", "R16 Heightmap", {"r16"}},
        {"image/x-r8", "R8 Heightmap", {"r8"}},
        {"application/x-spriter", "Spriter SCML", {"scml"}},
        {"image/x-svm", "Starview Metafile", {"svm"}},
        {"image/openraster", "OpenRaster Image", {"ora"}},
        {"application/x-photoshop-style-library", "Photoshop Layer Style Library", {"asl"}},
        {"application/x-gimp-color-palette", "Color Palette", {"gpl", "pal", "act", "aco", "colors", "xml", "sbz", "acb", "ase", "css"}},
        {"application/x-krita-palette", "Krita Color Palette", {"kpl"}},
        {"application/x-opencolorio-configuration", "OpenColorIO Configuration", {"ocio"}},
        {"application/x-gimp-gradient", "GIMP Gradients", {"ggr"}},
        {"image/x-gimp-pat", "GIMP Patterns", {"pat"}},
        {"application/x-krita-bundle", "Krita Resource Bundle", {"bundle"}},
        {"application/x-krita-workspace", "Krita Workspace", {"kws"}},
        {"application/x-krita-windowlayout", "Krita Window Layout", {"kwl"}},
        {"application/x-krita-session", "Krita Session", {"ksn"}},
        {"application/x-krita-taskset", "Krita Taskset", {"kts"}},
        {"application/x-krita-reference-images", "Krita Reference Image Collection", {"krf"}},
        {"application/x-krita-gamutmasks", "Krita Gamut Mask", {"kgm"}},
        {"application/x-krita-shortcuts", "Krita Shortcut Scheme", {"shortcuts"}},
        {"image/x-krita-raw", "Camera Raw Files", {"bay", "bmq", "cr2", "crw", "cs1", "dc2", "dcr", "dng", "erf", "fff", "k25", "kdc", "mdc", "mos", "mrw", "nef", "orf", "pef", "pxn", "raf", "raw", "rdc", "sr2", "srf", "x3f", "arw", "3fr", "cine", "ia", "kc2", "mef", "nrw", "qtk", "rw2", "sti", "rwl", "srw"}},
        {"application/x-extension-exr", "OpenEXR (Extended)", {"exr"}},
        {"image/x-psb", "Photoshop Image (Large)", {"psb"}},
        {"image/heic", "HEIC/HEIF Image", {"heic", "heif"}},
        {"image/avif", "AVIF Image", {"avif"}},
        {"application/x-krita-seexpr-script", "SeExpr script package", {"kse"}},
        {"application/x-krita-archive", "Krita Archival Image Format", {"krz"}},
        {"image/apng", "Animated PNG Image", {"apng"}},
        {"image/jxl", "JPEG-XL Image", {"jxl"}},
        {"text/csv", "CSV Document", {"csv"}},
        {"image/vnd.radiance", "Radiance RGBE Image", {"hdr"}},
    };
    return table;
}

// fillImageFallbackMimeData()：**标准图片格式的后缀兜底表**，补回决定 Q-7
// （docs/Qt替代品选型.md §6.7）里说的「被一并删掉的后缀查表」——当年把
// QMimeDatabase 的**内容嗅探**去掉时，把它的**后缀兜底**也一起带走了，而
// png/svg/jpg/tif/… 从来不在上面那张 37 条的 Krita 表里，它们一直靠兜底。
//
// 这张表**只做「后缀 → mime」的查表，不做内容嗅探**：mimeTypeForData() 仍是
// 恒返回空串的桩（决定原文：内容嗅探与后缀查表是两件事，只有前者被去掉）。
//
// 收表口径：**每一条都要有本仓内的消费方**，不收「Qt 有我们就照抄」的项。
// 逐条依据见任务报告；概括：
//   png / svg   —— 出货资源实际用到的格式（data/ 下 155 个 png + 5 个 svg；
//                  data/bundles/Krita_4_Default_Resources.bundle 的 brushes 下
//                  39 个 .png + 3 个 .svg），也是本仓资源 loader 注册表里唯二
//                  用到的标准图片 mime：ResourceTestHelper.h:106（PngBrushes →
//                  image/png）、:103/:112/:142（SvgBrushes / StopGradients /
//                  Symbols → image/svg+xml），同形注册另见 sdk/tests/kistest.h:299-300。
//   jpg / jpeg / gif / bmp / tif / tiff / webp —— Patterns 那条注册
//                  （ResourceTestHelper.h:128 用 QImageReader::supportedMimeTypes()
//                  整表注册）里的标准位图格式。KisResourceLoaderBase::filters()
//                  （libs/resources/KisResourceLoader.cpp:13-25）对每个注册 mime 调
//                  suffixesForMimeType() 产出 "*.xxx"，KisStoragePlugin.cpp:50 对
//                  每个资源文件名调 mimeTypeForFile()——两个方向都消费它们。
// 不收的：Qt reader∩writer 里剩下的 exotic 格式
// xbm/xpm/ico/ppm/pgm/pbm/pcx/pic/rgb/tga/icns/wbmp，以及 reader-only 的
// psd/xcf/x-hdr —— 它们同样只被 Patterns 那条「整表照抄」的注册间接带上，本身
// 没有专门的资源 loader、也没有出货资源用这些后缀；本仓自己就把这一类归为
// exotic：libs/impex/KisPkImageFileDecoderAdapter.cpp:29-37 的 exoticExtensions()
// 列出该集合，:64-66 明写它们「are a graceful GAP until a static high-level
// filter owns them」。真需要时是待认领的缺口，不是本任务漏项。
//
// description 恒为 nullptr：本表**不服务** descriptionForMimeType()（判定见报告
// 「②」——该函数的唯一消费方只把它当人读的提示串，且上游兜底来自 Qt 的**已翻译**
// comment，我们既无源可抄也不该现编）。置 nullptr 是为了让「这张表没有描述」这件
// 事在类型上就写死，避免以后有人顺手接上去用。
const std::vector<PkMimeEntry> &fillImageFallbackMimeData()
{
    static const std::vector<PkMimeEntry> table = {
        {"image/png", nullptr, {"png"}},
        {"image/svg+xml", nullptr, {"svg"}},
        {"image/jpeg", nullptr, {"jpg", "jpeg"}},
        {"image/gif", nullptr, {"gif"}},
        {"image/bmp", nullptr, {"bmp"}},
        {"image/tiff", nullptr, {"tif", "tiff"}},
        {"image/webp", nullptr, {"webp"}},
    };
    return table;
}

// 在一张表里按后缀找条目（后缀须已折成小写）；找不到返回 nullptr。
// 返回的是指向函数内 static 容器的指针，生命周期与进程一致。
const PkMimeEntry *findEntryBySuffix(const std::vector<PkMimeEntry> &table, const PkString &suffix)
{
    for (const PkMimeEntry &entry : table) {
        for (const char *candidate : entry.suffixes) {
            if (suffix == PkString(candidate)) {
                return &entry;
            }
        }
    }
    return nullptr;
}

// 在一张表里按 mimeType 找条目；找不到返回 nullptr。
const PkMimeEntry *findEntryByMimeType(const std::vector<PkMimeEntry> &table, const PkString &mimeType)
{
    for (const PkMimeEntry &entry : table) {
        if (PkString(entry.mimeType) == mimeType) {
            return &entry;
        }
    }
    return nullptr;
}

// 「先查 Krita 37 条表，查不到退到标准图片格式兜底表」——mimeTypeForFile 与
// mimeTypeForSuffix 共用这一条查找路径，保证两者同源同语义（上游
// KisMimeDatabase 的两个函数也是「同一张 Krita 表 + 同一份 QMimeDatabase 兜底」，
// mimeTypeForSuffix 甚至直接折回 mimeTypeForFile）。
// 后缀须已折成小写。
PkString lookupMimeType(const PkString &suffix)
{
    if (const PkMimeEntry *entry = findEntryBySuffix(fillMimeData(), suffix)) {
        return PkString(entry->mimeType);
    }
    if (const PkMimeEntry *entry = findEntryBySuffix(fillImageFallbackMimeData(), suffix)) {
        return PkString(entry->mimeType);
    }
    return PkString();
}

// 只折 ASCII 大小写——suffix 恒为 ASCII 文件扩展名，非 ASCII 码元原样透传。
// 不复用 pk/container/PkStringList.h 里的 pkAsciiFold：那是 PkStringList
// 内部比较用的自由函数，本类只需要「造一个新的小写 PkString」，用 PkString
// 自己公开的 at()/mid()/PkFromUtf8() 就地实现，不额外拉一个跨模块依赖。
PkString toLowerAscii(const PkString &s)
{
    PkString out;
    const int n = s.size();
    for (int i = 0; i < n; ++i) {
        const char16_t c = s.at(i);
        if (c >= u'A' && c <= u'Z') {
            const char lower = static_cast<char>('a' + (c - u'A'));
            out += PkString::PkFromUtf8(&lower, 1);
        } else if (c < 0x80) {
            const char ascii = static_cast<char>(c);
            out += PkString::PkFromUtf8(&ascii, 1);
        } else {
            out += s.mid(i, 1);
        }
    }
    return out;
}

// 等价于 QFileInfo(file).suffix()：取最后一个路径分隔符之后、最后一个 '.'
// 之后的部分；点是那段 basename 的第一个字符时（隐藏文件，如 ".bashrc"）
// 判定为「没有后缀」，与 Qt 语义一致。不做 toLower，交给调用方。
PkString extractRawSuffix(const PkString &file)
{
    const int n = file.size();
    int lastSlash = -1;
    int lastDot = -1;
    for (int i = 0; i < n; ++i) {
        const char16_t c = file.at(i);
        if (c == u'/' || c == u'\\') {
            lastSlash = i;
        } else if (c == u'.') {
            lastDot = i;
        }
    }
    const int basenameStart = lastSlash + 1;
    if (lastDot == -1 || lastDot <= basenameStart) {
        return PkString();
    }
    return file.mid(lastDot + 1);
}

} // namespace

PkString PkMimeDatabase::mimeTypeForFile(const PkString &file, bool checkExistingFiles)
{
    (void)checkExistingFiles; // 无内容嗅探能力，此参数只保留签名兼容，见头文件注释

    return lookupMimeType(toLowerAscii(extractRawSuffix(file)));
}

PkString PkMimeDatabase::mimeTypeForSuffix(const PkString &suffix)
{
    return lookupMimeType(toLowerAscii(suffix));
}

PkString PkMimeDatabase::mimeTypeForData(const PkByteArray & /*ba*/)
{
    // 决定原文：没有给内容嗅探留任何静态表可查——恒定返回空。
    return PkString();
}

PkString PkMimeDatabase::descriptionForMimeType(const PkString &mimeType)
{
    // **只查 Krita 37 条表，不查兜底表**——判定依据见头文件与本文件对
    // fillImageFallbackMimeData() 的注释：本仓唯一消费方
    // （plugins/impex/qimageio/kis_qimageio_export.cpp:46）只把返回值当人读的
    // 提示串用，不参与任何匹配/控制流；而兜底表里的格式没有「Krita 自己写过
    // 的英文原文」可抄（上游那条兜底来自 Qt 的已翻译 comment，按决定 D-6
    // 翻译归 Flutter 侧）。查不到就原样返回 mimeType 自己。
    if (const PkMimeEntry *entry = findEntryByMimeType(fillMimeData(), mimeType)) {
        return PkString(entry->description);
    }

    // 表查不到时原样返回 mimeType 自己——真实 KisMimeDatabase.cpp 第 117 行
    // 的兜底分支，不依赖 QMimeDatabase，保留这条行为。
    return mimeType;
}

PkStringList PkMimeDatabase::suffixesForMimeType(const PkString &mimeType)
{
    // 与 mimeTypeForFile/mimeTypeForSuffix 同源：两张表都查（Krita 表优先）。
    // 依据：KisResourceLoaderBase::filters()（libs/resources/KisResourceLoader.cpp:13）
    // 对每个注册的 mime 调它来产出 "*.xxx" 过滤项，再由
    // KisFolderStorage::resources()（libs/resources/KisFolderStorage.cpp:182）拿去
    // 枚举资源文件——若 image/png、image/svg+xml 在这里查不到后缀，png/svg 笔尖
    // 与符号库/停靠渐变就不会出现在任何目录枚举结果里，与
    // mimeTypeForFile() 的「后缀 → mime」方向自相矛盾。
    const PkMimeEntry *entry = findEntryByMimeType(fillMimeData(), mimeType);
    if (!entry) {
        entry = findEntryByMimeType(fillImageFallbackMimeData(), mimeType);
    }
    if (!entry) {
        return PkStringList();
    }

    PkStringList result;
    for (const char *s : entry->suffixes) {
        result.append(PkString(s));
    }
    return result;
}
