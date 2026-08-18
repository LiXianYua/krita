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

    const PkString suffix = toLowerAscii(extractRawSuffix(file));

    for (const PkMimeEntry &entry : fillMimeData()) {
        for (const char *s : entry.suffixes) {
            if (suffix == PkString(s)) {
                return PkString(entry.mimeType);
            }
        }
    }

    return PkString();
}

PkString PkMimeDatabase::mimeTypeForSuffix(const PkString &suffix)
{
    const PkString s = toLowerAscii(suffix);

    for (const PkMimeEntry &entry : fillMimeData()) {
        for (const char *candidate : entry.suffixes) {
            if (s == PkString(candidate)) {
                return PkString(entry.mimeType);
            }
        }
    }

    return PkString();
}

PkString PkMimeDatabase::mimeTypeForData(const PkByteArray & /*ba*/)
{
    // 决定原文：没有给内容嗅探留任何静态表可查——恒定返回空。
    return PkString();
}

PkString PkMimeDatabase::descriptionForMimeType(const PkString &mimeType)
{
    for (const PkMimeEntry &entry : fillMimeData()) {
        if (PkString(entry.mimeType) == mimeType) {
            return PkString(entry.description);
        }
    }

    // 表查不到时原样返回 mimeType 自己——真实 KisMimeDatabase.cpp 第 117 行
    // 的兜底分支，不依赖 QMimeDatabase，保留这条行为。
    return mimeType;
}

PkStringList PkMimeDatabase::suffixesForMimeType(const PkString &mimeType)
{
    for (const PkMimeEntry &entry : fillMimeData()) {
        if (PkString(entry.mimeType) == mimeType) {
            PkStringList result;
            for (const char *s : entry.suffixes) {
                result.append(PkString(s));
            }
            return result;
        }
    }

    return PkStringList();
}
