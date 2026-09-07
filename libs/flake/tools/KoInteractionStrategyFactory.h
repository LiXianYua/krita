/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOINTERACTIONSTRATEGYFACTORY_H
#define KOINTERACTIONSTRATEGYFACTORY_H

#include <PkScopedPointer.h>
#include <PkSharedPointer.h>
#include "kritaflake_export.h"

class PkString;
class PkPainter;
class KoInteractionStrategy;
class KoPointerEvent;
class KoViewConverter;

class KoInteractionStrategyFactory;
typedef PkSharedPointer<KoInteractionStrategyFactory> KoInteractionStrategyFactorySP;

class KRITAFLAKE_EXPORT KoInteractionStrategyFactory
{
public:
    KoInteractionStrategyFactory(int priority, const PkString &id);
    virtual ~KoInteractionStrategyFactory();

    PkString id() const;
    int priority() const;

    virtual KoInteractionStrategy* createStrategy(KoPointerEvent *ev) = 0;
    virtual bool hoverEvent(KoPointerEvent *ev) = 0;
    virtual bool paintOnHover(PkPainter &painter, const KoViewConverter &converter) = 0;
    virtual bool tryUseCustomCursor() = 0;

    static bool compareLess(KoInteractionStrategyFactorySP f1, KoInteractionStrategyFactorySP f2);

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};



#endif // KOINTERACTIONSTRATEGYFACTORY_H
