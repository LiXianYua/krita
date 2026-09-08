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
    void drawTransformedImage(const PkDrawImageCommand &command,
                              const PkRectF &source = PkRectF(), bool tiled = false);
    void renderImage(const PkImage &image, const std::vector<unsigned char> &mask,
                     const PkTransform &placement, const PkRectF &source, bool tiled);
    void fillPath(const PkPainterPath &path, const PkBrush &brush, bool rectangle = false);
    void strokePath(const PkPainterPath &path, const PkPen &pen, bool point = false);
    void setClip(const PkPainterPath &path, Pk::ClipOperation operation);
    std::vector<unsigned char> coverage(const PkPainterPath &path) const;
    std::vector<unsigned char> rectangleCoverage(const PkRectF &rect) const;

    PkImage &m_destination;
    struct State {
        qreal opacity {1.0};
        Pk::CompositionMode mode {Pk::CompositionMode_SourceOver};
        PkTransform transform;
        PkPen pen;
        PkBrush brush;
        unsigned hints {0};
        std::vector<unsigned char> clip;
        bool hasClip {false};
    };
    State m_state;
    std::vector<State> m_stack;
};
