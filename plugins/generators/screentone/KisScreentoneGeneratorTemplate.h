/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSCREENTONEGENERATORTEMPLATE_H
#define KISSCREENTONEGENERATORTEMPLATE_H

#include <PkVector.h>
#include <PkTransform.h>

#include "KisScreentoneGeneratorConfiguration.h"

class KisScreentoneGeneratorTemplate
{
public:
    KisScreentoneGeneratorTemplate(const KisScreentoneGeneratorConfigurationSP config);

    inline const PkVector<qreal>& templateData() const { return m_templateData; }
    inline const PkTransform& imageToScreenTransform() const { return m_imageToScreenTransform; }
    inline const PkTransform& screenToTemplateTransform() const { return m_screenToTemplateTransform; }
    inline const PkTransform& templateToScreenTransform() const { return m_templateToScreenTransform; }
    inline const PkPointF& screenPosition() const { return m_screenPosition; }
    inline const PkSize& macrocellSize() const { return m_macrocellSize; }
    inline const PkSize& templateSize() const { return m_templateSize; }
    inline const PkPoint& originOffset() const{ return m_originOffset; }
    inline const PkPointF& v1() const { return m_v1; }
    inline const PkPointF& v2() const { return m_v2; }

private:
    PkVector<qreal> m_templateData;
    PkTransform m_imageToScreenTransform, m_screenToTemplateTransform, m_templateToScreenTransform;
    PkPointF m_screenPosition;
    PkSize m_macrocellSize;
    PkSize m_templateSize;
    PkPoint m_originOffset;
    PkPointF m_v1, m_v2;

    template <typename ScreentoneFunction>
    void makeTemplate(const KisScreentoneGeneratorConfigurationSP config, ScreentoneFunction screentoneFunction);
    PkVector<int> makeCellOrderList(int macrocellColumns, int macrocellRows) const;
};

#endif
