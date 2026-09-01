/*
 *  SPDX-FileCopyrightText: 2003-2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2015 Moritz Molch <kde@moritzmolch.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>

#include <QAction>
#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLayout>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <QVBoxLayout>
#include <QWhatsThis>
#include <QWidget>

#include <PkObject.h>

#include "kis_tool_paint.h"
#include <kis_debug.h>

#include <klocalizedstring.h>

#include <kis_algebra_2d.h>
#include <KoShape.h>
#include <KoCanvasResourceProvider.h>
#include <KoColorSpace.h>
#include <KoPointerEvent.h>
#include <KoColor.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoViewConverter.h>

#include <kis_types.h>
#include <kis_global.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_layer.h>
#include <KisCanvasToolServices.h>
#include <KisColorSamplingCanvas.h>
#include <kis_cubic_curve.h>

#include "kis_config_notifier.h"
#include "kis_image_config.h"
#include <KisColorSamplerConfig.h>
#include <brushengine/kis_paintop.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/KisOptimizedBrushOutline.h>
#include "kis_paintop_utils.h"

namespace {

QString r44ToQString(const PkString &value)
{
    return QString::fromUtf8(value.PkToUtf8().c_str());
}

QRectF r44ToQRectF(const PkRectF &rect)
{
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}

PkPointF r44ToPkPointF(const QPointF &point)
{
    return PkPointF(point.x(), point.y());
}

PkPainterPath r44ToPkPainterPath(const QPainterPath &path)
{
    PkPainterPath result;
    result.setFillRule(path.fillRule());

    const int elementCount = path.elementCount();
    for (int i = 0; i < elementCount; ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        if (element.isMoveTo()) {
            result.moveTo(element.x, element.y);
        } else if (element.isLineTo()) {
            result.lineTo(element.x, element.y);
        } else if (element.isCurveTo()) {
            const QPainterPath::Element controlPoint2 = path.elementAt(i + 1);
            const QPainterPath::Element endPoint = path.elementAt(i + 2);
            result.cubicTo(element.x, element.y,
                           controlPoint2.x, controlPoint2.y,
                           endPoint.x, endPoint.y);
            i += 2;
        }
    }

    return result;
}

}


struct KisToolPaint::Private
{
    // Keeps track of past cursor positions. This is used to determine the drawing angle when
    // drawing the brush outline or starting a stroke.
    KisPaintOpUtils::PositionHistory lastCursorPos;
};


KisToolPaint::KisToolPaint(KoCanvasBase *canvas, const QCursor &cursor)
    : KisTool(canvas, cursor),
      m_isOutlineEnabled(true),
      m_isOutlineVisible(true),
      m_standardBrushSizes(1, KisImageConfig(true).maxBrushSize()),
      m_colorSamplerHelper(canvas,
                           dynamic_cast<KisColorSamplingCanvas *>(canvas)),
      m_d(new Private())
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas);
    KIS_ASSERT(services);
    connect(this, &KisToolPaint::sigPaintingFinished, this, [services]() {
        services->toolNotifyPaintingFinished();
    });

    connect(&m_colorSamplerHelper, SIGNAL(sigRequestCursor(QCursor)), this, SLOT(slotColorPickerRequestedCursor(QCursor)));
    connect(&m_colorSamplerHelper, SIGNAL(sigRequestCursorReset()), this, SLOT(slotColorPickerRequestedCursorReset()));
    connect(&m_colorSamplerHelper, SIGNAL(sigRequestUpdateOutline()), this, SLOT(slotColorPickerRequestedOutlineUpdate()));
}


KisToolPaint::~KisToolPaint()
{
}

int KisToolPaint::flags() const
{
    return KisTool::FLAG_USES_CUSTOM_COMPOSITEOP;
}

void KisToolPaint::canvasResourceChanged(int key, const QVariant& v)
{
    KisTool::canvasResourceChanged(key, v);

    switch(key) {
    case(KoCanvasResource::Opacity):
        break;
    case(KoCanvasResource::CurrentPaintOpPreset): {
        if (isActive()) {
            requestUpdateOutline(m_outlineDocPoint, 0);
        }
        break;
    }
    case KoCanvasResource::CurrentPaintOpPresetName: {
        if (isActive()) {
            const QString formattedBrushName = v.toString().replace("_", " ");
            Q_EMIT statusTextChanged(formattedBrushName);
        }
        break;
    }
    default: //nothing
        break;
    }

    static const char configConnectionProperty[] = "r44ConfigConnection";
    if (!property(configConnectionProperty).toBool()) {
        KisConfigNotifier *notifier = KisConfigNotifier::instance();
        PkConnection configConnection = PkObject::connect(
            notifier, &KisConfigNotifier::configChanged, notifier,
            [this]() { resetCursorStyle(); });
        QObject::connect(this, &QObject::destroyed,
                         [configConnection](QObject *) mutable {
                             PkObject::disconnect(configConnection);
                         });
        setProperty(configConnectionProperty, true);
    }

}

void KisToolPaint::tryRestoreOpacitySnapshot()
{
    /**
     * Here is a weird heuristics on when to restore
     * brush opacity and when not. Basically, we should
     * restore opacity to its saved if the brush preset
     * hasn't changed too much, that is, its version is
     * the same and it hasn't been reset into a clean
     * state since then. The latter condition is checked
     * in a fuzzy manner by just mangling the isDirty
     * state before and after.
     */

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    KisPaintOpPresetSP newPreset = services->toolCurrentPaintOpPreset();

    if (newPreset) {
        m_oldPreset = newPreset;
        m_oldPresetIsDirty = newPreset->isDirty();
        m_oldPresetVersion = newPreset->version();
    }
}


