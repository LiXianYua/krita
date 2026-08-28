/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_EXR_LAYERS_SORTER_H
#define __KIS_EXR_LAYERS_SORTER_H

#include "kis_types.h"
#include <PkScopedPointer.h>

class PkXmlDocument;


class KisExrLayersSorter
{
public:
    KisExrLayersSorter(const PkXmlDocument &extraData, KisImageSP image);
    ~KisExrLayersSorter();

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif /* __KIS_EXR_LAYERS_SORTER_H */
