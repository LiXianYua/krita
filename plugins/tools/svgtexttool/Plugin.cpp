/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "SvgTextToolResources.h"

// D-12 静态注册：原 K_PLUGIN_FACTORY_WITH_JSON 动态加载改为由 registerAllPlugins() 调用的静态注册。
void registerSvgTextTool()
{
    svgTextToolResourceAnchor();
    registerSvgTextToolFactory();
}
