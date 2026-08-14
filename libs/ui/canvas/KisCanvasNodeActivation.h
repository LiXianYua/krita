/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_NODE_ACTIVATION_H
#define KIS_CANVAS_NODE_ACTIVATION_H

#include <kritacanvas_export.h>
#include <kis_types.h>

/**
 * Narrow canvas port for tools that need to make a node current.
 *
 * The host remains responsible for validating the node and synchronizing its
 * view state. Tools only express the requested node transition.
 */
class KRITACANVAS_EXPORT KisCanvasNodeActivation
{
public:
    virtual ~KisCanvasNodeActivation();

    virtual void requestNodeActivation(KisNodeSP node) = 0;
};

#endif // KIS_CANVAS_NODE_ACTIVATION_H
