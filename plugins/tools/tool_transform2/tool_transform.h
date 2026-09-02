/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TOOL_TRANSFORM_H_
#define TOOL_TRANSFORM_H_

#include <functional>

#include "kritatooltransform_export.h"

enum class TransformToolRegistrationStep {
    ToolFactory,
    AnimatedParamsHolderFactory,
    TransformMaskFactory,
    DumbTransformMaskFactory
};

using TransformToolRegistrationCallback =
    std::function<void(TransformToolRegistrationStep)>;

/**
 * A module that provides a transform tool.
 */
KRITATOOLTRANSFORM_EXPORT void registerToolTransformPlugin(
    const TransformToolRegistrationCallback &callback);
KRITATOOLTRANSFORM_EXPORT void registerToolTransformPlugin();

#endif // TOOL_TRANSFORM_H_