void KisToolPaint::activate(const QSet<KoShape*> &shapes)
{
    if (currentPaintOpPreset()) {
        const QString formattedBrushName = currentPaintOpPreset() ? r44ToQString(currentPaintOpPreset()->name()).replace("_", " ") : QString();
        Q_EMIT statusTextChanged(formattedBrushName);
    }

    KisTool::activate(shapes);
    if (flags() & KisTool::FLAG_USES_CUSTOM_SIZE) {
        connect(action("increase_brush_size"), SIGNAL(triggered()), SLOT(increaseBrushSize()), Qt::UniqueConnection);
        connect(action("decrease_brush_size"), SIGNAL(triggered()), SLOT(decreaseBrushSize()), Qt::UniqueConnection);
        connect(action("increase_brush_size"), SIGNAL(triggered()), this, SLOT(showBrushSize()));
        connect(action("decrease_brush_size"), SIGNAL(triggered()), this, SLOT(showBrushSize()));

    }

    connect(action("rotate_brush_tip_clockwise"), SIGNAL(triggered()), SLOT(rotateBrushTipClockwise()), Qt::UniqueConnection);
    connect(action("rotate_brush_tip_clockwise_precise"), SIGNAL(triggered()), SLOT(rotateBrushTipClockwisePrecise()), Qt::UniqueConnection);
    connect(action("rotate_brush_tip_counter_clockwise"), SIGNAL(triggered()), SLOT(rotateBrushTipCounterClockwise()), Qt::UniqueConnection);
    connect(action("rotate_brush_tip_counter_clockwise_precise"), SIGNAL(triggered()), SLOT(rotateBrushTipCounterClockwisePrecise()), Qt::UniqueConnection);

    tryRestoreOpacitySnapshot();
}

void KisToolPaint::deactivate()
{
    if (flags() & KisTool::FLAG_USES_CUSTOM_SIZE) {
        disconnect(action("increase_brush_size"), 0, this, 0);
        disconnect(action("decrease_brush_size"), 0, this, 0);
    }

    disconnect(action("rotate_brush_tip_clockwise"), 0, this, 0);
    disconnect(action("rotate_brush_tip_clockwise_precise"), 0, this, 0);
    disconnect(action("rotate_brush_tip_counter_clockwise"), 0, this, 0);
    disconnect(action("rotate_brush_tip_counter_clockwise_precise"), 0, this, 0);

    tryRestoreOpacitySnapshot();
    Q_EMIT statusTextChanged(QString());

    KisTool::deactivate();
}

