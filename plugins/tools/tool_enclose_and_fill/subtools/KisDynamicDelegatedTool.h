/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDYNAMICDELEGATEDTOOL_H
#define KISDYNAMICDELEGATEDTOOL_H

#include <functional>
#include <tuple>
#include <type_traits>

#include <PkPainter.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSet.h>
#include <PkVariant.h>

#include <KoToolBase.h>
#include <KoToolBase_p.h>
#include <KoToolFactoryBase.h>
#include <KoCanvasBase.h>
#include <KoPointerEvent.h>
#include <KoDocumentResourceManager.h>
#include <KoCanvasResourceProvider.h>
#include <KoViewConverter.h>
#include <KoShapeController.h>
#include <KoShapeControllerBase.h>
#include <KoToolSelection.h>
#include <KoCanvasController.h>

template <typename Method>
struct KisDynamicMethodTraits;

template <typename Class, typename Return, typename... Args>
struct KisDynamicMethodTraits<Return (Class::*)(Args...)>
{
    using ReturnType = Return;
    template <std::size_t Index>
    using Argument = typename std::tuple_element<Index, std::tuple<Args...>>::type;
};

template <typename Class, typename Return, typename... Args>
struct KisDynamicMethodTraits<Return (Class::*)(Args...) const>
{
    using ReturnType = Return;
    template <std::size_t Index>
    using Argument = typename std::tuple_element<Index, std::tuple<Args...>>::type;
};

template <typename BaseClass>
class KisDynamicDelegateTool : public BaseClass
{
public:
    template <typename ... Args> 
    explicit KisDynamicDelegateTool(Args ... args) : BaseClass(args...) {}
    KisDynamicDelegateTool() = delete;
    KisDynamicDelegateTool(const KisDynamicDelegateTool&) = delete;
    KisDynamicDelegateTool& operator=(const KisDynamicDelegateTool&) = delete;
    ~KisDynamicDelegateTool() override {}

    using BaseClass::createOptionWidget;
    using BaseClass::listeningToModifiers;
    using BaseClass::listenToModifiers;
    using BaseClass::cursor;
    using BaseClass::setMode;
    using BaseClass::mode;
    using BaseClass::setCursor;
    using BaseClass::resetCursorStyle;
    using BaseClass::canvasResourceChanged;
    using BaseClass::paint;
    using BaseClass::activatePrimaryAction;
    using BaseClass::deactivatePrimaryAction;
    using BaseClass::activateAlternateAction;
    using BaseClass::deactivateAlternateAction;
    using BaseClass::beginAlternateAction;
    using BaseClass::continueAlternateAction;
    using BaseClass::endAlternateAction;
    using BaseClass::requestUpdateOutline;
    using BaseClass::getOutlinePath;
};

template <typename BaseClass>
class KisDynamicDelegatedTool : public BaseClass
{
public:
    using DelegateType = KisDynamicDelegateTool<BaseClass>;

    using CursorType = typename KisDynamicMethodTraits<decltype(&BaseClass::cursor)>::ReturnType;
    using KeyPressEvent = typename KisDynamicMethodTraits<decltype(&BaseClass::keyPressEvent)>::template Argument<0>;
    using KeyReleaseEvent = typename KisDynamicMethodTraits<decltype(&BaseClass::keyReleaseEvent)>::template Argument<0>;
    using PopupMenu = typename KisDynamicMethodTraits<decltype(&BaseClass::popupActionsMenu)>::ReturnType;

    explicit KisDynamicDelegatedTool(KoCanvasBase *canvas)
        : BaseClass(canvas, CursorType())
        , m_delegateTool(nullptr)
    {}

    KisDynamicDelegatedTool() = delete;
    KisDynamicDelegatedTool(const KisDynamicDelegatedTool&) = delete;
    KisDynamicDelegatedTool& operator=(const KisDynamicDelegatedTool&) = delete;

    ~KisDynamicDelegatedTool() override
    {
        if (m_delegateTool) {
            delete m_delegateTool;
        }
    }

    DelegateType* delegateTool() const
    {
        return m_delegateTool;
    }

    void setDelegateTool(DelegateType *newDelegateTool)
    {
        if (m_delegateTool == newDelegateTool) {
            return;
        }
        if (m_delegateTool) {
            delete m_delegateTool;
        }
        m_delegateTool = newDelegateTool;
        if (m_delegateTool) {
            BaseClass::connect(m_delegateTool, &DelegateType::activateTool,
                               this, &BaseClass::activateTool);
            BaseClass::connect(m_delegateTool, &DelegateType::cursorChanged,
                               this, [this](const auto &cursor) { this->BaseClass::useCursor(cursor); });
            BaseClass::connect(m_delegateTool, &DelegateType::selectionChanged,
                               this, &BaseClass::selectionChanged);
            BaseClass::connect(m_delegateTool, &DelegateType::statusTextChanged,
                               this, &BaseClass::statusTextChanged);
        }
    }

    PkRectF decorationsRect() const override
    {
        if (m_delegateTool) return m_delegateTool->decorationsRect();
        return PkRect();
    }

    bool wantsAutoScroll() const override
    {
        if (m_delegateTool) return m_delegateTool->wantsAutoScroll();
        return false;
    }

    void paint(PkPainter &painter, const KoViewConverter &converter) override
    {
        if (m_delegateTool) m_delegateTool->paint(painter, converter);
    }

    void mousePressEvent(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->mousePressEvent(event);
    }

    void mouseDoubleClickEvent(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->mouseDoubleClickEvent(event);
    }

    void mouseTripleClickEvent(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->mouseTripleClickEvent(event);
    }

