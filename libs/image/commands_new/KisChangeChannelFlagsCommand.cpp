/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisChangeChannelFlagsCommand.h"
#include "kis_layer.h"

KisChangeChannelFlagsCommand::KisChangeChannelFlagsCommand(const PkBitArray &newFlags, const PkBitArray &oldFlags,
                                                           KisLayerSP layer, KUndo2Command *parentCommand)
    : KUndo2Command(kundo2_text_raw("change-channel-flags-command"), parentCommand),
      m_layer(layer),
      m_oldFlags(oldFlags),
      m_newFlags(newFlags)
{
}

KisChangeChannelFlagsCommand::KisChangeChannelFlagsCommand(const PkBitArray &newFlags, KisLayerSP layer, KUndo2Command *parentCommand)
    : KisChangeChannelFlagsCommand(newFlags, layer->channelFlags(), layer, parentCommand)
{
}

void KisChangeChannelFlagsCommand::redo()
{
    m_layer->setChannelFlags(m_newFlags);
}

void KisChangeChannelFlagsCommand::undo()
{
    m_layer->setChannelFlags(m_oldFlags);
}
