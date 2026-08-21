/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CHANGE_CHANNEL_LOCK_FLAGS_COMMAND_H_
#define KIS_CHANGE_CHANNEL_LOCK_FLAGS_COMMAND_H_

#include <kritaimage_export.h>

#include <PkBitArray.h>

#include "kis_types.h"
#include <kundo2command.h>

class KisChangeChannelLockFlagsCommand : public KUndo2Command
{

public:
    KisChangeChannelLockFlagsCommand(const PkBitArray &newFlags,
                                     KisPaintLayerSP layer,
                                     KUndo2Command *parentCommand = 0);

    KisChangeChannelLockFlagsCommand(const PkBitArray &newFlags,
                                     const PkBitArray &oldFlags,
                                     KisPaintLayerSP layer,
                                     KUndo2Command *parentCommand = 0);

    void redo() override;
    void undo() override;

protected:
    KisPaintLayerSP m_layer;
    PkBitArray m_oldFlags;
    PkBitArray m_newFlags;
};

#endif