void KisToolPaint::slotColorPickerRequestedCursor(const QCursor &cursor)
{
    useCursor(cursor);
}

void KisToolPaint::slotColorPickerRequestedCursorReset()
{
    resetCursorStyle();
}

void KisToolPaint::slotColorPickerRequestedOutlineUpdate()
{
    requestUpdateOutline(m_outlineDocPoint, 0);
}

KisOptimizedBrushOutline KisToolPaint::tryFixBrushOutline(const KisOptimizedBrushOutline &originalOutline)
{
    KisImageConfig cfg(true);

    bool useSeparateEraserCursor = cfg.separateEraserCursor() && isEraser();

    const OutlineStyle currentOutlineStyle = !useSeparateEraserCursor ? cfg.newOutlineStyle() : cfg.eraserOutlineStyle();
    if (currentOutlineStyle == OUTLINE_NONE) return originalOutline;

    const qreal minThresholdSize = cfg.outlineSizeMinimum();

    /**
     * If the brush outline is bigger than the canvas itself (which
     * would make it invisible for a user in most of the cases) just
     * add a cross in the center of it
     */

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    const QSize widgetSize = services->toolCanvasWidgetSize();
    const int maxThresholdSum = widgetSize.width() + widgetSize.height();

    KisOptimizedBrushOutline outline = originalOutline;
    QRectF boundingRect = r44ToQRectF(outline.boundingRect());
    const qreal sum = boundingRect.width() + boundingRect.height();

    QPointF center = boundingRect.center();

    if (sum > maxThresholdSum) {
        const int hairOffset = 7;

        QPainterPath crossIcon;

        crossIcon.moveTo(center.x(), center.y() - hairOffset);
        crossIcon.lineTo(center.x(), center.y() + hairOffset);

        crossIcon.moveTo(center.x() - hairOffset, center.y());
        crossIcon.lineTo(center.x() + hairOffset, center.y());

        outline.addPath(r44ToPkPainterPath(crossIcon));

    } else if (sum < minThresholdSize && !outline.isEmpty()) {
        outline = KisOptimizedBrushOutline();
        outline.addEllipse(r44ToPkPointF(center), 0.5 * minThresholdSize, 0.5 * minThresholdSize);
    }

    return outline;
}

void KisToolPaint::paint(QPainter &gc, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    KisOptimizedBrushOutline path = tryFixBrushOutline(pixelToView(m_currentOutline));
    paintToolOutline(&gc, path);

    m_colorSamplerHelper.paint(gc, converter);
}

void KisToolPaint::setMode(ToolMode mode)
{
    if(this->mode() == KisTool::PAINT_MODE &&
            mode != KisTool::PAINT_MODE) {

        // Let's add history information about recently used colors
        Q_EMIT sigPaintingFinished();
    }

    KisTool::setMode(mode);
}

void KisToolPaint::activateAlternateAction(AlternateAction action)
{
    if (!isSamplingAction(action)) {
        KisTool::activateAlternateAction(action);
        return;
    }

    const bool sampleCurrentLayer = action == SampleFgNode || action == SampleBgNode;
    const bool sampleFgColor = action == SampleFgNode || action == SampleFgImage;
    m_colorSamplerHelper.activate(sampleCurrentLayer, sampleFgColor);
}

void KisToolPaint::deactivateAlternateAction(AlternateAction action)
{
    if (!isSamplingAction(action)) {
        KisTool::deactivateAlternateAction(action);
        return;
    }

    m_colorSamplerHelper.deactivate();
}

bool KisToolPaint::isSamplingAction(AlternateAction action) {
    return action == SampleFgNode ||
        action == SampleBgNode ||
        action == SampleFgImage ||
        action == SampleBgImage;
}

