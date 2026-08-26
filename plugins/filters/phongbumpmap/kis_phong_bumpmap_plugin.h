/*
 *  SPDX-FileCopyrightText: 2010-2011 José Luis Vergara <pentalis@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PHONG_BUMPMAP_PLUGIN_H
#define KIS_PHONG_BUMPMAP_PLUGIN_H

#include <QObject>
#include <PkVariant.h>

class KisPhongBumpmapPlugin : public QObject
{
    Q_OBJECT
public:
    KisPhongBumpmapPlugin(QObject *parent, const PkVariantList &);
    ~KisPhongBumpmapPlugin() override;
};

#endif //KIS_PHONG_BUMPMAP_PLUGIN_H
