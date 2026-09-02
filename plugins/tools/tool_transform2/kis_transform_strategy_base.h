/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TRANSFORM_STRATEGY_BASE_H
#define __KIS_TRANSFORM_STRATEGY_BASE_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkScopedPointer.h>

#include "kis_tool.h"
#include "TransformToolPlatform.h"


class PkImage;
class PkTransform;
class PkPainter;
class KoPointerEvent;
class PkPainterPath;


class KisTransformStrategyBase : public PkShellObject
{
public:
    KisTransformStrategyBase();
    ~KisTransformStrategyBase() override;

    PkImage originalImage() const;
    PkTransform thumbToImageTransform() const;

    void setThumbnailImage(const PkImage &image, PkTransform thumbToImageTransform);

public:

    virtual bool acceptsClicks() const;

    virtual void paint(TransformToolPainter &gc) = 0;
    virtual TransformCursorDescriptor getCurrentCursor() const = 0;
    virtual PkPainterPath getCursorOutline() const;

    virtual void externalConfigChanged() = 0;

    virtual void activatePrimaryAction();
    virtual void deactivatePrimaryAction();

    virtual void setDecorationThickness(int thickness);
    virtual int decorationThickness() const;

    virtual bool beginPrimaryAction(KoPointerEvent *event) = 0;
    virtual void continuePrimaryAction(KoPointerEvent *event) = 0;
    virtual bool endPrimaryAction(KoPointerEvent *event) = 0;
    virtual void hoverActionCommon(KoPointerEvent *event) = 0;

    virtual void activateAlternateAction(KisTool::AlternateAction action);
    virtual void deactivateAlternateAction(KisTool::AlternateAction action);

    virtual bool beginAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action);
    virtual void continueAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action);
    virtual bool endAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action);

    virtual void increaseBrushSize(KoCanvasBase *canvas);
    virtual void decreaseBrushSize(KoCanvasBase *canvas);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_TRANSFORM_STRATEGY_BASE_H */
