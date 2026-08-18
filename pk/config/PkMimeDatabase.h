#pragma once
#include "PkString.h"
#include "PkStringList.h"

// PkMimeDatabase 归属 R-02（pk/port），本头文件只前置声明——mimeTypeForData
// 的形参从不解引用它，前置声明足够通过编译；等 R-02 交付 PkByteArray 后，
// 调用方 #include 真定义即可，本文件不需要跟着改。
class PkByteArray;

// PkMimeDatabase —— libs/koplugin/KisMimeDatabase 的零 Qt 替代。
//
// 决定 Q-7（Qt替代品选型.md §6）：只保留 Krita 自己的 37 条硬编码 MIME 表
// （libs/koplugin/KisMimeDatabase.cpp 168-353 行 fillMimeData()，jp2 那 4 行
// 在真实源码里就是被注释掉的、从不进表——所以真正参与运行时查找的是 37 条，
// 不是文件里出现过的 mimeType 赋值语句总数），**丢弃 QMimeDatabase 的内容
// 嗅探兜底**：查表命中就返回，查不到就是「不支持」，不再问 Qt。
//
// description 是**未翻译的英文原文**（真实 i18nc(...) 调用的第二个参数）——
// 翻译职责移交 Flutter 侧，按这个英文字符串去查自己的翻译表。
//
// 不实现 iconNameForMimeType：唯一调用点是 UI 层，随 UI 一起删除（决定原文）。
// 不实现 Android 的 sanitizeSuffix 分支：Q_OS_ANDROID 宏保护的历史兼容 hack，
// 没有材料证明移动端仍然需要，保留范围之外——真需要时是待认领的缺口。
class PkMimeDatabase
{
public:
    // 按文件名找 mimetype。文件名需要带后缀。
    // checkExistingFiles 保留在签名里只为兼容旧调用点——真实 Krita 用它来
    // 决定要不要在表查不到时再用 QMimeDatabase 嗅探文件内容，而本类没有嗅探
    // 能力，所以这个参数在实现里未使用。
    static PkString mimeTypeForFile(const PkString &file, bool checkExistingFiles = true);

    // 按后缀找 mimetype。后缀可以带 "*.xxx" 形式或纯 "xxx"。
    static PkString mimeTypeForSuffix(const PkString &suffix);

    // 内容嗅探查 mimetype——本类没有这项能力（决定原文没有给它任何静态表可
    // 查），恒定返回空 PkString。
    static PkString mimeTypeForData(const PkByteArray &ba);

    // 找 mimetype 对应的用户可读描述。表里查不到时原样返回 mimeType 自己
    // ——这是真实 KisMimeDatabase::descriptionForMimeType 第 117 行自己的
    // 兜底分支，不依赖 QMimeDatabase，本类保留这条行为。
    static PkString descriptionForMimeType(const PkString &mimeType);

    // 找 mimetype 对应的全部后缀，第一个是首选后缀。表里查不到返回空列表。
    static PkStringList suffixesForMimeType(const PkString &mimeType);
};
