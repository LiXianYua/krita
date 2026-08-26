// PkNodeId —— QUuid 的零 Qt 对应物（S-06 遗留缺口补定义，S-05-b 交接路径）。
//
// S-06 剥 libs/image 时把 `QUuid` 改名 `PkNodeId`（kis_base_node.h 的
// uuid()/setUuid()、kis_layer_composition.h 的 PkMap<PkNodeId,bool>、
// kis_psd_layer_style.h 的 uuid() 等），但从未定义——pk/ 与 libs/image 都只有
// 消费方，S-03-e 交接「归 S-08 全链验或 pk 层补定义」，S-08 未做。本任务
// （S-05-b，libkra 用 setUuid）在 pk 层补定义。
//
// API 对拍 Qt 5.15 QUuid（从消费方反推，不是完整 QUuid 面）：
//   - PkNodeId()                        null（全零）
//   - PkNodeId(const PkString&)         解析（{...}/无花括号 8-4-4-4-12，无效 → null）
//   - PkNodeId::createUuid()            RFC 4122 v4（随机）
//   - PkNodeId::fromString(...)         同解析构造
//   - isNull()                          是否 null
//   - toString()                        带花括号 {xxxxxxxx-xxxx-...}（对拍 QUuid::toString）
//   - operator== / != / <               PkMap key 需要 <
//
// 实现不依赖 Qt，随机数用 std::random_device + std::mt19937（非加密用途足够）。
#pragma once

#include "PkString.h"

#include <array>
#include <cstdint>

class PkNodeId
{
public:
    PkNodeId();
    explicit PkNodeId(const PkString &text);

    static PkNodeId createUuid();
    static PkNodeId fromString(const PkString &text);

    bool isNull() const;
    PkString toString() const;

    bool operator==(const PkNodeId &o) const;
    bool operator!=(const PkNodeId &o) const;
    bool operator<(const PkNodeId &o) const;

private:
    std::array<std::uint8_t, 16> m_data;
};
