#include "test_mime_database.h"
#include "../PkMimeDatabase.h"

// 全部 37 条表（mimeType/description/suffixes）的来源、逐条核对方法见任务
// 报告「表核对方法」一节：脚本从 libs/koplugin/KisMimeDatabase.cpp 168-353 行
// 用正则抽取三元组生成下面两张数据表（非手抄），抽取结果与逐条人工通读比对
// 完全一致。文件里第 323-326 行的 jp2 那 4 行本身就是注释掉的（`//` 开头），
// 从未执行 `s_mimeDatabase << mimeType`，所以真实运行时表是 37 条，不是 38
// 条——与本任务分派 prompt 里假设的「38 条」不符，这是任务分派环节对源码的
// 误记，不是本次转录漏项；已按真实源码核实过（详见报告）。

void TestMimeDatabase::boundaryEntriesFirstLastMultiSuffix()
{
    // 首条
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("gbr")), PkString("image/x-gimp-brush"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("vbr")), PkString("image/x-gimp-brush"));
    // 末条（真实源码里最后一次进表的条目）
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("hdr")), PkString("image/vnd.radiance"));
    // 多后缀条目：kpp 是单后缀的对照
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("kpp")), PkString("application/x-krita-paintoppreset"));
    // 10 后缀的 gpl/pal/act/aco/colors/xml/sbz/acb/ase/css 都映射到同一个 mimeType
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("gpl")), PkString("application/x-gimp-color-palette"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("css")), PkString("application/x-gimp-color-palette"));
    // 37 后缀的 x-krita-raw：首尾各测一个
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("bay")), PkString("image/x-krita-raw"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("srw")), PkString("image/x-krita-raw"));
}

void TestMimeDatabase::allSuffixesRoundTripToMimeType()
{
    // 全部 37 条表项、按后缀展开后的 84 对 (mimeType, suffix)——每一个后缀都
    // 单独测一遍，一个不漏。顺序与 libs/koplugin/KisMimeDatabase.cpp
    // fillMimeData() 里出现的顺序一致。
    struct Pair { const char *mimeType; const char *suffix; };
    static const Pair pairs[] = {
        {"image/x-gimp-brush", "gbr"},
        {"image/x-gimp-brush", "vbr"},
        {"image/x-gimp-brush-animated", "gih"},
        {"image/x-adobe-brushlibrary", "abr"},
        {"application/x-krita-paintoppreset", "kpp"},
        {"application/x-mypaint-brush", "myb"},
        {"application/x-krita-assistant", "paintingassistant"},
        {"image/x-r32", "r32"},
        {"image/x-r16", "r16"},
        {"image/x-r8", "r8"},
        {"application/x-spriter", "scml"},
        {"image/x-svm", "svm"},
        {"image/openraster", "ora"},
        {"application/x-photoshop-style-library", "asl"},
        {"application/x-gimp-color-palette", "gpl"},
        {"application/x-gimp-color-palette", "pal"},
        {"application/x-gimp-color-palette", "act"},
        {"application/x-gimp-color-palette", "aco"},
        {"application/x-gimp-color-palette", "colors"},
        {"application/x-gimp-color-palette", "xml"},
        {"application/x-gimp-color-palette", "sbz"},
        {"application/x-gimp-color-palette", "acb"},
        {"application/x-gimp-color-palette", "ase"},
        {"application/x-gimp-color-palette", "css"},
        {"application/x-krita-palette", "kpl"},
        {"application/x-opencolorio-configuration", "ocio"},
        {"application/x-gimp-gradient", "ggr"},
        {"image/x-gimp-pat", "pat"},
        {"application/x-krita-bundle", "bundle"},
        {"application/x-krita-workspace", "kws"},
        {"application/x-krita-windowlayout", "kwl"},
        {"application/x-krita-session", "ksn"},
        {"application/x-krita-taskset", "kts"},
        {"application/x-krita-reference-images", "krf"},
        {"application/x-krita-gamutmasks", "kgm"},
        {"application/x-krita-shortcuts", "shortcuts"},
        {"image/x-krita-raw", "bay"},
        {"image/x-krita-raw", "bmq"},
        {"image/x-krita-raw", "cr2"},
        {"image/x-krita-raw", "crw"},
        {"image/x-krita-raw", "cs1"},
        {"image/x-krita-raw", "dc2"},
        {"image/x-krita-raw", "dcr"},
        {"image/x-krita-raw", "dng"},
        {"image/x-krita-raw", "erf"},
        {"image/x-krita-raw", "fff"},
        {"image/x-krita-raw", "k25"},
        {"image/x-krita-raw", "kdc"},
        {"image/x-krita-raw", "mdc"},
        {"image/x-krita-raw", "mos"},
        {"image/x-krita-raw", "mrw"},
        {"image/x-krita-raw", "nef"},
        {"image/x-krita-raw", "orf"},
        {"image/x-krita-raw", "pef"},
        {"image/x-krita-raw", "pxn"},
        {"image/x-krita-raw", "raf"},
        {"image/x-krita-raw", "raw"},
        {"image/x-krita-raw", "rdc"},
        {"image/x-krita-raw", "sr2"},
        {"image/x-krita-raw", "srf"},
        {"image/x-krita-raw", "x3f"},
        {"image/x-krita-raw", "arw"},
        {"image/x-krita-raw", "3fr"},
        {"image/x-krita-raw", "cine"},
        {"image/x-krita-raw", "ia"},
        {"image/x-krita-raw", "kc2"},
        {"image/x-krita-raw", "mef"},
        {"image/x-krita-raw", "nrw"},
        {"image/x-krita-raw", "qtk"},
        {"image/x-krita-raw", "rw2"},
        {"image/x-krita-raw", "sti"},
        {"image/x-krita-raw", "rwl"},
        {"image/x-krita-raw", "srw"},
        {"application/x-extension-exr", "exr"},
        {"image/x-psb", "psb"},
        {"image/heic", "heic"},
        {"image/heic", "heif"},
        {"image/avif", "avif"},
        {"application/x-krita-seexpr-script", "kse"},
        {"application/x-krita-archive", "krz"},
        {"image/apng", "apng"},
        {"image/jxl", "jxl"},
        {"text/csv", "csv"},
        {"image/vnd.radiance", "hdr"},
    };
    PK_COMPARE(static_cast<int>(sizeof(pairs) / sizeof(pairs[0])), 84);
    for (const Pair &p : pairs) {
        PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString(p.suffix)), PkString(p.mimeType));
    }
}

