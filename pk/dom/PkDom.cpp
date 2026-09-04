#include "PkDom.h"

PkDomElement *PkDomNode::toElement() {
    return (m_type == ElementNode) ? static_cast<PkDomElement *>(this) : nullptr;
}
PkDomText *PkDomNode::toText() {
    return (m_type == TextNode) ? static_cast<PkDomText *>(this) : nullptr;
}

std::string PkDomNode::text() const {
    std::string r;
    for (PkDomNode *c = firstChild(); c; c = c->nextSibling()) {
        if (PkDomText *t = c->toText()) r += t->data();
        else if (c->toElement()) r += c->text();   // 递归进入子元素
    }
    return r;
}

PkDomNodeList PkDomNode::childNodes() const {
    PkDomNodeList l;
    for (PkDomNode *c : m_children) l.push_back(c);
    return l;
}

PkDomNode *PkDomNode::appendChild(PkDomNode *n) {
    if (!n) return nullptr;
    n->m_parent = this;
    if (!m_children.empty()) {
        m_children.back()->m_next = n;
        n->m_prev = m_children.back();
    }
    m_children.push_back(n);
    return n;
}

PkDomNode *PkDomNode::insertBefore(PkDomNode *n, PkDomNode *ref) {
    if (!n) return nullptr;
    n->m_parent = this;
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (*it == ref) {
            n->m_prev = (it == m_children.begin()) ? nullptr : *(it - 1);
            n->m_next = *it;
            if (n->m_prev) n->m_prev->m_next = n;
            (*it)->m_prev = n;
            m_children.insert(it, n);
            return n;
        }
    }
    return appendChild(n);
}

PkDomNode *PkDomNode::removeChild(PkDomNode *n) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (*it == n) {
            if (n->m_prev) n->m_prev->m_next = n->m_next;
            if (n->m_next) n->m_next->m_prev = n->m_prev;
            m_children.erase(it);
            n->m_parent = nullptr;
            return n;
        }
    }
    return nullptr;
}
