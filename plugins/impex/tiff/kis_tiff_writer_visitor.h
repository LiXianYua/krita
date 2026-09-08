/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TIFF_WRITER_VISITOR_H
#define KIS_TIFF_WRITER_VISITOR_H

#include <tiffio.h>

#include <kis_node_visitor.h>

#include "kis_tiff_base_writer.h"

struct KisTIFFOptions;
class KisLayer;

/**
   @author Cyrille Berger <cberger@cberger.net>
*/
class KisTIFFWriterVisitor : public KisNodeVisitor, protected KisTIFFBaseWriter
{
public:

    using KisNodeVisitor::visit;

    KisTIFFWriterVisitor(TIFF*image, KisTIFFOptions* options);
    ~KisTIFFWriterVisitor() override;

public:

    bool visit(KisNode *) override;
    bool visit(KisPaintLayer *layer) override;
    bool visit(KisGroupLayer *layer) override;
    bool visit(KisGeneratorLayer *layer) override;
    bool visit(KisCloneLayer *layer) override;
    bool visit(KisExternalLayer *layer) override;
    bool visit(KisAdjustmentLayer *layer) override;
    bool visit(KisFilterMask *) override;
    bool visit(KisTransformMask *) override;
    bool visit(KisTransparencyMask *) override;
    bool visit(KisSelectionMask *) override;
    bool visit(KisColorizeMask *) override;


private:
    bool saveLayerProjection(KisLayer *);
};

#endif
