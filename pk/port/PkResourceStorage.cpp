#include "PkResourceStorage.h"

#include "PkString.h"

#include <string>
#include <vector>

PkResourceStorage::EntryIterator::EntryIterator() = default;
PkResourceStorage::EntryIterator::~EntryIterator() = default;

PkResourceStorage::PkResourceStorage() = default;
PkResourceStorage::~PkResourceStorage() = default;

// ---------------------------------------------------------------------------
// 路径拼接：纯字符串工具。内部转成 std::string 操作——PkString 的公开 API
// 是 QString 14 项用量表的镜像，没有 endsWith()，用 std::string 实现更直接、
// 不为了凑一个 endsWith() 去扩大 PkString 的清单外 API。
// ---------------------------------------------------------------------------

namespace {

std::string toUtf8(const PkString &s)
{
    return s.PkToUtf8();
}

PkString fromUtf8(const std::string &s)
{
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

// 按 '/' 切分成非空段，同时报告原串是否以 '/' 开头（绝对路径）。
std::vector<std::string> splitSegments(const std::string &path, bool *absoluteOut)
{
    std::vector<std::string> segments;
    if (absoluteOut) {
        *absoluteOut = !path.empty() && path.front() == '/';
    }
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
    return segments;
}

} // namespace

PkString PkResourceStorage::joinPath(const PkString &dirPath, const PkString &name)
{
    const std::string dir = toUtf8(dirPath);
    const std::string leaf = toUtf8(name);
    // 评审 M-1：**有意收窄，不是遗漏**——真 Qt `QDir::filePath()` 对空目录/
    // 空 leaf 走的是"当前目录是 QDir 的隐式默认值"这条语义（`QDir()` 等价于
    // `QDir(".")`），拼出来的结果会带上 `.`/`./` 前缀或去掉目录侧多余的尾部
    // 斜杠。真链 Qt 5.15.13 探针（`pk/port/probe/probe_qdir.cpp` "joinPath()
    // 与真 Qt 的 7 处未登记分歧"一节，2026-08-12 实测）：
    //   joinPath("",       "a")  pk="a"        真 Qt QDir().filePath("a")        = "./a"
    //   joinPath("",       "")   pk=""         真 Qt QDir().filePath("")         = "."
    //   joinPath("/root/", "")   pk="/root/"   真 Qt QDir("/root/").filePath("") = "/root"
    // 本端口没有"当前目录"这个隐式概念（调用方总是显式传目录），也没有已知
    // 调用点会传空目录字符串——跟着 QDir 的隐式默认值走反而会让空目录输入
    // 拼出带 "." 前缀的结果，对资源路径拼接没有实际意义。**保持简单收窄**：
    // 空 dir 原样返回 leaf、空 leaf 原样返回 dir，不模拟 QDir 的隐式默认目录
    // 语义。另外 4 个同类形态（"a/"、"./a"、"../a"、"a/b"，dir 均为
    // "/root/"）探针实测跟本实现**完全一致**，不在收窄范围内，见探针输出。
    if (dir.empty()) {
        return fromUtf8(leaf);
    }
    // 真 Qt QDir::filePath/absoluteFilePath 在 name 是绝对路径时原样返回
    // name，完全不管 dir 是什么——评审 I-2，真链 Qt 探针见
    // pk/port/probe/probe_qdir.cpp。
    if (!leaf.empty() && leaf.front() == '/') {
        return fromUtf8(leaf);
    }
    if (leaf.empty()) {
        return fromUtf8(dir);
    }
    const bool dirTrail = dir.back() == '/';
    const bool leafLead = leaf.front() == '/';
    if (dirTrail && leafLead) {
        return fromUtf8(dir + leaf.substr(1));
    }
    if (dirTrail || leafLead) {
        return fromUtf8(dir + leaf);
    }
    return fromUtf8(dir + "/" + leaf);
}

// 折叠连续分隔符、解析 "." 与 ".."。**真 Qt `QDir::cleanPath` 不折叠越过根
// 的 ".."，原样保留**——评审 C-1：这里此前按"Qt 文档 + 常识"猜的是丢弃，
// 与真 Qt 相反；真链 Qt 5.15.13 探针见 pk/port/probe/probe_qdir.cpp（
// `"/.."` → `"/.."`、`"/../.."` → `"/../.."`、`"/../a"` → `"/../a"`、
// `"/a/../.."` → `"/.."`），其余既有形态（`"//"`、结尾 `"/"`、`"."`、
// 空串、`"../.."`、`"a/./b"`、`"a/b/../.."`）都与探针结果一致，唯独越根
// ".." 这一类之前是反的。
PkString PkResourceStorage::cleanPath(const PkString &path)
{
    const std::string in = toUtf8(path);
    if (in.empty()) {
        return fromUtf8(in);
    }

    bool absolute = false;
    const std::vector<std::string> rawSegments = splitSegments(in, &absolute);

    std::vector<std::string> segments;
    for (const std::string &seg : rawSegments) {
        if (seg == ".") {
            continue;
        }
        if (seg == "..") {
            if (!segments.empty() && segments.back() != "..") {
                segments.pop_back();
            } else {
                // 没有可回退的段：无论 absolute 与否都保留这个 ".."（真 Qt
                // 在绝对路径上也不丢弃越根的 ".."）。
                segments.push_back("..");
            }
            continue;
        }
        segments.push_back(seg);
    }

    std::string out;
    if (absolute) {
        out += "/";
    }
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i) {
            out += "/";
        }
        out += segments[i];
    }
    if (out.empty()) {
        out = absolute ? "/" : ".";
    }
    return fromUtf8(out);
}

// 计算从 basePath 到 targetPath 的相对路径：先各自按 cleanPath 规范化，
// 剥离公共前缀段，为 base 剩下的每一段补一个 ".."，再接上 target 剩下的段。
PkString PkResourceStorage::relativePath(const PkString &basePath, const PkString &targetPath)
{
    const std::string base = toUtf8(cleanPath(basePath));
    const std::string target = toUtf8(cleanPath(targetPath));

    bool baseAbs = false;
    bool targetAbs = false;
    std::vector<std::string> baseSegs = splitSegments(base, &baseAbs);
    std::vector<std::string> targetSegs = splitSegments(target, &targetAbs);

    if (baseAbs != targetAbs) {
        // 一个绝对一个相对，没有公共基准可比——原样返回规范化后的 target。
        return fromUtf8(target);
    }

    std::size_t common = 0;
    while (common < baseSegs.size() && common < targetSegs.size()
           && baseSegs[common] == targetSegs[common]) {
        ++common;
    }

    std::string out;
    const bool hasClimb = common < baseSegs.size();
    for (std::size_t i = common; i < baseSegs.size(); ++i) {
        if (!out.empty()) {
            out += "/";
        }
        out += "..";
    }
    const bool hasTargetRemainder = common < targetSegs.size();
    for (std::size_t i = common; i < targetSegs.size(); ++i) {
        if (!out.empty()) {
            out += "/";
        }
        out += targetSegs[i];
    }
    if (out.empty()) {
        out = ".";
    } else if (hasClimb && !hasTargetRemainder) {
        // 评审 M-2：真 Qt QDir::relativeFilePath 在 target 是 base 祖先目录
        // 时（爬完 ".." 之后 target 侧没有剩余段可拼）结果带一个多余的尾部
        // "/"——真链 Qt 探针见 pk/port/probe/probe_qdir.cpp：
        // "/a/b/c"→"/a" 得 "../../"，"/a/b"→"/a" 得 "../"。
        out += "/";
    }
    return fromUtf8(out);
}
