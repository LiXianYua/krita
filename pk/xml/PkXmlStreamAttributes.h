#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "../string/PkString.h"

// PkXmlStreamAttributes —— QXmlStreamAttributes 的零 Qt 对应物：一个元素起始
// 标签上全部属性的只读快照，由 PkXmlStreamReader::attributes() 在遇到
// StartElement 时构造。头文件里写全——只是对内部 std::vector 的直通存取，
// 没有需要摊到 .cpp 里的逻辑（同 PkXmlNodeList.h 的做法）。
//
// count()/value() 是 brief 的 Interfaces 列出的接口；hasAttribute() 是实现前
// 对真实调用点复核用量表时发现的缺口——`libs/pigment/resources/KoColorSet.cpp`
// （`colorProperties.hasAttribute("RGB")` 等 2 处）与
// `plugins/assistants/Assistants/kis_painting_assistant.cpp`（`xml.attributes()
// .hasAttribute(...)` 共 5 处调用——4 行代码，其中一行 `hasAttribute
// ("editorWidgetOffset_X") && hasAttribute("editorWidgetOffset_Y")` 一行两次
// 调用）都有对 QXmlStreamAttributes 直接调用 hasAttribute() 的真实用例，brief
// 的 Interfaces 枚举漏列了它（用量表 §1 的 `hasAttribute | 108` 一行全部记在
// PkXmlElement 名下，疑似把这几处 QXmlStreamAttributes 的调用点也并进了那个
// 计数——按 CLAUDE.md「新发现的缺口直接补进来」原则补上，不是自创接口）。
// **真实调用点总数 2+5=7**（R-07 全分支终审 I5 现场核实统一，见
// `pk/xml/README.md` 已知偏离清单——此前 README 记「2 处」、这里的旧注释记
// 「kis_painting_assistant.cpp 还有 4 处」，两处数字互相矛盾，都不对，
// 已统一为本条注释里逐处核实过的 7）。
//
// R-07 全分支终审 I4 新增 `at(int)` + `Attribute::name()`/`value()` 方法：真实
// 调用点 `libs/flake/text/KoSvgTextShapeMarkupConverter.cpp:879/881/883`
// 写的是 `elementAttributes.at(a).name() != "style"`
// （`.at(a).name()`/`.at(a).value()` 下标+方法调用形态），与此前只有字段
// （`Attribute::name`/`Attribute::value`，无下标 `at()`）的形状不兼容，编不过
// ——本文件旧版头注释「没有出现下标 [i]/range-for 遍历的写法」因此也不准确，
// 已改正（见下）。`Attribute` 的字段与同名方法在 C++ 里不能共存（同一作用域
// 不能有同名的非静态数据成员与成员函数），已核实 pk/xml 内外没有任何代码/
// 测试依赖 `Attribute::name`/`Attribute::value` 字段直接访问（只有本文件内部
// 用过，已一并改成走新增的方法），因此把内部存储字段改为私有的
// `_name`/`_value`，公开面换成 `name()`/`value()` 方法（与真 Qt
// `QXmlStreamAttribute` 的形状一致——它本来就没有公开字段，只有方法）。
class PkXmlStreamAttributes
{
public:
    class Attribute {
    public:
        Attribute() = default;
        Attribute(PkString name, PkString value)
            : _name(std::move(name)), _value(std::move(value))
        {
        }

        PkString name() const { return _name; }
        PkString value() const { return _value; }

    private:
        PkString _name;
        PkString _value;
    };

    PkXmlStreamAttributes() = default;

    PkString value(const PkString &name) const
    {
        for (const auto &a : _attrs) {
            if (a.name() == name) {
                return a.value();
            }
        }
        return PkString();
    }

    bool hasAttribute(const PkString &name) const
    {
        for (const auto &a : _attrs) {
            if (a.name() == name) {
                return true;
            }
        }
        return false;
    }

    int count() const { return static_cast<int>(_attrs.size()); }
    int size() const { return count(); } // QXmlStreamAttributes 是 QVector<QXmlStreamAttribute> 的别名，size() 同义转发

    // 迭代访问：真实调用点用的是 `.value(name)`/`.hasAttribute(name)`（多数），
    // 但 `KoSvgTextShapeMarkupConverter.cpp` 也有 `.at(a).name()`/`.value()`
    // 这种下标+方法形态（R-07 全分支终审 I4 现场核实）——`at()` 直接转发到
    // 原有的下标访问机制 `operator[]`（同 Qt `QVector::at()`/`operator[]`
    // 本来就是同一份存储的两个入口）。
    const Attribute &operator[](int i) const { return _attrs[static_cast<std::size_t>(i)]; }
    const Attribute &at(int i) const { return operator[](i); }
    std::vector<Attribute>::const_iterator begin() const { return _attrs.begin(); }
    std::vector<Attribute>::const_iterator end() const { return _attrs.end(); }

    // 内部工具：由 PkXmlStreamReader 在遇到 StartElement 时填充，不是 Qt
    // 兼容表面的一部分（同 PkXmlNodeList::PkAppend 的做法）。
    void PkAppend(const PkString &name, const PkString &value)
    {
        _attrs.push_back(Attribute(name, value));
    }

private:
    std::vector<Attribute> _attrs;
};