void KisToolPaint::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (isSamplingAction(action)) {
        setMode(SECONDARY_PAINT_MODE);

        KisColorSamplerConfig config;
        config.load();

        m_colorSamplerHelper.startAction(event->point, config.radius, config.blend);
        requestUpdateOutline(event->point, event);
    } else {
        KisTool::beginAlternateAction(event, action);
    }
}

void KisToolPaint::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (isSamplingAction(action)) {
        m_colorSamplerHelper.continueAction(event->point);
        requestUpdateOutline(event->point, event);
    } else {
        KisTool::continueAlternateAction(event, action);
    }
}

void KisToolPaint::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (isSamplingAction(action)) {
        m_colorSamplerHelper.endAction();
        requestUpdateOutline(event->point, event);
        setMode(HOVER_MODE);
    } else {
        KisTool::endAlternateAction(event, action);
    }
}

void KisToolPaint::mousePressEvent(KoPointerEvent *event)
{
    KisTool::mousePressEvent(event);
    if (mode() == KisTool::HOVER_MODE) {
        requestUpdateOutline(event->point, event);
    }
}

void KisToolPaint::mouseMoveEvent(KoPointerEvent *event)
{
    KisTool::mouseMoveEvent(event);
    if (mode() == KisTool::HOVER_MODE) {
        requestUpdateOutline(event->point, event);
    }
}

KisPopupWidgetInterface *KisToolPaint::popupWidget()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    if (!services) {
        return nullptr;
    }
    return services->toolPopupWidget();
}

void KisToolPaint::mouseReleaseEvent(KoPointerEvent *event)
{
    KisTool::mouseReleaseEvent(event);
    if (mode() == KisTool::HOVER_MODE) {
        requestUpdateOutline(event->point, event);
    }
}

QWidget *KisToolPaint::createOptionWidget()
{
    QWidget *optionWidget = new QWidget();
    optionWidget->setObjectName(toolId());

    QVBoxLayout *verticalLayout = new QVBoxLayout(optionWidget);
    verticalLayout->setObjectName("KisToolPaint::OptionWidget::VerticalLayout");
    verticalLayout->setContentsMargins(0,0,0,0);
    verticalLayout->setSpacing(5);

    // See https://bugs.kde.org/show_bug.cgi?id=316896
    QWidget *specialSpacer = new QWidget(optionWidget);
    specialSpacer->setObjectName("SpecialSpacer");
    specialSpacer->setFixedSize(0, 0);
    verticalLayout->addWidget(specialSpacer);
    verticalLayout->addWidget(specialSpacer);

    m_optionsWidgetLayout = new QGridLayout();
    m_optionsWidgetLayout->setColumnStretch(1, 1);
    verticalLayout->addLayout(m_optionsWidgetLayout);
    m_optionsWidgetLayout->setContentsMargins(0,0,0,0);
    m_optionsWidgetLayout->setSpacing(5);

    if (!quickHelp().isEmpty()) {
        QPushButton *push = new QPushButton(QIcon(), QString(), optionWidget);
        connect(push, SIGNAL(clicked()), this, SLOT(slotPopupQuickHelp()));
        QHBoxLayout *hLayout = new QHBoxLayout();
        hLayout->addWidget(push);
        hLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
        verticalLayout->addLayout(hLayout);
    }

    return optionWidget;
}

QWidget* findLabelWidget(QGridLayout *layout, QWidget *control)
{
    QWidget *result = 0;

    int index = layout->indexOf(control);

    int row, col, rowSpan, colSpan;
    layout->getItemPosition(index, &row, &col, &rowSpan, &colSpan);

    if (col > 0) {
        QLayoutItem *item = layout->itemAtPosition(row, col - 1);

        if (item) {
            result = item->widget();
        }
    } else {
        QLayoutItem *item = layout->itemAtPosition(row, col + 1);
        if (item) {
            result = item->widget();
        }
    }

    return result;
}

void KisToolPaint::showControl(QWidget *control, bool value)
{
    control->setVisible(value);
    QWidget *label = findLabelWidget(m_optionsWidgetLayout, control);
    if (label) {
        label->setVisible(value);
    }
}

