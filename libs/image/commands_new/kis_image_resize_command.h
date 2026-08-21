/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMAGE_RESIZE_COMMAND_H_
#define KIS_IMAGE_RESIZE_COMMAND_H_

#include "kritaimage_export.h"
#include "kis_types.h"

#include <kundo2command.h>
#include <PkSize.h>

class KRITAIMAGE_EXPORT KisImageResizeCommand : public KUndo2Command
{
public:
    KisImageResizeCommand(KisImageWSP image, const PkSize& newRect, KUndo2Command *parent = 0);

    void redo() override;
    void undo() override;

private:
    PkSize m_sizeBefore;
    PkSize m_sizeAfter;
    KisImageWSP m_image;
};

#endif
