/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSHAPEFILLRESOURCECONNECTOR_H
#define KOSHAPEFILLRESOURCECONNECTOR_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkVariant.h>

class KoCanvasBase;

class KoShapeFillResourceConnector : public PkObject
{
public:
    explicit KoShapeFillResourceConnector(PkObject *parent = nullptr);
    ~KoShapeFillResourceConnector();

    void connectToCanvas(KoCanvasBase *canvas);
    void disconnect();

private:
    void slotCanvasResourceChanged(int key, const PkVariant &value);

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif // KOSHAPEFILLRESOURCECONNECTOR_H
