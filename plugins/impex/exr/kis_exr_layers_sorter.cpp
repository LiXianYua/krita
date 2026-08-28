/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_exr_layers_sorter.h"

#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include "kis_image.h"
#include "exr_extra_tags.h"
#include "kis_kra_savexml_visitor.h"
#include "kis_paint_layer.h"


struct KisExrLayersSorter::Private
{
    Private(const PkXmlDocument &_extraData, KisImageSP _image)
        : extraData(_extraData), image(_image) {}

    const PkXmlDocument &extraData;
    KisImageSP image;

    PkMap<PkString, PkXmlElement> pathToElementMap;
    PkMap<PkString, int> pathToOrderingMap;

    PkMap<KisNodeSP, int> nodeToOrderingMap;

    void createOrderingMap();
    void processLayers(KisNodeSP root);
    void sortLayers(KisNodeSP root);
};

PkString getNodePath(KisNodeSP node) {
    KIS_ASSERT_RECOVER(node) { return "UNDEFINED"; }

    PkString path;

    KisNodeSP parentNode = node->parent();
    while(parentNode) {
        if (!path.isEmpty()) {
            path = PkString(".") + path;
        }
        path = node->name() + path;

        node = parentNode;
        parentNode = node->parent();
    }

    return path;
}

void KisExrLayersSorter::Private::createOrderingMap()
{
    int index = 0;
    PkXmlElement el = extraData.documentElement().firstChildElement();


    while (!el.isNull()) {
        PkString path = el.attribute(EXR_NAME);
        pathToElementMap.insert(path, el);
        pathToOrderingMap.insert(path, index);

        el = el.nextSiblingElement();
        index++;
    }
}

template <typename T>
T fetchMapValueLazy(const PkMap<PkString, T> &map, PkString path)
{
    if (map.contains(path)) return map[path];


    typename PkMap<PkString, T>::const_iterator it = map.constBegin();
    typename PkMap<PkString, T>::const_iterator end = map.constEnd();

    for (; it != end; ++it) {
        if (it.key().startsWith(path)) {
            return it.value();
        }
    }

    return T();
}

void KisExrLayersSorter::Private::processLayers(KisNodeSP root)
{
    if (root && root->parent()) {
        PkString path = getNodePath(root);

        nodeToOrderingMap.insert(root, fetchMapValueLazy(pathToOrderingMap, path));

        if (KisPaintLayer *paintLayer = dynamic_cast<KisPaintLayer*>(root.data())) {
            PkXmlElement el = pathToElementMap[path];

            /**
             * In older files (before Krita 5.2.7) we used to save the layer offset
             * into the XML metadata, while loading the pixel data at the origin.
             * Since 5.2.7 we stopped saving offsets (we save null instead of them),
             * we let's just make sure we don't load garbage from older files).
             *
             * Hence, we pass `false` for loading the offset
             */
            KisSaveXmlVisitor::loadPaintLayerAttributes(el, paintLayer, false);
        }
    }

    KisNodeSP child = root->firstChild();
    while (child) {
        processLayers(child);
        child = child->nextSibling();
    }
}

struct CompareNodesFunctor
{
    CompareNodesFunctor(const PkMap<KisNodeSP, int> &map)
        : m_nodeToOrderingMap(map) {}

    bool operator() (KisNodeSP lhs, KisNodeSP rhs) {
        return m_nodeToOrderingMap[lhs] < m_nodeToOrderingMap[rhs];
    }

private:
    const PkMap<KisNodeSP, int> &m_nodeToOrderingMap;
};


void KisExrLayersSorter::Private::sortLayers(KisNodeSP root)
{
    PkList<KisNodeSP> childNodes;

    // first move all the children to the list
    KisNodeSP child = root->firstChild();
    while (child) {
        KisNodeSP lastChild = child;
        child = child->nextSibling();

        childNodes.append(lastChild);
        image->removeNode(lastChild);
    }

    // sort the list
    std::stable_sort(childNodes.begin(), childNodes.end(), CompareNodesFunctor(nodeToOrderingMap));

    // put the children back
    for (KisNodeSP node : childNodes) {
        image->addNode(node, root, root->childCount());
    }

    // recursive calls
    child = root->firstChild();
    while (child) {
        sortLayers(child);
        child = child->nextSibling();
    }
}

KisExrLayersSorter::KisExrLayersSorter(const PkXmlDocument &extraData, KisImageSP image)
    : m_d(new Private(extraData, image))
{
    KIS_ASSERT_RECOVER_RETURN(!extraData.isNull());
    m_d->createOrderingMap();

    m_d->processLayers(image->root());
    m_d->sortLayers(image->root());
}

KisExrLayersSorter::~KisExrLayersSorter()
{
}
