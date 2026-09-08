/* SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-or-later
 * Native subset of Qt 5.15.7 qrasterizer_p.h: outline scan conversion.
 */
#pragma once
#include "PkRasterDefs.h"
#include <PkRect.h>

using ProcessSpans = PK_FT_SpanFunc;
class PkAliasedRasterizerPrivate;
class PkAliasedRasterizer
{
public:
    PkAliasedRasterizer();
    ~PkAliasedRasterizer();
    PkAliasedRasterizer(const PkAliasedRasterizer &) = delete;
    PkAliasedRasterizer &operator=(const PkAliasedRasterizer &) = delete;
    void setAntialiased(bool antialiased);
    void setClipRect(const PkRect &clip);
    void setLegacyRoundingEnabled(bool enabled);
    void initialize(ProcessSpans callback, void *data);
    void rasterize(const PK_FT_Outline *outline, Pk::FillRule fillRule);
private:
    PkAliasedRasterizerPrivate *d;
};
