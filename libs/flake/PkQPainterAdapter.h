/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtCore/QtCore>
#include <QtGui/QtGui>

#include "kritaflake_export.h"
#include <PkPaintCommand.h>

class KRITAFLAKE_EXPORT PkQPainterAdapter final : public PkPainterBackend
{
public:
    explicit PkQPainterAdapter(QPainter &painter);
    void submit(const PkPaintCommand &command) override;

private:
    QPainter &m_painter;
};
