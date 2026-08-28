/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_OPEN_RASTER_STACK_LOAD_VISITOR_H_
#define KIS_OPEN_RASTER_STACK_LOAD_VISITOR_H_

#include "kis_global.h"
#include <PkVector.h>
#include "kis_types.h"

class PkXmlElement;

class KisUndoStore;
class KisOpenRasterLoadContext;

class KisOpenRasterStackLoadVisitor
{
public:
    KisOpenRasterStackLoadVisitor(KisUndoStore *undoStore, KisOpenRasterLoadContext* orlc);
    virtual ~KisOpenRasterStackLoadVisitor();

public:
    void loadImage();
    void loadPaintLayer(const PkXmlElement& elem, KisPaintLayerSP pL);
    void loadAdjustmentLayer(const PkXmlElement& elem, KisAdjustmentLayerSP pL);
    void loadGroupLayer(const PkXmlElement& elem, KisGroupLayerSP groupLayer);
    KisImageSP image();
    vKisNodeSP activeNodes();
private:
    void loadLayerInfo(const PkXmlElement& elem, KisLayerSP layer);
    struct Private;
    Private* const d;
};


#endif // KIS_LAYER_VISITOR_H_
