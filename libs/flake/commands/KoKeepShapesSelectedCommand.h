/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOKEEPSHAPESSELECTEDCOMMAND_H
#define KOKEEPSHAPESSELECTEDCOMMAND_H

#include <PkXmlCompat.h>

#include "kis_command_utils.h"
#include <kritaflake_export.h>

class KoSelectedShapesProxy;
class KoSelection;
class KoShape;

class KRITAFLAKE_EXPORT KoKeepShapesSelectedCommand : public KisCommandUtils::FlipFlopCommand
{
public:
    KoKeepShapesSelectedCommand(const PkList<KoShape*> &selectedBefore,
                                const PkList<KoShape*> &selectedAfter,
                                KoSelectedShapesProxy *selectionProxy,
                                bool isFinalizing,
                                KUndo2Command *parent);

protected:
    void partB() override;

private:
    PkList<KoShape*> m_selectedBefore;
    PkList<KoShape*> m_selectedAfter;
    KoSelectedShapesProxy *m_selectionProxy;
};

#endif // KOKEEPSHAPESSELECTEDCOMMAND_H
