// R-75：`PkImage` 的文件 I/O —— 按路径构造 / `load` / `save`。
//
// 为什么是独立文件、编进 `pkimageio` 而不是 `pkimage`：`pkimage` 是零 Qt、零
// 编解码的静态库（`pk/image/README.md` §0：范围只做内存像素 buffer），PNG 等
// 编解码住在 `pkimageio`。所以这三个符号**声明**在 `pkimage`（`PkImage.h`）、
// **定义**在这里 —— 即 `libpkimage.a` 里 `PkImage(PkString,…)`/`load`/`save`
// **整个不存在**：既不是定义，也不是「被引用但没定义」的未定义符号（R-75 实测
// `nm -g -C libpkimage.a | grep -E 'PkImage::(load|save)\('` 零命中，详见
// `README.md` §3.1）。定义只存在于 `pkimageio` 且都是**导出**符号；任何调用它们
// 的消费方都必须在链接行上带 `pkimageio`。
//
// 语义对齐真 Qt 5.15.7 的 `QImage`，全部经探针实测（命令与原始输出见
// `.superpowers/sdd/R-75/task-1-report.md`「探针」一节）：
//   · `QImage(path)` / `load(path)` 打不开路径时得到 null image、返回 false，
//     不抛不崩（P2/P5）；
//   · `load` 失败时把 `*this` 置成 null image（P5：非 null 图上 load 坏路径后
//     `isNull()==true`、`width()==0`）；
//   · `save` 成功返回 true，坏目录/空路径/未知格式令牌/无后缀都返回 false
//     （P3/P7）；显式 `format` 优先于路径后缀（P3：`save("x.bin","PNG")` 成功）；
//   · 显式 `format` 令牌的语义**只判「有没有这个扩展名的解码器」**，不是
//     「内容必须是这个格式」。真 Qt 在内容与令牌不符时返回 null（P8：
//     `QImage(realpng,"JPG")` 为 null —— PNG 内容不是 JPEG），**本实现不模拟
//     这一格**：令牌有解码器（如 `"JPG"`，本仓 `native.jpeg` handler 认它）就
//     放行，随后按内容嗅探解码。这是一条**已声明的判据①范围裁剪偏离**（理由见
//     `README.md` §2，计数见 `oracle/image.deviation`）；
//   · 显式给了**本仓没有对应 handler 的令牌**时返回失败（与真 Qt 对未知格式
//     一致，P7）。

#include "PkImage.h"

#include "PkImageFileDecoder.h"
#include "PkImageIoExport.h"
#include "PkPngWriter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace
{

// 真 Qt 的 format 令牌既可写 "PNG" 也可写 ".png"、大小写不敏感。
std::string normalizedFormatToken(const char *format)
{
    if (!format) {
        return std::string();
    }
    std::string token(format);
    while (!token.empty() && token.front() == '.') {
        token.erase(token.begin());
    }
    std::transform(token.begin(), token.end(), token.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return token;
}

// 路径最后一个 '.' 之后的后缀（不含 '.'），小写；没有点、或点在最后一个路径
// 分隔符之前（如 "/a.b/c"），返回空串。
std::string extensionOfPath(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    if (slash != std::string::npos && dot < slash) {
        return std::string();
    }
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension;
}

bool formatTokenSupported(const std::string &token)
{
    const std::vector<std::string> supported = PkImageFileDecoder::supportedExtensions();
    return std::find(supported.begin(), supported.end(), token) != supported.end();
}

} // namespace

// 三个定义都要显式 default visibility：`pkimageio` 设了
// `CXX_VISIBILITY_PRESET hidden`，而 `PkImage` 类本身住在零 Qt 的静态库
// `pkimage` 里、类上带不了导出宏（它不知道上层有没有 dylib）。所以导出标记
// 落在**定义处**——这正是「声明在 pkimage、定义在 pkimageio」这条落地方式的
// 必然形状。改这里一定要连带改下面两个定义。
PKIMAGEIO_EXPORT PkImage::PkImage(const PkString &fileName, const char *format)
{
    // 对齐真 Qt：构造失败（路径打不开）时得到的是 null image，不是抛异常。
    load(fileName, format);
}

PKIMAGEIO_EXPORT bool PkImage::load(const PkString &fileName, const char *format)
{
    const std::string path = fileName.PkToUtf8();
    if (path.empty()) {
        *this = PkImage();
        return false;
    }

    // ⚠ format 令牌的语义（**已声明偏离**，理由见 `README.md` §2，计数见
    // `oracle/image.deviation`）：这一段只判「**有没有这个扩展名的解码器**」
    // （`formatTokenSupported`），**不判「内容必须是这个格式」**。真 Qt 在内容
    // 与令牌不符时返回 null（探针 P8：`QImage(一个真 PNG, "JPG")` 为 null）；
    // 本实现不模拟这一格 —— 令牌有解码器就放行，随后由 `PkImageFileDecoder::load`
    // 按**内容嗅探**解码，于是 `PkImage(png, "JPG")` 会成功。全仓零调用点传显式
    // format，按判据①「一项不多一项不少」裁剪为「存在解码器」判定，不做内容匹配。
    const std::string token = normalizedFormatToken(format);
    if (!token.empty() && !formatTokenSupported(token)) {
        *this = PkImage();
        return false;
    }

    // 解码复用既有的 `PkImageFileDecoder::load`（pk/image/PkImageFileDecoder.cpp:174）：
    // 它按内容嗅探选 handler，path 只作提示，正是 Qt 无显式 format 时的行为。
    const PkImage decoded = PkImageFileDecoder::load(path);
    *this = decoded;
    return !decoded.isNull();
}

PKIMAGEIO_EXPORT bool PkImage::save(const PkString &fileName, const char *format, int quality) const
{
    const std::string path = fileName.PkToUtf8();
    if (path.empty() || isNull()) {
        return false;
    }

    // 有效格式 = 显式 format 令牌；没给则取路径后缀（真 Qt 同序，探针 P3）。
    std::string effective = normalizedFormatToken(format);
    if (effective.empty()) {
        effective = extensionOfPath(path);
    }
    // 本仓目前只有 PNG 编码器（`PkPngWriter`）。其余令牌一律失败，与 Qt 对
    // 未知格式的返回一致（探针 P7），不做「假装成功」。
    if (effective != "png") {
        return false;
    }

    const std::vector<uint8_t> bytes = PkPngWriter::write(*this, quality);
    if (bytes.empty()) {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.flush();
    return static_cast<bool>(output);
}
