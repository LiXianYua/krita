/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtCore/QtCore>
#include <QtGui/QtGui>

#include <pk/render/PkPainter.h>
#include <PkPaintCommand.h>

// Qt oracle only: never link this backend into a production target.
class PkQPainterAdapter final : public PkPainterBackend
{
public:
    explicit PkQPainterAdapter(QPainter &painter);
    void submit(const PkPaintCommand &command) override;
    qreal devicePixelRatio() const override;

private:
    QPainter &m_painter;
};