void TestMimeDatabase::allDescriptionsMatchKritaOriginalEnglishText()
{
    // 全部 37 条表项的 (mimeType, description) 对，description 是真实
    // i18nc("description of a file type", "...") 调用的第二个参数（未翻译
    // 英文原文），逐条比对不省略。
    struct Pair { const char *mimeType; const char *description; };
    static const Pair pairs[] = {
        {"image/x-gimp-brush", "Gimp Brush"},
        {"image/x-gimp-brush-animated", "Gimp Image Hose Brush"},
        {"image/x-adobe-brushlibrary", "Adobe Brush Library"},
        {"application/x-krita-paintoppreset", "Krita Brush Preset"},
        {"application/x-mypaint-brush", "MyPaint Brush"},
        {"application/x-krita-assistant", "Krita Assistant"},
        {"image/x-r32", "R32 Heightmap"},
        {"image/x-r16", "R16 Heightmap"},
        {"image/x-r8", "R8 Heightmap"},
        {"application/x-spriter", "Spriter SCML"},
        {"image/x-svm", "Starview Metafile"},
        {"image/openraster", "OpenRaster Image"},
        {"application/x-photoshop-style-library", "Photoshop Layer Style Library"},
        {"application/x-gimp-color-palette", "Color Palette"},
        {"application/x-krita-palette", "Krita Color Palette"},
        {"application/x-opencolorio-configuration", "OpenColorIO Configuration"},
        {"application/x-gimp-gradient", "GIMP Gradients"},
        {"image/x-gimp-pat", "GIMP Patterns"},
        {"application/x-krita-bundle", "Krita Resource Bundle"},
        {"application/x-krita-workspace", "Krita Workspace"},
        {"application/x-krita-windowlayout", "Krita Window Layout"},
        {"application/x-krita-session", "Krita Session"},
        {"application/x-krita-taskset", "Krita Taskset"},
        {"application/x-krita-reference-images", "Krita Reference Image Collection"},
        {"application/x-krita-gamutmasks", "Krita Gamut Mask"},
        {"application/x-krita-shortcuts", "Krita Shortcut Scheme"},
        {"image/x-krita-raw", "Camera Raw Files"},
        {"application/x-extension-exr", "OpenEXR (Extended)"},
        {"image/x-psb", "Photoshop Image (Large)"},
        {"image/heic", "HEIC/HEIF Image"},
        {"image/avif", "AVIF Image"},
        {"application/x-krita-seexpr-script", "SeExpr script package"},
        {"application/x-krita-archive", "Krita Archival Image Format"},
        {"image/apng", "Animated PNG Image"},
        {"image/jxl", "JPEG-XL Image"},
        {"text/csv", "CSV Document"},
        {"image/vnd.radiance", "Radiance RGBE Image"},
    };
    PK_COMPARE(static_cast<int>(sizeof(pairs) / sizeof(pairs[0])), 37);
    for (const Pair &p : pairs) {
        PK_COMPARE(PkMimeDatabase::descriptionForMimeType(PkString(p.mimeType)), PkString(p.description));
    }
}

