/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kritashapemodel_export.h"

#include <PkPaintCommand.h>

class KRITASHAPEMODEL_EXPORT PkImageRasterBackend final : public PkPainterBackend
{
public:
    explicit PkImageRasterBackend(PkImage &destination);

    void submit(const PkPaintCommand &command) override;
    qreal devicePixelRatio() const override;

private:
    void drawImage(const PkDrawImageCommand &command);

    PkImage &m_destination;
    qreal m_opacity {1.0};
};
