/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSELECTEDSHAPESPROXYSIMPLE_H
#define KOSELECTEDSHAPESPROXYSIMPLE_H

#include <PkPointer.h>
#include <KoSelectedShapesProxy.h>
#include <PkConnection.h>

class KoShapeManager;


class KRITAFLAKE_EXPORT KoSelectedShapesProxySimple : public KoSelectedShapesProxy
{
public:
    KoSelectedShapesProxySimple(KoShapeManager *shapeManager);
    ~KoSelectedShapesProxySimple() override;
    KoSelection *selection() override;

private:
    PkPointer<KoShapeManager> m_shapeManager;
    PkList<PkConnection> m_connections;
};

#endif // KOSELECTEDSHAPESPROXYSIMPLE_H
