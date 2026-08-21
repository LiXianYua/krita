/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisChangeChannelLockFlagsCommand.h"
#include "kis_paint_layer.h"

KisChangeChannelLockFlagsCommand::KisChangeChannelLockFlagsCommand(const PkBitArray &newFlags, const PkBitArray &oldFlags,
                                                                   KisPaintLayerSP layer, KUndo2Command *parentCommand)
    : KUndo2Command(kundo2_text_raw("change-channel-lock-flags-command"), parentCommand),
      m_layer(layer),
      m_oldFlags(oldFlags),
      m_newFlags(newFlags)
{
}

KisChangeChannelLockFlagsCommand::KisChangeChannelLockFlagsCommand(const PkBitArray &newFlags, KisPaintLayerSP layer, KUndo2Command *parentCommand)
    : KisChangeChannelLockFlagsCommand(newFlags, layer->channelLockFlags(), layer, parentCommand)
{
}

void KisChangeChannelLockFlagsCommand::redo()
{
    m_layer->setChannelLockFlags(m_newFlags);
}

void KisChangeChannelLockFlagsCommand::undo()
{
    m_layer->setChannelLockFlags(m_oldFlags);
}
