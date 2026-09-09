/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_NODE_SHAPE_H_
#define KIS_NODE_SHAPE_H_

#include <QtMath>
#include <PkObject.h>
#include <PkConnection.h>

#include <KoShapeLayer.h>

#include <kritashapemodel_export.h>
#include <kis_types.h>


#define KIS_NODE_SHAPE_ID "KisNodeShape"

/**
 * A KisNodeShape is a flake wrapper around Krita nodes. It is used
 * for dealing with currently active node for tools.
 */
class KRITASHAPEMODEL_EXPORT KisNodeShape : public PkObject, public KoShapeLayer
{
public:
    KisNodeShape(KisNodeSP node);
    ~KisNodeShape() override;

    KisNodeSP node();

    // Empty implementations as the node is not painted anywhere
    PkSizeF size() const override;
    PkRectF boundingRect() const override;
    void setPosition(const PkPointF &) override;
    void paint(PkPainter &painter) const override;

private:
    void editabilityChanged();

private:
    PkConnection m_nodeChangedConnection;
    struct Private;
    Private * const m_d;
};

#endif
