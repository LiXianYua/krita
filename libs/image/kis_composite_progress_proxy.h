/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_COMPOSITE_PROGRESS_PROXY_H
#define __KIS_COMPOSITE_PROGRESS_PROXY_H

#include <PkList.h>
#include <KoProgressProxy.h>
#include "kritaimage_export.h"


class KRITAIMAGE_EXPORT KisCompositeProgressProxy : public KoProgressProxy
{
public:
    void addProxy(KoProgressProxy *proxy);
    void removeProxy(KoProgressProxy *proxy);

    int maximum() const override;
    void setValue(int value) override;
    void setRange(int minimum, int maximum) override;
    void setFormat(const PkString &format) override;
    void setAutoNestedName(const PkString &name) override;

private:
    PkList<KoProgressProxy*> m_proxies;
    PkList<KoProgressProxy*> m_uniqueProxies;
};

#endif /* __KIS_COMPOSITE_PROGRESS_PROXY_H */
