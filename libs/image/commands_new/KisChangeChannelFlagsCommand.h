/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CHANGE_CHANNEL_FLAGS_COMMAND_H_
#define KIS_CHANGE_CHANNEL_FLAGS_COMMAND_H_

#include <kritaimage_export.h>

#include <PkBitArray.h>

#include "kis_types.h"
#include <kundo2command.h>

class KisChangeChannelFlagsCommand : public KUndo2Command
{

public:
    KisChangeChannelFlagsCommand(const PkBitArray &newFlags,
                                 KisLayerSP layer,
                                 KUndo2Command *parentCommand = 0);

    KisChangeChannelFlagsCommand(const PkBitArray &newFlags,
                                 const PkBitArray &oldFlags,
                                 KisLayerSP layer,
                                 KUndo2Command *parentCommand = 0);

    void redo() override;
    void undo() override;

protected:
    KisLayerSP m_layer;
    PkBitArray m_oldFlags;
    PkBitArray m_newFlags;
};

#endif
