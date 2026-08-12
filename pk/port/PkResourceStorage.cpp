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
    if (dir.empty()) {
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

// 折叠连续分隔符、解析 "." 与 ".."。绝对路径上多余的 ".."（越过根）被丢弃
// ——同 QDir::cleanPath 的实测行为一致（真链 Qt 未在本任务里重新探针，按
// Qt 文档 + 常识行为实现，登记见 pk/port/README.md「哪些地方是猜的」）。
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
            } else if (!absolute) {
                segments.push_back("..");
            }
            // absolute 且没有可回退的段：越过根，丢弃这个 ".."。
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
    for (std::size_t i = common; i < baseSegs.size(); ++i) {
        if (!out.empty()) {
            out += "/";
        }
        out += "..";
    }
    for (std::size_t i = common; i < targetSegs.size(); ++i) {
        if (!out.empty()) {
            out += "/";
        }
        out += targetSegs[i];
    }
    if (out.empty()) {
        out = ".";
    }
    return fromUtf8(out);
}
