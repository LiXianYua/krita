/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBatchNodeUpdate.h"

#include "kis_node.h"
#include "kis_layer_utils.h"
#include <PkRect.h>
#include <PkDebug.h>

KisBatchNodeUpdate::KisBatchNodeUpdate(const std::vector<std::pair<KisNodeSP, PkRect>> &rhs)
    : std::vector<std::pair<KisNodeSP, PkRect>>(rhs)
{

}

void KisBatchNodeUpdate::addUpdate(KisNodeSP node, const PkRect &rc)
{
    push_back(std::make_pair(node, rc));
}

void KisBatchNodeUpdate::compress()
{
    *this = compressed();
}

KisBatchNodeUpdate KisBatchNodeUpdate::compressed() const
{
    KisBatchNodeUpdate newUpdateData;

    KisNodeList rootNodes;

    std::transform(begin(), end(), std::back_inserter(rootNodes),
              [] (const std::pair<KisNodeSP, PkRect> &update) {return update.first; });

    rootNodes = KisLayerUtils::sortAndFilterMergeableInternalNodes(rootNodes, true);

    for (KisNodeSP root : rootNodes) {
        PkRect dirtyRect;

        for (auto it = begin(); it != end(); ++it) {
            if (it->first == root || KisLayerUtils::checkIsChildOf(it->first, {root})) {
                dirtyRect |= it->second;
            }
        }

        newUpdateData.push_back(std::make_pair(root, dirtyRect));
    }

    return newUpdateData;
}

KisBatchNodeUpdate &KisBatchNodeUpdate::operator|=(const KisBatchNodeUpdate &rhs)
{
    if (this == &rhs)
        return *this;

    reserve(size() + rhs.size());

    std::copy(rhs.begin(), rhs.end(), std::back_inserter(*this));
    std::sort(begin(), end(), [](const std::pair<KisNodeSP, PkRect> &lhs, const std::pair<KisNodeSP, PkRect> &rhs) { return lhs.first.data() < rhs.first.data(); });

    if (size() <= 1)
        return *this;

    for (auto prevIt = begin(), it = next(prevIt); it != end();) {
        if (prevIt->first == it->first) {
            prevIt->second |= it->second;
            it = erase(it);
        } else {
            ++prevIt;
            ++it;
        }
    }

    return *this;
}

PkDebug operator<<(PkDebug dbg, const KisBatchNodeUpdate &update)
{
    dbg.nospace() << "KisBatchNodeUpdate (";

    for (auto it = update.begin(); it != update.end(); ++it) {
        dbg.nospace() << it->first << "->" << it->second;

        if (next(it) != update.end()) {
            dbg.nospace() << "; ";
        }
    }

    return dbg;
}
