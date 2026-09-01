/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2018 Emmet & Eoin O'Neill <emmetoneill.pdx@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_colorsampler.h"

#include <KisCanvasToolServices.h>
#include <KisColorSamplingCanvas.h>
#include <KoCanvasBase.h>
#include <KoPointerEvent.h>
#include <KoViewConverter.h>

KisToolColorSampler::KisToolColorSampler(KoCanvasBase *canvas)
    : KisTool(canvas, dynamic_cast<KisCanvasToolServices *>(canvas)->toolSamplerCursor()),
      m_config(new KisColorSamplerConfig),
      m_helper(canvas, dynamic_cast<KisColorSamplingCanvas *>(canvas))
{
    setObjectName("tool_colorsampler");
    PkObject::connect(&m_helper, &KisAsyncColorSamplerHelper::sigRequestCursor,
                      this, [this](const auto &cursor) { useCursor(cursor); });
    PkObject::connect(&m_helper, &KisAsyncColorSamplerHelper::sigRequestCursorReset,
                      this, &KisToolColorSampler::slotColorPickerRequestedCursorReset);
    PkObject::connect(&m_helper, &KisAsyncColorSamplerHelper::sigRequestUpdateOutline,
                      this, &KisToolColorSampler::slotColorPickerRequestedOutlineUpdate);
    PkObject::connect(&m_helper, &KisAsyncColorSamplerHelper::sigRawColorSelected,
                      this, &KisToolColorSampler::slotColorPickerSelectedColor);
    PkObject::connect(&m_helper, &KisAsyncColorSamplerHelper::sigFinalColorSelected,
                      this, &KisToolColorSampler::slotColorPickerSelectionFinished);
}

KisToolColorSampler::~KisToolColorSampler()
{
    if (m_isActivated) {
        m_config->save();
    }
}

void KisToolColorSampler::slotColorPickerRequestedCursorReset()
{
    /// we explicitly avoid resetting the cursor style
    /// to avoid blinking of the cursor
}

void KisToolColorSampler::slotColorPickerRequestedOutlineUpdate()
{
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolColorSampler::slotColorPickerSelectedColor(const KoColor &color)
{
    /**
     * Please remember that m_sampledColor also have the alpha
     * of the picked color!
     */
    m_sampledColor = color;
}

void KisToolColorSampler::slotColorPickerSelectionFinished(const KoColor &color)
{
    (void)color;

    // Body removed with the options panel (now deleted): it used to add the
    // sampled colour as a swatch to whichever palette the panel's cmbPalette
    // had selected, guarded by m_config->addColorToCurrentPalette. Only the
    // palette chooser lived in the panel; the guard itself is in the untouched
    // KisToolUtils::ColorSamplerConfig (key "addPalette", default false), so
    // the out-of-the-box behaviour is unchanged -- users who had switched it on
    // silently stop getting swatches until the S-line UI supplies a palette
    // chooser again. The connect() in the constructor is deliberately kept so
    // that re-wiring only has to fill this body back in.
}

void KisToolColorSampler::paint(PkPainter &gc, const KoViewConverter &converter)
{
    m_helper.paint(gc, converter);
}

void KisToolColorSampler::activate(const PkSet<KoShape*> &shapes)
{

    m_isActivated = true;
    m_config->load();

    KisTool::activate(shapes);
}

void KisToolColorSampler::deactivate()
{
    m_config->save();

    m_isActivated = false;
    KisTool::deactivate();
}

void KisToolColorSampler::beginPrimaryAction(KoPointerEvent *event)
{
    m_helper.setUpdateGlobalColor(m_config->updateColor);

    bool useOtherColor = canvas()->resourceManager()->boolResource(KoCanvasResource::UsingOtherColor);
    // if useOtherColor is true, apply to the other color than that configured in the tool options
    m_helper.activate(!m_config->sampleMerged, m_config->toForegroundColor != useOtherColor);
    m_helper.startAction(event->point, m_config->radius, m_config->blend);
    requestUpdateOutline(event->point, event);

    setMode(KisTool::PAINT_MODE);
}

void KisToolColorSampler::mouseMoveEvent(KoPointerEvent *event){
    KisTool::mouseMoveEvent(event);
}

void KisToolColorSampler::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    m_helper.continueAction(event->point);
    requestUpdateOutline(event->point, event);
}

void KisToolColorSampler::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    m_helper.endAction();
    m_helper.deactivate();
    requestUpdateOutline(event->point, event);

}
void KisToolColorSampler::activatePrimaryAction()
{
    /**
     * We explicitly avoid calling KisTool::activatePrimaryAction()
     * here, because it resets the cursor, causing cursor blinking
     */
    bool useOtherColor = canvas()->resourceManager()->boolResource(KoCanvasResource::UsingOtherColor);
    // if useOtherColor is true, apply to the other color than that configured in the tool options
    m_helper.updateCursor(!m_config->sampleMerged, m_config->toForegroundColor != useOtherColor);
}

void KisToolColorSampler::deactivatePrimaryAction()
{
    /**
     * We explicitly avoid calling KisTool::endPrimaryAction()
     * here, because it resets the cursor, causing cursor blinking
     */
}

void KisToolColorSampler::requestUpdateOutline(const PkPointF &outlineDocPoint, const KoPointerEvent *event)
{
    (void)event;

    PkRectF colorPreviewDocUpdateRect;

    qreal zoomX;
    qreal zoomY;
    canvas()->viewConverter()->zoom(&zoomX, &zoomY);
    qreal xoffset = 2.0/zoomX;
    qreal yoffset = 2.0/zoomY;

    m_outlineDocPoint = outlineDocPoint;

    colorPreviewDocUpdateRect = m_helper.colorPreviewDocRect(m_outlineDocPoint);

    if (!colorPreviewDocUpdateRect.isEmpty()) {
        colorPreviewDocUpdateRect = colorPreviewDocUpdateRect.adjusted(-xoffset,-yoffset,xoffset,yoffset);
    }

    if (!m_oldColorPreviewUpdateRect.isEmpty()){
        canvas()->updateCanvas(m_oldColorPreviewUpdateRect);
    }

    if (!colorPreviewDocUpdateRect.isEmpty()){
        canvas()->updateCanvas(colorPreviewDocUpdateRect);
    }

    m_oldColorPreviewUpdateRect = colorPreviewDocUpdateRect;
}
