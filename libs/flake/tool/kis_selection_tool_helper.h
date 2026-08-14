/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SELECTION_TOOL_HELPER_H
#define KIS_SELECTION_TOOL_HELPER_H

#include <kritashapemodel_export.h>

#include "kundo2magicstring.h"
#include "kis_selection.h"
#include "kis_processing_applicator.h"

class KoCanvasBase;
class KoShape;

/**
 * XXX: Doc!
 */
class KRITASHAPEMODEL_EXPORT KisSelectionToolHelper
{
public:

    KisSelectionToolHelper(KoCanvasBase *canvas,
                           KisImageSP image,
                           KisNodeSP activeNode,
                           const KUndo2MagicString& name);
    virtual ~KisSelectionToolHelper();

    void selectPixelSelection(KisProcessingApplicator& applicator, KisPixelSelectionSP selection, SelectionAction action);
    void selectPixelSelection(KisPixelSelectionSP selection, SelectionAction action);

    void addSelectionShape(KoShape* shape, SelectionAction action = SELECTION_DEFAULT);
    void addSelectionShapes(QList<KoShape*> shapes, SelectionAction action = SELECTION_DEFAULT);

    bool canShortcutToDeselect(const QRect &rect, SelectionAction action);
    bool canShortcutToNoop(const QRect &rect, SelectionAction action);

    bool tryDeselectCurrentSelection(const QRectF selectionViewRect, SelectionAction action);

    SelectionMode tryOverrideSelectionMode(KisSelectionSP activeSelection, SelectionMode currentMode, SelectionAction currentAction) const;


private:
    KoCanvasBase *m_canvas;
    KisImageSP m_image;
    KisNodeSP m_activeNode;
    KUndo2MagicString m_name;
};


#endif
