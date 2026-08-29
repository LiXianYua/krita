/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_undo_adapter.h"


KisUndoAdapter::KisUndoAdapter(KisUndoStore *undoStore, PkObject *parent)
    : PkShellObject(parent),
      m_undoStore(undoStore)
{
}

KisUndoAdapter::~KisUndoAdapter()
{
}

void KisUndoAdapter::emitSelectionChanged()
{
    selectionChanged();
}