    void mouseMoveEvent(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->mouseMoveEvent(event);
    }

    void mouseReleaseEvent(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->mouseReleaseEvent(event);
    }

    void keyPressEvent(KeyPressEvent event) override
    {
        if (m_delegateTool) m_delegateTool->keyPressEvent(event);
    }

    void keyReleaseEvent(KeyReleaseEvent event) override
    {
        if (m_delegateTool) m_delegateTool->keyReleaseEvent(event);
    }

    void explicitUserStrokeEndRequest() override
    {
        if (m_delegateTool) m_delegateTool->explicitUserStrokeEndRequest();
    }

    PopupMenu popupActionsMenu() override
    {
        if (m_delegateTool) return m_delegateTool->popupActionsMenu();
        return nullptr;
    }

    KisPopupWidgetInterface* popupWidget() override
    {
        if (m_delegateTool) return m_delegateTool->popupWidget();
        return nullptr;
    }

    void activatePrimaryAction() override
    {
        if (m_delegateTool) m_delegateTool->activatePrimaryAction();
    }

    void deactivatePrimaryAction() override
    {
        if (m_delegateTool) m_delegateTool->deactivatePrimaryAction();
    }

    void beginPrimaryAction(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->beginPrimaryAction(event);
    }

    void continuePrimaryAction(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->continuePrimaryAction(event);
    }

    void endPrimaryAction(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->endPrimaryAction(event);
    }

    void beginPrimaryDoubleClickAction(KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->beginPrimaryDoubleClickAction(event);
    }

    bool primaryActionSupportsHiResEvents() const override
    {
        if (m_delegateTool) return m_delegateTool->primaryActionSupportsHiResEvents();
        return false;
    }

    void activateAlternateAction(typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->activateAlternateAction(action);
    }

    void deactivateAlternateAction(typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->deactivateAlternateAction(action);
    }

    void beginAlternateAction(KoPointerEvent *event, typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->beginAlternateAction(event, action);
    }

    void continueAlternateAction(KoPointerEvent *event, typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->continueAlternateAction(event, action);
    }

    void endAlternateAction(KoPointerEvent *event, typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->endAlternateAction(event, action);
    }

    void beginAlternateDoubleClickAction(KoPointerEvent *event, typename BaseClass::AlternateAction action) override
    {
        if (m_delegateTool) m_delegateTool->beginAlternateDoubleClickAction(event, action);
    }

    bool alternateActionSupportsHiResEvents(typename BaseClass::AlternateAction action) const override
    {
        if (m_delegateTool) return m_delegateTool->alternateActionSupportsHiResEvents(action);
        return false;
    }

    void newActivationWithExternalSource(KisPaintDeviceSP externalSource) override
    {
        if (m_delegateTool) m_delegateTool->newActivationWithExternalSource(externalSource);
    }

    void requestUpdateOutline(const PkPointF &outlineDocPoint, const KoPointerEvent *event) override
    {
        if (m_delegateTool) m_delegateTool->requestUpdateOutline(outlineDocPoint, event);
    }

public:
    void requestUndoDuringStroke() override
    {
        if (m_delegateTool) m_delegateTool->requestUndoDuringStroke();
    }

    void requestRedoDuringStroke() override
    {
        if (m_delegateTool) m_delegateTool->requestRedoDuringStroke();
    }

    void requestStrokeCancellation() override
    {
        if (m_delegateTool) m_delegateTool->requestStrokeCancellation();
    }

    void requestStrokeEnd() override
    {
        if (m_delegateTool) m_delegateTool->requestStrokeEnd();
    }

    void activate(const PkSet<KoShape*> &shapes) override
    {
        if (m_delegateTool) m_delegateTool->activate(shapes);
        KisTool::activate(shapes);
    }

    void deactivate() override
    {
        if (m_delegateTool) m_delegateTool->deactivate();
        KisTool::deactivate();
    }

    void canvasResourceChanged(int key, const PkVariant &res) override
    {
        if (m_delegateTool) m_delegateTool->canvasResourceChanged(key, res);
    }

    void documentResourceChanged(int key, const PkVariant &res) override
    {
        if (m_delegateTool) m_delegateTool->documentResourceChanged(key, res);
    }

    void repaintDecorations() override
    {
        if (m_delegateTool) m_delegateTool->repaintDecorations();
    }

    void updateSettingsViews() override
    {
        if (m_delegateTool) m_delegateTool->updateSettingsViews();
    }

protected:
    bool listeningToModifiers() override
    {
        if (m_delegateTool) return m_delegateTool->listeningToModifiers();
        return false;
    }

    void listenToModifiers(bool listen) override
    {
        if (m_delegateTool) m_delegateTool->listenToModifiers(listen);
    }

    CursorType cursor() const
    {
        if (m_delegateTool) return m_delegateTool->cursor();
        return BaseClass::cursor();
    }

    void setCursor(const CursorType &cursor)
    {
        if (m_delegateTool) m_delegateTool->setCursor(cursor);
        BaseClass::setCursor(cursor);
    }
    
    void setMode(typename BaseClass::ToolMode mode) override
    {
        if (m_delegateTool) m_delegateTool->setMode(mode);
        BaseClass::setMode(mode);
    }

    typename BaseClass::ToolMode mode() const override
    {
        if (m_delegateTool) return m_delegateTool->mode();
        return BaseClass::mode();
    }

protected:
    void resetCursorStyle() override
    {
        if (m_delegateTool) m_delegateTool->resetCursorStyle();
        BaseClass::resetCursorStyle();
    }

private:
    DelegateType *m_delegateTool;
    
};

#endif
