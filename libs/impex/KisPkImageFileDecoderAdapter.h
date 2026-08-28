/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PK_IMAGE_FILE_DECODER_ADAPTER_H
#define KIS_PK_IMAGE_FILE_DECODER_ADAPTER_H

#include "kritaimpex_export.h"

/**
 * Ensure that the process-wide Pk image decoder can delegate exotic raster
 * files to the statically registered Krita import filters.
 *
 * The kritaimpex DSO invokes this during initialization. The exported anchor
 * also lets DSO-lifetime tests force and verify registration explicitly.
 */
extern "C" KRITAIMPEX_EXPORT void kisEnsurePkImageFileDecoderAdapterRegistered();

#endif