void TestMimeDatabase::suffixesForMimeTypeReturnsAllWithPreferredFirst()
{
    // image/x-krita-raw 有 37 个后缀，首选（第一个）是 "bay"。
    PkStringList suffixes = PkMimeDatabase::suffixesForMimeType(PkString("image/x-krita-raw"));
    PK_COMPARE(suffixes.size(), 37);
    PK_VERIFY(suffixes.at(0) == PkString("bay"));
    PK_VERIFY(suffixes.at(36) == PkString("srw"));

    // image/heic 有 2 个后缀，首选是 "heic"。
    PkStringList heicSuffixes = PkMimeDatabase::suffixesForMimeType(PkString("image/heic"));
    PK_COMPARE(heicSuffixes.size(), 2);
    PK_VERIFY(heicSuffixes.at(0) == PkString("heic"));
    PK_VERIFY(heicSuffixes.at(1) == PkString("heif"));

    // application/x-krita-paintoppreset 只有 1 个后缀。
    PkStringList kppSuffixes = PkMimeDatabase::suffixesForMimeType(PkString("application/x-krita-paintoppreset"));
    PK_COMPARE(kppSuffixes.size(), 1);
    PK_VERIFY(kppSuffixes.at(0) == PkString("kpp"));
}

void TestMimeDatabase::unknownExtensionReturnsEmpty()
{
    // 决定原文：「表里没有的扩展名就是不支持」——不做 QMimeDatabase 内容嗅探
    // 兜底。
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("unknownext")), PkString(""));
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("jp2")), PkString("")); // 真实源码里被注释掉、从未进表
    PK_COMPARE(PkMimeDatabase::mimeTypeForSuffix(PkString("")), PkString(""));
}

void TestMimeDatabase::descriptionForUnknownMimeTypeReturnsMimeTypeItself()
{
    // descriptionForMimeType 找不到时原样返回 mimeType 自己——这是真实
    // KisMimeDatabase::descriptionForMimeType 在表查不到时的最终返回值
    // （libs/koplugin/KisMimeDatabase.cpp 第 117 行 `return mimeType;`），
    // 不依赖 QMimeDatabase，本任务保留这条行为。
    PK_COMPARE(PkMimeDatabase::descriptionForMimeType(PkString("application/x-unknown")), PkString("application/x-unknown"));
    PK_COMPARE(PkMimeDatabase::descriptionForMimeType(PkString("")), PkString(""));
}

void TestMimeDatabase::suffixesForUnknownMimeTypeReturnsEmptyList()
{
    PkStringList suffixes = PkMimeDatabase::suffixesForMimeType(PkString("application/x-unknown"));
    PK_COMPARE(suffixes.size(), 0);
}

void TestMimeDatabase::mimeTypeForFileUsesLowercasedSuffix()
{
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("brush.GBR")), PkString("image/x-gimp-brush"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("brush.Gbr")), PkString("image/x-gimp-brush"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("brush.gbr")), PkString("image/x-gimp-brush"));
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("preset.KPP")), PkString("application/x-krita-paintoppreset"));
}

void TestMimeDatabase::mimeTypeForFileHandlesNoExtensionAndDotfile()
{
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("noext")), PkString(""));
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("")), PkString(""));
    // 隐藏文件（点是 basename 首字符）视为「没有后缀」，与 QFileInfo::suffix() 一致。
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString(".bashrc")), PkString(""));
}

void TestMimeDatabase::mimeTypeForFileHandlesPathWithDotsInDirectory()
{
    // 目录名里带点不应该被误当成后缀分隔符——取的是最后一个 '/' 之后的部分。
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("/home/user/my.folder/brush.gbr")), PkString("image/x-gimp-brush"));
    // 目录带点、文件本身没有后缀 → 仍是空。
    PK_COMPARE(PkMimeDatabase::mimeTypeForFile(PkString("/home/user/my.folder/noext")), PkString(""));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_mime_database.inc"

int run_mime_database_tests(int argc, char **argv)
{
    TestMimeDatabase tc;
    return PkTest::qExec(&tc, argc, argv);
}
