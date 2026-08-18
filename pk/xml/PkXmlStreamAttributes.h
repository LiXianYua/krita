#pragma once

#include <cstddef>
#include <vector>

#include "../string/PkString.h"

// PkXmlStreamAttributes —— QXmlStreamAttributes 的零 Qt 对应物：一个元素起始
// 标签上全部属性的只读快照，由 PkXmlStreamReader::attributes() 在遇到
// StartElement 时构造。头文件里写全——只是对内部 std::vector 的直通存取，
// 没有需要摊到 .cpp 里的逻辑（同 PkXmlNodeList.h 的做法）。
//
// count()/value() 是 brief 的 Interfaces 列出的接口；hasAttribute() 是实现前
// 对真实调用点复核用量表时发现的缺口——`libs/pigment/resources/KoColorSet.cpp`
// （`colorProperties.hasAttribute("RGB")` 等）与
// `plugins/assistants/Assistants/kis_painting_assistant.cpp`（`xml.attributes()
// .hasAttribute("useCustomColor")` 等共 4 处）都有对 QXmlStreamAttributes 直接
// 调用 hasAttribute() 的真实用例，brief 的 Interfaces 枚举漏列了它（用量表
// §1 的 `hasAttribute | 108` 一行全部记在 PkXmlElement 名下，疑似把这几处
// QXmlStreamAttributes 的调用点也并进了那个计数——按 CLAUDE.md「新发现的缺口
// 直接补进来」原则补上，不是自创接口）。
class PkXmlStreamAttributes
{
public:
    struct Attribute {
        PkString name;
        PkString value;
    };

    PkXmlStreamAttributes() = default;

    PkString value(const PkString &name) const
    {
        for (const auto &a : _attrs) {
            if (a.name == name) {
                return a.value;
            }
        }
        return PkString();
    }

    bool hasAttribute(const PkString &name) const
    {
        for (const auto &a : _attrs) {
            if (a.name == name) {
                return true;
            }
        }
        return false;
    }

    int count() const { return static_cast<int>(_attrs.size()); }
    int size() const { return count(); } // QXmlStreamAttributes 是 QVector<QXmlStreamAttribute> 的别名，size() 同义转发

    // 迭代访问：实现前对真实调用点的用量复核（用量表 §1 QXmlStream 系，见
    // pk/xml 线级 plan）没有出现下标 `[i]`/range-for 遍历 QXmlStreamAttributes
    // 的写法——全部真实调用点用的是 `.value(name)`/`.hasAttribute(name)`。仍按
    // brief「迭代访问」的要求把两种通路都留出来，不锁死接口。
    const Attribute &operator[](int i) const { return _attrs[static_cast<std::size_t>(i)]; }
    std::vector<Attribute>::const_iterator begin() const { return _attrs.begin(); }
    std::vector<Attribute>::const_iterator end() const { return _attrs.end(); }

    // 内部工具：由 PkXmlStreamReader 在遇到 StartElement 时填充，不是 Qt
    // 兼容表面的一部分（同 PkXmlNodeList::PkAppend 的做法）。
    void PkAppend(const PkString &name, const PkString &value)
    {
        _attrs.push_back(Attribute{name, value});
    }

private:
    std::vector<Attribute> _attrs;
};
