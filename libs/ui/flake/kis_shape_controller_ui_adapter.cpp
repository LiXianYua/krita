/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_shape_controller_ui_adapter.h"

namespace
{
KisShapeControllerUiAdapter *s_adapter = nullptr;
}

KisShapeControllerUiAdapter::~KisShapeControllerUiAdapter() = default;

KisShapeControllerUiAdapter *KisShapeControllerUiAdapter::instance()
{
    return s_adapter;
}

void KisShapeControllerUiAdapter::setInstance(KisShapeControllerUiAdapter *adapter)
{
    s_adapter = adapter;
}
