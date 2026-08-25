/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_node_query_path.h"

#include <PkStringList.h>
#include <kis_node.h>
#include <kis_image.h>
#include <kis_paint_device.h>

struct PathElement {
    enum Type {
        Wildcard,
        Parent,
        Index
    };
    PathElement(Type _type) : type(_type) {
        Q_ASSERT(type == Wildcard || type == Parent);
    }
    PathElement(int _i) : type(Index), index(_i) {}
    Type type;
    unsigned int index {0};
};

struct Q_DECL_HIDDEN KisNodeQueryPath::Private {
    PkList<PathElement> elements;
    bool relative;
    /// This function will remove unneeded call to parent, for instance, "1/../3/../5" => "5"
    void simplifyPath() {
        // No elements then return
        if (elements.isEmpty()) return;
        PkList<PathElement> newelements;
        int i = 0;
        for (; i < elements.count() && elements[i].type == PathElement::Parent; ++i) {
            newelements.push_back(PathElement::Parent);
        }
        // Loop over the element of the list
        for (; i < elements.count(); ++i) {
            PathElement pe = elements[i];
            // If it's the last element, or the next element isn't a parent
            if (pe.type != PathElement::Parent) {
                newelements.push_back(pe);
            } else {
                if (newelements.isEmpty() || newelements.last().type == PathElement::Parent) {
                    newelements.push_back(PathElement::Parent);
                } else {
                    newelements.removeLast();
                }
            }
        }
        // Set the new list
        elements = newelements;
    }
    void queryLevel(int _level, KisNodeSP _node, PkList<KisNodeSP>& _result) {
        if (_level >= elements.size()) {
            _result.push_back(_node);
        } else {
            PathElement pe = elements[_level];

            switch (pe.type) {
            case PathElement::Wildcard: {
                for (KisNodeSP child = _node->firstChild();
                        child != 0; child = child->nextSibling()) {
                    queryLevel(_level + 1, child, _result);
                }
            }
            break;
            case PathElement::Parent: {
                if (_node->parent()) {
                    queryLevel(_level + 1, _node->parent(), _result);
                } else {
                    dbgKrita << "No parent";
                }
                break;
            }
            case PathElement::Index: {
                if (pe.index < _node->childCount()) {
                    queryLevel(_level + 1, _node->at(pe.index), _result);
                } else {
                    dbgKrita << "No parent";
                }
                break;
            }
            }
        }
    }
};

KisNodeQueryPath::KisNodeQueryPath() : d(new Private)
{
}

KisNodeQueryPath::~KisNodeQueryPath()
{
    delete d;
}

KisNodeQueryPath::KisNodeQueryPath(const KisNodeQueryPath& nqp) : d(new Private(*nqp.d))
{
}

KisNodeQueryPath& KisNodeQueryPath::operator=(const KisNodeQueryPath & nqp)
{
    *d = *nqp.d;
    return *this;
}

bool KisNodeQueryPath::isRelative() const
{
    return d->relative;
}


PkList<KisNodeSP> KisNodeQueryPath::queryNodes(KisImageWSP image, KisNodeSP currentNode) const
{
    KisNodeSP _node;
    if (d->relative) {
        _node = currentNode;
    } else {
        _node = image->root();
    }

    PkList<KisNodeSP> result;

    d->queryLevel(0, _node, result);

    return result;
}

KisNodeSP KisNodeQueryPath::queryUniqueNode(KisImageWSP image, KisNodeSP currentNode) const
{
    PkList<KisNodeSP> result = queryNodes(image, currentNode);
    KIS_ASSERT_RECOVER_NOOP(result.size() <= 1);

    return !result.isEmpty() ? result.first() : 0;
}

PkString KisNodeQueryPath::toString() const
{
    PkString str;
    if (!d->relative) {
        str = PkString("/");
    } else if (d->elements.count() == 0) {
        return PkString(".");
    }
    for (int i = 0; i < d->elements.count(); ++i) {
        PathElement pe = d->elements[i];
        switch (pe.type) {
        case PathElement::Wildcard:
            str += PkString("*");
            break;
        case PathElement::Parent:
            str += "..";
            break;
        case PathElement::Index:
            // 剥离修正：原文数字格式化(pe.index)。PkString().arg(int) 语义
            // 是替换占位符（空串无占位符 → 返回空），须带 "%1" 占位符（与
            // kis_cubic_curve.cpp 同款写法）。
            str += PkString("%1").arg(static_cast<int>(pe.index));
            break;
        }
        if (i != d->elements.count() - 1) {
            str += PkString("/");
        }
    }
    return str;
}

KisNodeQueryPath KisNodeQueryPath::fromString(const PkString& _path)
{
    KisNodeQueryPath path;
    if (_path.size() == 0 || _path == ".") {
        path.d->relative = true;
        return path;
    }
    if (_path == "/") {
        path.d->relative = false;
        return path;
    }
    path.d->relative = !(_path.at(0) == '/');
    std::vector<PkString> indexes = _path.split('/');
    if (!path.d->relative) {
        indexes.erase(indexes.begin()); // In case of an absolute path "/1/2", the list is "", "1", "2" which is not good
    }
    for (const PkString& index : indexes) {
        if (index == "*") {
            path.d->elements.push_back(PathElement::Wildcard);
        } else if (index == "..") {
            path.d->elements.push_back(PathElement::Parent);
        } else {
            path.d->elements.push_back(index.toInt());
        }
    }
    path.d->simplifyPath();
    return path;
}

KisNodeQueryPath KisNodeQueryPath::absolutePath(KisNodeSP node)
{
    KisNodeQueryPath path;
    path.d->relative = false;
    KisNodeSP parent = 0;
    while ((parent = node->parent())) {
        int index = parent->index(node);
        if (index >= 0) {
            path.d->elements.push_front(index);
        }
        node = parent;
    }
    return path;
}


