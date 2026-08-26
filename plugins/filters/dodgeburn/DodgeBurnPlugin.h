/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _DODGE_BURN_PLUGIN_H_
#define _DODGE_BURN_PLUGIN_H_

#include <QObject>
#include <PkVariant.h>

class DodgeBurnPlugin : public QObject
{
    Q_OBJECT
public:
    DodgeBurnPlugin(QObject *parent, const PkVariantList &);
    ~DodgeBurnPlugin() override;
};

#endif
