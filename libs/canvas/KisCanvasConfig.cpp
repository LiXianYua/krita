/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCanvasConfig.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkString.h>

#include <config-ocio.h>

namespace
{
PkConfigGroup rootConfig()
{
    return PkSharedConfig::openConfig()->group(PkString(""));
}
}

KisCanvasConfig::KisCanvasConfig(bool defaultValues)
    : m_defaultValues(defaultValues)
{
}

qreal KisCanvasConfig::vastScrolling() const
{
    return m_defaultValues ? 0.9 : rootConfig().readEntry(PkString("vastScrolling"), 0.9);
}

bool KisCanvasConfig::showSingleChannelAsColor() const
{
    return m_defaultValues ? false : rootConfig().readEntry(PkString("showSingleChannelAsColor"), false);
}

bool KisCanvasConfig::useOcio() const
{
#ifdef HAVE_OCIO
    return m_defaultValues ? false : rootConfig().readEntry(PkString("Krita/Ocio/UseOcio"), false);
#else
    return false;
#endif
}
