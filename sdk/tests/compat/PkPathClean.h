// R-77 · sdk/tests/compat/PkPathClean.h
// ---------------------------------------------------------------------------
// 真 Qt `qt_cleanPath` 的对齐实现，**QDir::cleanPath 与 QFileInfo::absoluteFilePath
// 共用这一份**。不是 Qt 同名垫片（没有 Q 名），是这两个垫片之间的内部工具头——
// 与同目录的 PkTestFailMessage.h / PkTestSleepShim.h / QtGlobalColorAliases.h 同类。
//
// 为什么共用而不是各写一份：两者只差**一条**规则——
//   * `QDir::cleanPath("/a/b/")`            → `"/a/b"`   （尾斜杠丢弃，探针 §9e）
//   * `QFileInfo("/a/b/").absoluteFilePath()` → `"/a/b/"` （尾斜杠**保留**，探针 §7b）
// 其余 17 个边角用例两边逐条相同（§9e / §7b）。算法写两遍就会漂。
//
// 为什么不放在 QDir 里：compat/QDir 从 R-65 起就 `#include "QFileInfo"`（真 Qt 的
// <QDir> 传递声明 QFileInfo，见该文件 18-29 行的探针记录），QFileInfo 反向 include
// QDir 会成环，且两文件谁先被 include 决定谁报 "incomplete type"。
//
// 逐条对齐的 18 个真 Qt 用例（探针 §9e，keepTrailingSlash=false）：
//   "/a/b/../c//d/"→"/a/c/d" · "/a/../../b"→"/../b" · ""→"" · "."→"." · ".."→".."
//   "/"→"/" · "//"→"/" · "a/b"→"a/b" · "./a/b"→"a/b" · "a/./b"→"a/b"
//   "/a/b/"→"/a/b" · "/a/b//"→"/a/b" · "a//b"→"a/b" · "../../a"→"../../a"
//   "/../a"→"/../a" · "a/.."→"." · "/a/.."→"/" · "a/b/../.."→"."
// 以及 §7b 的 18 个 absoluteFilePath 用例（keepTrailingSlash=true）。
// **越根的 ".." 原样保留**（"/a/../../b" → "/../b"），不是丢弃——这条是真 Qt 的
// 实测行为，也是 pk/port/PkResourceStorage::cleanPath 评审 C-1 踩过的坑。
//
// 与 pk/port/PkResourceStorage::cleanPath 的关系：算法同一份（那边早已探针钉死），
// 这里**不链接 pkport**，因为 sdk/tests/compat/* 的形态是「纯头、零库依赖」，而
// pkport 是 SHARED、还传递拖 minizip-ng——为一个纯函数把 minizip 拽进每一个 pk
// 测试目标不划算。两处算法如有改动需同步：pk/port 那处不在本 Task 的锁内，只报不改。
// ---------------------------------------------------------------------------
#pragma once
#include <string>
#include <vector>

inline std::string pkCleanPath(const std::string &path, bool keepTrailingSlash)
{
    if (path.empty()) {
        return path;
    }

    const bool absolute = path.front() == '/';
    // 探针 §7b：`/a/b/` → `/a/b/`、`a/b/../` → `…/a/`；但 `/` 本身不加第二条斜杠。
    const bool wantTrailing = keepTrailingSlash && path.size() > 1 && path.back() == '/';

    // 按 '/' 切分成非空段。
    std::vector<std::string> segments;
    std::string cur;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!cur.empty()) {
                segments.push_back(cur);
            }
            cur.clear();
        } else {
            cur += path[i];
        }
    }

    std::vector<std::string> kept;
    for (const std::string &seg : segments) {
        if (seg == ".") {
            continue;
        }
        if (seg == "..") {
            if (!kept.empty() && kept.back() != "..") {
                kept.pop_back();
            } else {
                // 没有可回退的段：absolute 与否都**保留**（真 Qt 在绝对路径上也不丢）。
                kept.push_back("..");
            }
            continue;
        }
        kept.push_back(seg);
    }

    std::string result;
    if (absolute) {
        result += "/";
    }
    for (std::size_t i = 0; i < kept.size(); ++i) {
        if (i) {
            result += "/";
        }
        result += kept[i];
    }
    if (result.empty()) {
        result = absolute ? "/" : ".";
    }
    if (wantTrailing && result.back() != '/') {
        result += "/";
    }
    return result;
}
