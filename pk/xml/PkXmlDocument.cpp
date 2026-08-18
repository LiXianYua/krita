#include "PkXmlDocument.h"

#include <sstream>
#include <string>
#include <vector>

// R-25 Task 2：字节导向 setContentImpl 需要的完整定义（头文件只前置声明）。
#include "../port/PkStream.h"
#include "PkXmlOffsetUtil.h"
#include "PkXmlStreamReader.h"

PkXmlDocument::PkXmlDocument()
    : PkXmlNode()
{
    // R-25 Task 3：make_shared<PkXmlDocRoot>()（不再是裸 pugi::xml_document）
    // ——PkXmlDocRoot 公开继承 pugi::xml_document 只多一个 `source` 字段，见
    // PkXmlNode.h 顶部注释。
    _doc = std::make_shared<PkXmlDocRoot>();
    _node = *_doc;
    _kind = Kind::Node;
}

PkXmlDocument::PkXmlDocument(const PkString &docName)
    : PkXmlDocument()
{
    // Qt: QDomDocument(const QString &name) 创建文档的同时设好 doctype 的
    // name（publicId/systemId 留空）——真实调用点没有用到这个构造函数带出的
    // doctype 的 publicId/systemId（用量表没出现），只存 name 即可对齐。
    if (!docName.isEmpty()) {
        pugi::xml_node dt = _doc->append_child(pugi::node_doctype);
        dt.set_value(docName.PkToUtf8().c_str());
    }
}

pugi::xml_node PkXmlDocument::ensureLimboNode()
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (pkIsXmlLimboNode(c)) {
            return c;
        }
    }
    pugi::xml_node limbo = _doc->append_child(pugi::node_element);
    limbo.set_name(kPkXmlLimboTag);
    return limbo;
}

PkXmlElement PkXmlDocument::createElement(const PkString &tagName)
{
    // 创建即挂树：见 PkXmlNode.h 顶部注释——新节点立刻挂进 limbo 容器（而不是
    // 真正悬空的，也不是文档根的直接子节点），appendChild() 用 append_move()
    // 在同一棵树内部把它从 limbo 搬到真正的目标位置。
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_element);
    n.set_name(tagName.PkToUtf8().c_str());
    return PkXmlElement(n, _doc);
}

PkXmlText PkXmlDocument::createTextNode(const PkString &value)
{
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_pcdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlText(n, _doc);
}

PkXmlCDATASection PkXmlDocument::createCDATASection(const PkString &value)
{
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_cdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlCDATASection(n, _doc);
}

PkXmlElement PkXmlDocument::documentElement() const
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        // limbo 容器本身也是 node_element 类型，必须先排除，否则孤儿节点
        // 堆积多了之后 limbo 会被误当成"第一个元素子节点"返回——见 R-07 全
        // 分支终审 I1，PkXmlNode.h 顶部类注释。
        if (c.type() == pugi::node_element && !pkIsXmlLimboNode(c)) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

