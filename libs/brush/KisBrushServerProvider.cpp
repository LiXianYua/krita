/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisBrushServerProvider.h"

#include <KoResourcePaths.h>

#include <KoResource.h>

#include <kis_debug.h>

KisBrushServerProvider::KisBrushServerProvider()
{
    m_brushServer = new KoResourceServer<KisBrush>(ResourceType::Brushes);
}

KisBrushServerProvider::~KisBrushServerProvider()
{
    delete m_brushServer;
}

KisBrushServerProvider* KisBrushServerProvider::instance()
{
    static KisBrushServerProvider s_instance;
    return &s_instance;
}

KoResourceServer<KisBrush>* KisBrushServerProvider::brushServer()
{
    return m_brushServer;
}
