/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef NodeTypeCheck_H
#define NodeTypeCheck_H

#include "KisExportCheckRegistry.h"
#include <KoID.h>
#include <kis_image.h>
#include <kis_count_visitor.h>

class NodeTypeCheck : public KisExportCheckBase
{
public:

    NodeTypeCheck(const PkString &nodeType, const PkString &description, const PkString &id, Level level, const PkString &customWarning = PkString())
        : KisExportCheckBase(id, level, customWarning, true)
        , m_nodeType(nodeType)
    {
        if (customWarning.isEmpty()) {
            m_warning = PkString("The image contains layers of unsupported type <b>%1</b>. Only the rendered result will be saved.").arg(description);
        }
    }

    bool checkNeeded(KisImageSP image) const override
    {
        PkStringList nodetypes = PkStringList() << m_nodeType;
        KoProperties props;
        KisCountVisitor v(nodetypes, props);
        image->rootLayer()->accept(v);

        // There is always one group layer, the root layer.
        if (m_nodeType == "KisGroupLayer") {
            return (v.count() > 1);
        }
        else {
            return (v.count() > 0);
        }
    }

    Level check(KisImageSP /*image*/) const override
    {
        return m_level;
    }

    PkString m_nodeType;
};

class NodeTypeCheckFactory : public KisExportCheckFactory
{
public:

    NodeTypeCheckFactory(const PkString &nodeType, const PkString &description)
        : m_nodeType(nodeType)
        , m_description(description)
    {}

    ~NodeTypeCheckFactory() override {}

    KisExportCheckBase *create(KisExportCheckBase::Level level, const PkString &customWarning) override
    {
        return new NodeTypeCheck(m_nodeType, m_description, id(), level, customWarning);
    }

    PkString id() const override {
        return PkString("NodeTypeCheck/") + m_nodeType;
    }

    PkString m_nodeType;
    PkString m_description;

};

#endif // NodeTypeCheck_H