void KisToolPaint::enableControl(QWidget *control, bool value)
{
    control->setEnabled(value);
    QWidget *label = findLabelWidget(m_optionsWidgetLayout, control);
    if (label) {
        label->setEnabled(value);
    }
}

void KisToolPaint::addOptionWidgetLayout(QLayout *layout)
{
    Q_ASSERT(m_optionsWidgetLayout != 0);
    int rowCount = m_optionsWidgetLayout->rowCount();
    m_optionsWidgetLayout->addLayout(layout, rowCount, 0, 1, 2);
}


void KisToolPaint::addOptionWidgetOption(QWidget *control, QWidget *label)
{
    Q_ASSERT(m_optionsWidgetLayout != 0);
    if (label) {
        m_optionsWidgetLayout->addWidget(label, m_optionsWidgetLayout->rowCount(), 0);
        m_optionsWidgetLayout->addWidget(control, m_optionsWidgetLayout->rowCount() - 1, 1);
    }
    else {
        m_optionsWidgetLayout->addWidget(control, m_optionsWidgetLayout->rowCount(), 0, 1, 2);
    }
}


void KisToolPaint::slotPopupQuickHelp()
{
    QWhatsThis::showText(QCursor::pos(), quickHelp());
}

void KisToolPaint::activatePrimaryAction()
{
    setOutlineVisible(true);
    KisTool::activatePrimaryAction();
}

void KisToolPaint::deactivatePrimaryAction()
{
    setOutlineVisible(false);
    KisTool::deactivatePrimaryAction();
}

bool KisToolPaint::isOutlineEnabled() const
{
    return m_isOutlineEnabled;
}

void KisToolPaint::setOutlineEnabled(bool enabled)
{
    m_isOutlineEnabled = enabled;
    requestUpdateOutline(m_outlineDocPoint, lastDeliveredPointerEvent());
}

bool KisToolPaint::isOutlineVisible() const
{
    return m_isOutlineVisible;
}

void KisToolPaint::setOutlineVisible(bool visible)
{
    m_isOutlineVisible = visible;
    requestUpdateOutline(m_outlineDocPoint, lastDeliveredPointerEvent());
}

