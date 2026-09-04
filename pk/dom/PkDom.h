#ifndef PK_DOM_H
#define PK_DOM_H

// PkDom —— QDom 的零 Qt 替代（R-50）。
// 覆盖 text/svg 实测用量：QDomDocument / QDomElement / QDomNode / QDomText /
// QDomNodeList / QDomAttr。内部树结构自持；setContent 解析后续接 pugixml
// （避免与 R-07/R-25 的 pk/xml 锁重叠，见 tasks.yaml R-50 注释）。
//
// 所有权模型抄 Qt：QDomDocument 持有整棵树，元素/文本节点是 值语义的 句柄，
// 但底层节点由 document 拥有（Qt 实际是隐式共享 + document 生命周期）。
// 本实现用裸树 + document 拥有，句柄持有裸指针。

#include <string>
#include <vector>
#include <map>

class PkDomNode;
class PkDomElement;
class PkDomText;
class PkDomDocument;
class PkDomNodeList;
class PkDomAttr;

class PkDomNode {
public:
    enum NodeType {
        ElementNode, AttributeNode, TextNode, CDATASectionNode,
        EntityReferenceNode, EntityNode, ProcessingInstructionNode,
        CommentNode, DocumentNode, DocumentTypeNode, DocumentFragmentNode,
        NotationNode, BaseNode = ElementNode
    };

    // document 持有整棵树：基类析构递归释放子节点（元素/文本均 new 自 document）。
    virtual ~PkDomNode() { for (PkDomNode *c : m_children) delete c; }

    NodeType nodeType() const { return m_type; }
    std::string nodeName() const { return m_nodeName; }

    PkDomNode *parentNode() const { return m_parent; }
    PkDomNode *firstChild() const { return m_children.empty() ? nullptr : m_children.front(); }
    PkDomNode *lastChild() const { return m_children.empty() ? nullptr : m_children.back(); }
    PkDomNode *nextSibling() const { return m_next; }
    PkDomNode *previousSibling() const { return m_prev; }

    PkDomNodeList childNodes() const;

    // 递归拼接全部后代文本（对齐 QDomElement::text() / QDomNode::text()）。
    std::string text() const;

    PkDomElement *toElement();   // 定义在 PkDom.cpp（需 PkDomElement 完整）
    PkDomText *toText();         // 定义在 PkDom.cpp（需 PkDomText 完整）

    // 子树增删（document 拥有节点生命周期）
    PkDomNode *appendChild(PkDomNode *n);
    PkDomNode *insertBefore(PkDomNode *n, PkDomNode *ref);
    PkDomNode *removeChild(PkDomNode *n);

protected:
    PkDomNode(NodeType t, const std::string &name, PkDomDocument *doc)
        : m_type(t), m_nodeName(name), m_doc(doc) {}

    NodeType m_type = ElementNode;
    std::string m_nodeName;
    PkDomDocument *m_doc = nullptr;
    PkDomNode *m_parent = nullptr;
    PkDomNode *m_next = nullptr;
    PkDomNode *m_prev = nullptr;
    std::vector<PkDomNode *> m_children;

    friend class PkDomDocument;
};

class PkDomText : public PkDomNode {
public:
    explicit PkDomText(PkDomDocument *doc, const std::string &data = std::string())
        : PkDomNode(TextNode, "#text", doc), m_data(data) {}

    std::string data() const { return m_data; }
    void setData(const std::string &d) { m_data = d; }

private:
    std::string m_data;
};

class PkDomAttr {
public:
    PkDomAttr() = default;
    PkDomAttr(const std::string &name, const std::string &val) : m_name(name), m_value(val) {}
    std::string name() const { return m_name; }
    std::string value() const { return m_value; }
    void setValue(const std::string &v) { m_value = v; }
private:
    std::string m_name, m_value;
};

class PkDomElement : public PkDomNode {
public:
    explicit PkDomElement(PkDomDocument *doc, const std::string &tag = std::string())
        : PkDomNode(ElementNode, tag, doc) {}

    std::string tagName() const { return m_nodeName; }
    void setTagName(const std::string &t) { m_nodeName = t; }

    bool hasAttribute(const std::string &n) const { return m_attrs.count(n) != 0; }
    std::string attribute(const std::string &n, const std::string &def = std::string()) const {
        auto it = m_attrs.find(n); return it == m_attrs.end() ? def : it->second.value();
    }
    void setAttribute(const std::string &n, const std::string &v) {
        m_attrs[n] = PkDomAttr(n, v);
    }
    void removeAttribute(const std::string &n) { m_attrs.erase(n); }

    PkDomNodeList elementsByTagName(const std::string &tag) const;

private:
    std::map<std::string, PkDomAttr> m_attrs;
};

class PkDomNodeList {
public:
    std::size_t length() const { return m_nodes.size(); }
    PkDomNode *item(std::size_t i) const { return i < m_nodes.size() ? m_nodes[i] : nullptr; }
    PkDomElement *at(std::size_t i) const {
        PkDomNode *n = item(i); return n ? n->toElement() : nullptr;
    }
    void push_back(PkDomNode *n) { m_nodes.push_back(n); }
private:
    std::vector<PkDomNode *> m_nodes;
};

#endif // PK_DOM_H
