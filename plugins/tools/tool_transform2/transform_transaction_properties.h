/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TRANSFORM_TRANSACTION_PROPERTIES_H
#define __TRANSFORM_TRANSACTION_PROPERTIES_H

#include <PkRect.h>
#include <PkPoint.h>
#include "kis_node.h"
#include "kis_layer_utils.h"
#include "kis_external_layer_iface.h"
#include "kis_transform_mask.h"

class ToolTransformArgs;

class TransformTransactionProperties
{
public:
    TransformTransactionProperties()
    {
    }

TransformTransactionProperties(const PkRectF &originalRect,
                               ToolTransformArgs *currentConfig,
                               KisNodeList rootNodes,
                               const PkList<KisNodeSP> &transformedNodes)
        : m_originalRect(originalRect),
          m_currentConfig(currentConfig),
          m_rootNodes(rootNodes),
          m_transformedNodes(transformedNodes),
          m_shouldAvoidPerspectiveTransform(false),
          m_boundsRotationAllowed(true)
    {
        m_hasInvisibleNodes = false;
        for (const KisNodeSP &node : transformedNodes) {
            if (KisExternalLayer *extLayer = dynamic_cast<KisExternalLayer*>(node.data())) {
                if (!extLayer->supportsPerspectiveTransform()) {
                    m_shouldAvoidPerspectiveTransform = true;
                    break;
                }
            }
            if (dynamic_cast<const KisTransformMask*>(node.data())) {
                m_boundsRotationAllowed = false;
            }

            m_hasInvisibleNodes |= !node->visible(false);
        }
    }

    qreal originalHalfWidth() const {
        return m_originalRect.width() / 2.0;
    }

    qreal originalHalfHeight() const {
        return m_originalRect.height() / 2.0;
    }

    PkRectF originalRect() const {
        return m_originalRect;
    }

    PkPointF originalCenterGeometric() const {
        return m_originalRect.center();
    }

    PkPointF originalTopLeft() const {
        return m_originalRect.topLeft();
    }

    PkPointF originalBottomLeft() const {
        return m_originalRect.bottomLeft();
    }

    PkPointF originalBottomRight() const {
        return m_originalRect.bottomRight();
    }

    PkPointF originalTopRight() const {
        return m_originalRect.topRight();
    }

    PkPointF originalMiddleLeft() const {
        return PkPointF(m_originalRect.left(), (m_originalRect.top() + m_originalRect.bottom()) / 2.0);
    }

    PkPointF originalMiddleRight() const {
        return PkPointF(m_originalRect.right(), (m_originalRect.top() + m_originalRect.bottom()) / 2.0);
    }

    PkPointF originalMiddleTop() const {
        return PkPointF((m_originalRect.left() + m_originalRect.right()) / 2.0, m_originalRect.top());
    }

    PkPointF originalMiddleBottom() const {
        return PkPointF((m_originalRect.left() + m_originalRect.right()) / 2.0, m_originalRect.bottom());
    }

    PkPoint originalTopLeftAligned() const {
        return m_originalRect.toAlignedRect().topLeft();
    }

    PkPoint originalBottomRightAligned() const {
        return m_originalRect.toAlignedRect().bottomRight();
    }

    ToolTransformArgs* currentConfig() const {
        return m_currentConfig;
    }

    KisNodeList rootNodes() const {
        return m_rootNodes;
    }

    KisNodeList transformedNodes() const {
        return m_transformedNodes;
    }

    qreal basePreviewOpacity() const {
        // Todo: this doesn't work for multiple layers
        return 0.9 * qreal(m_rootNodes[0]->opacity()) / 255.0;
    }

    const PkPolygonF convexHull() {
        return m_convexHull;
    }

    bool convexHullHasBeenRequested() {
        return m_convexHullHasBeenRequested;
    }

    bool shouldAvoidPerspectiveTransform() const {
        return m_shouldAvoidPerspectiveTransform;
    }

    bool hasInvisibleNodes() const {
        return m_hasInvisibleNodes;
    }

    bool boundsRotationAllowed() const {
        return m_boundsRotationAllowed;
    }

    void setCurrentConfigLocation(ToolTransformArgs *config) {
        m_currentConfig = config;
    }

    void setConvexHull(const PkPolygonF &hull) {
        m_convexHull = hull;
    }

    void setConvexHullHasBeenRequested(bool b) {
        m_convexHullHasBeenRequested = b;
    }

private:
    /**
     * Information about the original selected rect
     * (before any transformations)
     */
    PkRectF m_originalRect;
    ToolTransformArgs *m_currentConfig {0};
    KisNodeList m_rootNodes;
    KisNodeList m_transformedNodes;
    PkPolygonF m_convexHull;
    bool m_convexHullHasBeenRequested {false};
    bool m_shouldAvoidPerspectiveTransform {false};
    bool m_hasInvisibleNodes {false};
    bool m_boundsRotationAllowed {true};
};

#endif /* __TRANSFORM_TRANSACTION_PROPERTIES_H */