void KisToolPaint::increaseBrushSize()
{
    qreal paintopSize = currentPaintOpPreset()->settings()->paintOpSize();
    int newValue = m_standardBrushSizes.increaseBrushSize(paintopSize);
    currentPaintOpPreset()->settings()->setPaintOpSize(newValue);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::decreaseBrushSize()
{
    qreal paintopSize = currentPaintOpPreset()->settings()->paintOpSize();
    int newValue = m_standardBrushSizes.decreaseBrushSize(paintopSize);
    currentPaintOpPreset()->settings()->setPaintOpSize(newValue);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::showBrushSize()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    services->toolShowBrushSize(currentPaintOpPreset()->settings()->paintOpSize());
}

void KisToolPaint::rotateBrushTipClockwise()
{
    const qreal angle = currentPaintOpPreset()->settings()->paintOpAngle();
    currentPaintOpPreset()->settings()->setPaintOpAngle(angle - 15);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::rotateBrushTipClockwisePrecise()
{
    const qreal angle = currentPaintOpPreset()->settings()->paintOpAngle();
    currentPaintOpPreset()->settings()->setPaintOpAngle(angle - 1);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::rotateBrushTipCounterClockwise()
{
    const qreal angle = currentPaintOpPreset()->settings()->paintOpAngle();
    currentPaintOpPreset()->settings()->setPaintOpAngle(angle + 15);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::rotateBrushTipCounterClockwisePrecise()
{
    const qreal angle = currentPaintOpPreset()->settings()->paintOpAngle();
    currentPaintOpPreset()->settings()->setPaintOpAngle(angle + 1);
    requestUpdateOutline(m_outlineDocPoint, 0);
}

void KisToolPaint::requestUpdateOutline(const QPointF &outlineDocPoint, const KoPointerEvent *event)
{
    QRectF outlinePixelRect;
    QRectF outlineDocRect;

    QRectF colorPreviewDocUpdateRect;

    QPointF outlineMoveVector;

    if (m_supportOutline) {
        KisImageConfig cfg(true);
        KisPaintOpSettings::OutlineMode outlineMode;

        bool useSeparateEraserCursor = cfg.separateEraserCursor() && isEraser();

        const OutlineStyle currentOutlineStyle = !useSeparateEraserCursor ? cfg.newOutlineStyle() : cfg.eraserOutlineStyle();
        const auto outlineStyleIsVisible = [&]() {
            return currentOutlineStyle == OUTLINE_FULL ||
                   currentOutlineStyle == OUTLINE_CIRCLE ||
                   currentOutlineStyle == OUTLINE_TILT;
        };
        const auto shouldShowOutlineWhilePainting = [&]() {
            return !useSeparateEraserCursor ? cfg.showOutlineWhilePainting() : cfg.showEraserOutlineWhilePainting();
        };
        if (isOutlineEnabled() && isOutlineVisible() &&
                (mode() == KisTool::GESTURE_MODE ||
                    (outlineStyleIsVisible() &&
                        (mode() == HOVER_MODE ||
                         (mode() == PAINT_MODE && shouldShowOutlineWhilePainting()))))) { // lisp forever!

            outlineMode.isVisible = true;

            switch (!useSeparateEraserCursor ? cfg.newOutlineStyle() : cfg.eraserOutlineStyle()) {
            case OUTLINE_CIRCLE:
                outlineMode.forceCircle = true;
                break;
            case OUTLINE_TILT:
                outlineMode.forceCircle = true;
                outlineMode.showTiltDecoration = true;
                break;
            default:
                break;
            }
        }

        outlineMode.forceFullSize = !useSeparateEraserCursor ? cfg.forceAlwaysFullSizedOutline() : cfg.forceAlwaysFullSizedEraserOutline();

        outlineMoveVector = outlineDocPoint - m_outlineDocPoint;

        m_outlineDocPoint = outlineDocPoint;
        m_currentOutline = getOutlinePath(m_outlineDocPoint, event, outlineMode);

        const PkRectF pkOutlinePixelRect = tryFixBrushOutline(m_currentOutline).boundingRect();
        outlinePixelRect = r44ToQRectF(pkOutlinePixelRect);
        outlineDocRect = r44ToQRectF(currentImage()->pixelToDocument(pkOutlinePixelRect));

        // This adjusted call is needed as we paint with a 3 pixel wide brush and the pen is outside the bounds of the path
        // Pen uses view coordinates so we have to zoom the document value to match 2 pixel in view coordinates
        // See BUG 275829
        qreal zoomX;
        qreal zoomY;
        canvas()->viewConverter()->zoom(&zoomX, &zoomY);
        qreal xoffset = 2.0/zoomX;
        qreal yoffset = 2.0/zoomY;

        if (!outlineDocRect.isEmpty()) {
            outlineDocRect.adjust(-xoffset,-yoffset,xoffset,yoffset);
        }

        colorPreviewDocUpdateRect = m_colorSamplerHelper.colorPreviewDocRect(m_outlineDocPoint);

        if (!colorPreviewDocUpdateRect.isEmpty()) {
            colorPreviewDocUpdateRect = colorPreviewDocUpdateRect.adjusted(-xoffset,-yoffset,xoffset,yoffset);
        }

    }

    // DIRTY HACK ALERT: we should fetch the assistant's dirty rect when requesting
    //                   the update, instead of just dumbly update the entire canvas!

    // WARNING: assistants code is also duplicated in KisDelegatedSelectPathWrapper::mouseMoveEvent

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    services->toolUpdateAssistantDecoration();

    if (!m_oldColorPreviewUpdateRect.isEmpty()) {
        services->toolUpdateOutlineDoc(m_oldColorPreviewUpdateRect);
    }

    if (!m_oldOutlineRect.isEmpty()) {
        services->toolUpdateOutlineDoc(m_oldOutlineRect);
    }

    if (!outlineDocRect.isEmpty()) {
        /**
         * A simple "update-ahead" implementation that issues an update a little
         * bigger to accommodate the possible following outline.
         *
         * The point is that canvas rendering comes through two stages of
         * compression and the canvas may request outline update when the
         * outline itself has already been changed. It causes visual tearing
         * on the screen (see https://bugs.kde.org/show_bug.cgi?id=476300).
         *
         * We can solve that in two ways:
         *
         * 1) Pass the actual outline with the update rect itself, which is
         *    a bit complicated and may result in the outline being a bit
         *    delayed visually. We don't implement this method (yet).
         *
         * 2) Just pass the update rect a bit bigger than the actual outline
         *    to accommodate a possible change in the outline. We calculate
         *    this bigger rect by offsetting the rect by the previous cursor
         *    offset.
         */

        /// Don't try to update-ahead if the offset is bigger than 50%
        /// of the brush outline
        const qreal maxUpdateAheadOutlinePortion = 0.5;

        /// 10% of extra move is added to offset
        const qreal offsetFuzzyExtension = 0.1;

        const qreal moveDistance = KisAlgebra2D::norm(outlineMoveVector);

        QRectF offsetRect;

        if (moveDistance < maxUpdateAheadOutlinePortion * KisAlgebra2D::maxDimension(outlineDocRect)) {
            offsetRect = outlineDocRect.translated((1.0 + offsetFuzzyExtension) * outlineMoveVector);
        }

        services->toolUpdateOutlineDoc(outlineDocRect | offsetRect);
    }

    if (!colorPreviewDocUpdateRect.isEmpty()) {
        services->toolUpdateOutlineDoc(colorPreviewDocUpdateRect);
    }

    m_oldOutlineRect = outlineDocRect;
    m_oldColorPreviewUpdateRect = colorPreviewDocUpdateRect;
}

bool KisToolPaint::isEraser() const {
    return canvas()->resourceManager()->resource(KoCanvasResource::CurrentEffectiveCompositeOp).toString() == r44ToQString(COMPOSITE_ERASE);
}

KisOptimizedBrushOutline KisToolPaint::getOutlinePath(const QPointF &documentPos,
                                                      const KoPointerEvent *event,
                                                      KisPaintOpSettings::OutlineMode outlineMode)
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);

    const QPointF pixelPos = convertToPixelCoord(documentPos);
    const PkPointF pkPixelPos = r44ToPkPointF(pixelPos);
    // When touch drawing, a "hover" event means the finger was just pressed
    // down. The last cursor position is invalid with regards to distance and
    // speed, since it isn't updated while the finger isn't down, so reset it.
    if (event && event->isTouchEvent() && mode() == HOVER_MODE) {
        m_d->lastCursorPos.reset(pkPixelPos);
    }

    KisPaintInformation info(pkPixelPos);
    info.setCanvasMirroredH(services->toolCanvasMirroredHorizontally());
    info.setCanvasMirroredV(services->toolCanvasMirroredVertically());
    info.setCanvasRotation(services->toolCanvasRotation());
    info.setRandomSource(new KisRandomSource());
    info.setPerStrokeRandomSource(new KisPerStrokeRandomSource());

    const qreal currentZoom = services->toolEffectiveZoom();

    PkPointF prevPoint = m_d->lastCursorPos.pushThroughHistory(pkPixelPos, currentZoom);
    qreal startAngle = KisAlgebra2D::directionBetweenPoints(prevPoint, pkPixelPos, 0);
    KisDistanceInformation distanceInfo(prevPoint, startAngle);

    KisPaintInformation::DistanceInformationRegistrar registrar =
        info.registerDistanceInformation(&distanceInfo);

    KisOptimizedBrushOutline path = currentPaintOpPreset()->settings()->
        brushOutline(info,
                     outlineMode, services->toolEffectivePhysicalZoom());

    return path;
}
