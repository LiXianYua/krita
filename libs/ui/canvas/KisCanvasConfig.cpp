/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCanvasConfig.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <config-ocio.h>

namespace
{
KConfigGroup rootConfig()
{
    return KSharedConfig::openConfig()->group("");
}
}

KisCanvasConfig::KisCanvasConfig(bool defaultValues)
    : m_defaultValues(defaultValues)
{
}

qreal KisCanvasConfig::vastScrolling() const
{
    return m_defaultValues ? 0.9 : rootConfig().readEntry("vastScrolling", 0.9);
}

bool KisCanvasConfig::showSingleChannelAsColor() const
{
    return m_defaultValues ? false : rootConfig().readEntry("showSingleChannelAsColor", false);
}

bool KisCanvasConfig::useOcio() const
{
#ifdef HAVE_OCIO
    return m_defaultValues ? false : rootConfig().readEntry("Krita/Ocio/UseOcio", false);
#else
    return false;
#endif
}