bool PkXmlDocument::setContent(const PkString &xml, PkString *errorMsg, int *errorLine,
                                int *errorColumn)
{
    return setContent(xml, false, errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContent(const PkString &xml, bool namespaceProcessing, PkString *errorMsg,
                                int *errorLine, int *errorColumn)
{
    // pugixml 本身不做命名空间解析（探针 P4 之后的关键实现说明 5）——两个重载
    // 走同一套 parse_default 解析流程，namespaceProcessing 参数只是为了签名
    // 对齐 Qt；命名空间处理在 PkXmlElement::localName()/namespaceURI() 里实现。
    (void)namespaceProcessing;
    return setContentImpl(xml, pugi::parse_default | pugi::parse_doctype, errorMsg, errorLine,
                           errorColumn);
}

bool PkXmlDocument::setContentPreservingWhitespace(const PkString &xml, bool namespaceProcessing,
                                                    PkString *errorMsg, int *errorLine,
                                                    int *errorColumn)
{
    // I2（R-07 全分支终审）：额外加 parse_ws_pcdata——普通 setContent()（P1
    // 探针对齐 Qt 丢弃纯空白文本节点）不适用于"空白有语义"的真实调用点
    // （SvgParser.cpp，见 README 已知偏离清单 I2）。
    (void)namespaceProcessing;
    return setContentImpl(xml, pugi::parse_default | pugi::parse_doctype | pugi::parse_ws_pcdata,
                           errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContentImpl(const PkString &xml, unsigned int parseFlags,
                                    PkString *errorMsg, int *errorLine, int *errorColumn)
{
    // R-25 Task 2：转成 UTF-8 字节后转调字节导向版本，避免重复"reset() →
    // load_buffer → 成功/失败分支 → 行列换算"这段逻辑。这条路径本来就是先把
    // PkString 转 UTF-8 再解析（真实调用点都是"我手上已经是 PkString"的场景，
    // 编码早已确定），跟字节导向版本"跳过 PkString 中转、保留 pugixml 自己的
    // 编码自动探测"的设计意图不冲突——两者服务不同的输入形态。
    const std::string utf8 = xml.PkToUtf8();
    return setContentImpl(utf8.data(), utf8.size(), parseFlags, errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContentImpl(const char *data, std::size_t size, unsigned int parseFlags,
                                    PkString *errorMsg, int *errorLine, int *errorColumn)
{
    // 重复调用 setContent 要能覆盖上一次的树（Qt 语义），reset() 之后同一个
    // shared_ptr<pugi::xml_document> 重新 load——旧的借出句柄语义上失效，与
    // Qt 的"整棵树被替换"效果一致。这也会连带清掉任何还留在 limbo 里、尚未
    // appendChild 的孤儿节点——与 Qt 语义一致（没有等价概念，reset 即可）。
    _doc->reset();
    // parse_default 不含 parse_doctype（pugixml 默认丢弃 DOCTYPE 声明，见
    // src/pugixml.hpp parse_full 的定义）——补上它，否则 doctype() 永远找不到
    // 任何从 setContent 解析出来的 DOCTYPE 节点。已现场探针确认：`<!DOCTYPE
    // note SYSTEM "Note.dtd">` 解析后 node_doctype 的 value() 是整段原始文本
    // `note SYSTEM "Note.dtd"`，doctype() 取第一个空白分隔 token 当 name()。
    //
    // 探针 P14：直接把原始字节喂给 load_buffer，不经过 PkString 中转——
    // pugixml 自己的编码自动探测（encoding_auto）据此才能对 XML 声明里的
    // encoding（例如 UTF-8 多字节属性值）生效，PkString::PkFromUtf8 会假定
    // 输入已经是 UTF-8、丢掉这个探测能力。
    const pugi::xml_parse_result result = _doc->load_buffer(data, size, parseFlags);
    _node = *_doc;

    // R-25 Task 3：不管解析成功还是失败都把原始字节存一份进 `_doc->source`
    // ——lineNumber()/columnNumber()（PkXmlNode.cpp）要用它做 offset→行列
    // 换算；失败时也可能已经建出部分节点（pugixml 是"尽量往前解析到出错点"
    // 的策略），行列换算仍然可能用得到。
    _doc->source.assign(data, size);

    if (result) {
        return true;
    }

    if (errorMsg) {
        *errorMsg = pkXmlFromPugi(result.description());
    }
    if (errorLine || errorColumn) {
        // 探针 P4：Qt 的 errLine/errCol 是 1-based。pugixml 只给字节 offset，
        // 没有现成的行列号——换算逻辑重构进 PkXmlOffsetUtil.h（R-25 Task 3），
        // DOM 侧这里与 lineNumber()/columnNumber()、Stream 侧共用同一份实现，
        // 行为与重构前逐字节一致（已有测试必须继续绿）。
        if (errorLine) {
            *errorLine = pkXmlOffsetToLine(_doc->source, result.offset);
        }
        if (errorColumn) {
            *errorColumn = pkXmlOffsetToColumn(_doc->source, result.offset);
        }
    }
    return false;
}

bool PkXmlDocument::setContent(PkStream *device, PkString *errorMsg, int *errorLine,
                                int *errorColumn)
{
    return setContent(device, false, errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContent(PkStream *device, bool namespaceProcessing, PkString *errorMsg,
                                int *errorLine, int *errorColumn)
{
    // pugixml 本身不做命名空间解析（同 PkString 版本的 setContent 那条注释）
    // ——namespaceProcessing 参数只是为了签名对齐 Qt。
    (void)namespaceProcessing;

    if (!device) {
        if (errorMsg) {
            *errorMsg = PkString::PkFromUtf8("null device", 11);
        }
        return false;
    }

    // 探针 P14：从设备**当前位置**开始读到 EOF（不自己 seek(0)），EOF（返回
    // 0）不是错误。`read(char*, pk_int64)` 是 PkStream 唯一已定义、可用的
    // 读取入口——`readAll()` 只声明不定义，用了会链接期报错（见
    // pk/port/PkStream.h 头部"一个刻意的设计"）。
    std::string buf;
    char chunk[4096];
    while (true) {
        const PkStream::pk_int64 n =
            device->read(chunk, static_cast<PkStream::pk_int64>(sizeof(chunk)));
        if (n == 0) {
            break; // EOF
        }
        if (n < 0) {
            if (errorMsg) {
                *errorMsg = device->errorString();
            }
            return false;
        }
        buf.append(chunk, static_cast<std::size_t>(n));
    }

    return setContentImpl(buf.data(), buf.size(), pugi::parse_default | pugi::parse_doctype,
                           errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContent(PkXmlStreamReader *reader, bool namespaceProcessing,
                                PkString *errorMsg, int *errorLine, int *errorColumn)
{
    // 本重载不产出行列号——PkXmlStreamReader::lineNumber()/columnNumber() 是
    // R-25 Task 3 才交付的能力，Task 2 交付时还没有（本 commit 只做 Task 3 的
    // PkXmlDocRoot/offset 工具基础设施，lineNumber()/columnNumber() 的具体接线
    // 留到 Task 3 Stream 侧那个 commit，那时 PkXmlStreamReader::lineNumber()
    // 才存在）。真实调用点 SvgParser.cpp:201 的调用形状允许 errorLine/
    // errorColumn 为非空指针，这里保持指针形参对齐签名，但暂不写入值。
    (void)errorLine;
    (void)errorColumn;
    // pugixml 不做命名空间解析——签名对齐 Qt，不改变行为。
    (void)namespaceProcessing;

    if (!reader) {
        if (errorMsg) {
            *errorMsg = PkString::PkFromUtf8("null reader", 11);
        }
        return false;
    }

    // 有意打破 Task 1 定下的"DOM/Stream 两侧不互相消费对方类型"边界——见
    // README §11.3 / 计划设计①-b：把 PkXmlStreamReader 当 StAX token 源，
    // 驱动一个"流转 DOM"构建器。新建节点走现成的 limbo 容器机制
    // （createElement()/createTextNode()），跟正常 DOM 构建路径完全一致。
    _doc->reset();
    _node = *_doc;
    // 这条路径不经过 load_buffer（节点是程序创建、从 limbo 挂树的，见上），
    // 没有单一原始字节 buffer 可言——`_doc->source` 保持空，后续对这些节点
    // 调用 lineNumber()/columnNumber() 会因为 offset_debug() 恒为 -1（未解析
    // 节点）自然短路返回 -1，不需要额外判断。清空是为了不让上一次
    // setContent(PkString/PkStream) 留下的旧 source 误导（虽然 offset_debug()
    // 的短路已经足够安全，这里仍然显式清空以保持"source 反映当前树"的不变量）。
    _doc->source.clear();

    // 游标栈：stack.back() 是"下一个节点该挂到谁身上"。初始只有文档自身一层
    // （*this 隐式转换成 PkXmlNode——PkXmlDocument 继承自 PkXmlNode，appendChild
    // 是继承来的公开方法，对文档自己调用等价于 Qt `QDomDocument::appendChild`）。
    std::vector<PkXmlNode> stack;
    stack.push_back(*this);

    while (true) {
        const PkXmlStreamReader::TokenType t = reader->readNext();
        if (t == PkXmlStreamReader::StartElement) {
            PkXmlElement el = createElement(reader->name());
            const PkXmlStreamAttributes attrs = reader->attributes();
            for (int i = 0; i < attrs.count(); ++i) {
                el.setAttribute(attrs.at(i).name(), attrs.at(i).value());
            }
            stack.back().appendChild(el);
            stack.push_back(el);
        } else if (t == PkXmlStreamReader::Characters) {
            PkXmlText text = createTextNode(reader->text());
            stack.back().appendChild(text);
        } else if (t == PkXmlStreamReader::EndElement) {
            if (stack.size() > 1) { // 防御性：不弹出最底层的文档自身
                stack.pop_back();
            }
        } else if (t == PkXmlStreamReader::EndDocument || t == PkXmlStreamReader::Invalid) {
            break;
        }
        // StartDocument/NoToken：不建节点，继续循环。
    }

    if (reader->hasError()) {
        if (errorMsg) {
            *errorMsg = reader->errorString();
        }
        return false;
    }
    return true;
}

PkXmlNode PkXmlDocument::importNode(const PkXmlNode &importedNode, bool deep)
{
    // 探针 P13：传入 null 节点返回 null。
    if (importedNode.isNull()) {
        return PkXmlNode();
    }

    const pugi::xml_node src = pkRawNode(importedNode);

    if (deep) {
        // pugixml 的 append_copy() 本来就是深拷贝（含全部子树 + 属性），
        // 天然覆盖这个分支——探针 P13 确认的 Qt 语义正是"深拷贝一份改了
        // 身份的新副本"，跟 append_copy() 是同一件事，不需要额外代码。
        pugi::xml_node n = ensureLimboNode().append_copy(src);
        return PkXmlNode(n, _doc);
    }

    // deep == false：手工建同类型同名节点，只拷贝属性，不递归拷贝子节点。
    pugi::xml_node n = ensureLimboNode().append_child(src.type());
    n.set_name(src.name());
    // set_value() 对 node_element 是无操作（pugixml 只对 pcdata/cdata/
    // comment/pi/doctype 类型生效），对 node_pcdata/node_cdata 才有意义
    // ——两种情形都安全调用，不需要按类型分支。
    n.set_value(src.value());
    for (pugi::xml_attribute a = src.first_attribute(); a; a = a.next_attribute()) {
        pugi::xml_attribute newAttr = n.append_attribute(a.name());
        newAttr.set_value(a.value());
    }
    return PkXmlNode(n, _doc);
}

PkString PkXmlDocument::toString(int indent) const
{
    // 探针 P3：indent >= 0 时结尾带一个 '\n'（format_indent 天然产出）；
    // indent < 0（含默认值 1 判定用的是 `< 0`，不是 `== -1`——但默认值 1 走的
    // 是缩进分支）走单行紧凑模式（format_raw）且无结尾换行。
    // format_no_declaration：Qt 的 toString() 不输出 `<?xml ?>` 声明（即便
    // setContent 解析到了声明，Qt 也不保留/不回显它）。
    unsigned int flags = pugi::format_no_declaration;
    std::string indentStr;
    if (indent < 0) {
        flags |= pugi::format_raw;
    } else {
        flags |= pugi::format_indent;
        indentStr.assign(static_cast<std::size_t>(indent), ' ');
    }

    // limbo 容器（R-07 全分支终审 I1）里挂着的孤儿节点不是真实文档结构的
    // 一部分，不能出现在序列化输出里——pugixml 没有"临时隐藏子节点再复原"的
    // API（remove_child 会真正释放内存，不可逆），深拷贝一份跳过 limbo 的
    // 临时文档再 save() 是最简单可靠的做法：只多一次 O(n) 深拷贝，换来不用
    // 碰活树的结构。
    pugi::xml_document tmp;
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (pkIsXmlLimboNode(c)) {
            continue;
        }
        tmp.append_copy(c);
    }

    std::ostringstream oss;
    tmp.save(oss, indentStr.c_str(), flags, pugi::encoding_utf8);
    const std::string s = oss.str();
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

PkString PkXmlDocument::toByteArray(int indent) const
{
    // 已知偏离，见头文件类注释：PkByteArray 尚未交付，退化成 UTF-8 编码的
    // PkString，行为等价于 toString()。
    return toString(indent);
}

PkXmlDocumentType PkXmlDocument::doctype() const
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_doctype) {
            const std::string raw = c.value();
            const std::size_t sp = raw.find_first_of(" \t\r\n");
            const std::string name = (sp == std::string::npos) ? raw : raw.substr(0, sp);
            return PkXmlDocumentType(c, _doc,
                                      PkString::PkFromUtf8(name.c_str(),
                                                            static_cast<int>(name.size())));
        }
    }
    return PkXmlDocumentType();
}
