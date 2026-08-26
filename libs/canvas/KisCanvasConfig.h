/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_CANVAS_CONFIG_H
#define KIS_CANVAS_CONFIG_H

#include <PkGlobal.h>

class KisCanvasConfig
{
public:
    explicit KisCanvasConfig(bool defaultValues = false);

    qreal vastScrolling() const;
    bool showSingleChannelAsColor() const;
    bool useOcio() const;

private:
    bool m_defaultValues;
};

#endif
