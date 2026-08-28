/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PAINTING_ASSISTANT_H_
#define _KIS_PAINTING_ASSISTANT_H_

#include <PkString.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkColor.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkSize.h>
#include <PkTransform.h>
#include <PkAuxTypes.h>
#include <PkXmlStreamReader.h>
#include <PkXmlStreamWriter.h>
#include <PkStringHash.h>

#include <cassert>

#include <kritaassistanttool_export.h>
#include <kis_shared.h>
#include <PkSharedPointer.h>

class PkRect;
class PkRectF;
class KoStore;
class PkXmlDocument;
class PkXmlElement;

#include <kis_shared_ptr.h>
#include <KoGenericRegistry.h>

class KisPaintingAssistantHandle;
typedef KisSharedPtr<KisPaintingAssistantHandle> KisPaintingAssistantHandleSP;
class KisPaintingAssistant;
typedef PkSharedPointer<KisPaintingAssistant> KisPaintingAssistantSP;

enum HandleType {
    NORMAL,
    SIDE,
    CORNER,
    VANISHING_POINT,
    ANCHOR
};

/**
 * Tool-facing assistant lifecycle capability. It is separate from the
 * rendering layer so tools do not gain access to assistant decorations.
 */
class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantToolServices
{
public:
    virtual ~KisPaintingAssistantToolServices() = default;

    virtual void endStroke() = 0;
    virtual void updateDecorationIfNeeded() = 0;
};


/**
  * Represent an handle of the assistant, used to edit the parameters
  * of an assistants. Handles can be shared between assistants.
  */
class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantHandle : public PkPointF, public KisShared
{
    friend class KisPaintingAssistant;

public:
    KisPaintingAssistantHandle(double x, double y);
    explicit KisPaintingAssistantHandle(PkPointF p);
    KisPaintingAssistantHandle(const KisPaintingAssistantHandle&);
    ~KisPaintingAssistantHandle();
    void mergeWith(KisPaintingAssistantHandleSP);
    void uncache();
    KisPaintingAssistantHandle& operator=(const PkPointF&);
    void setType(char type);
    char handleType() const;

    /**
     * Returns the pointer to the "chief" assistant,
     * which is supposed to handle transformations of the
     * handle, when all the assistants are transformed
     */
    KisPaintingAssistant* chiefAssistant() const;

private:
    void registerAssistant(KisPaintingAssistant*);
    void unregisterAssistant(KisPaintingAssistant*);
    bool containsAssistant(KisPaintingAssistant*) const;

private:
    struct Private;
    Private* const d;
};

/**
 * A KisPaintingAssistant is an object that assist the drawing on the canvas.
 * With this class you can implement virtual equivalent to ruler or compass.
 */
class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistant
{
public:
    KisPaintingAssistant(const PkString& id, const PkString& name);
    virtual ~KisPaintingAssistant();
    virtual KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const = 0;
    const PkString& id() const;
    const PkString& name() const;
    bool isSnappingActive() const;
    void setSnappingActive(bool set);
    //copy SharedData from an assistant to this
    void copySharedData(KisPaintingAssistantSP assistant);


    /**
     * Adjust the position given in parameter.
     * @param point the coordinates in point in the document reference
     * @param strokeBegin the coordinates of the beginning of the stroke
     * @param snapToAny because now assistants can be composited out of multiple inside assistants.
     *         snapToAny true means that you can use any of the inside assistant, while it being false
     *         means you should use the last used one. The logic determining when it happens (first stroke etc.)
     *         is in the decoration, so those two options are enough.
     * @param moveThresholdPt the threshold for the "move" of the cursor measured in pt
     *                        (usually equals to 2px in screen coordinates converted to pt)
     */
    virtual PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, bool snapToAny, qreal moveThresholdPt) = 0;
    virtual void adjustLine(PkPointF& point, PkPointF& strokeBegin) = 0;
    virtual void endStroke();
    virtual void setAdjustedBrushPosition(const PkPointF position);
    virtual void setFollowBrushPosition(bool follow);
    virtual PkPointF getDefaultEditorPosition() const = 0; // Returns standard editor widget position for this assistant
    virtual PkPointF getEditorPosition() const; // Returns editor widget position in document-space coordinates.
    virtual int numHandles() const = 0;

    /**
     * @brief canBeLocal
     * @return if the assistant can be potentially a "local assistant" (limited to rectangular area) or not
     */
    virtual bool canBeLocal() const;
    /**
     * @brief isLocal
     * @return if the assistant is limited to a rectangular area or not
     */
    bool isLocal() const;
    /**
     * @brief setLocal
     * @param value set the indication if the assistant is limited to a rectangular area or not
     */
    void setLocal(bool value);

    /**
     * @brief isLocked
     * @return if the assistant is locked (= cannot be moved, or edited in any way), or not
     */
    bool isLocked();
    /**
     * @brief setLocked
     * @param value set the indication if the assistant is locked (= cannot be moved, or edited in any way) or not
     */
    void setLocked(bool value);
    /**
     * @brief isDuplicating
     * @return If the duplication button is pressed
     */
    /*The duplication button must be depressed when the user clicks it. This getter function indicates to the
    render function when the button is clicked*/
    bool isDuplicating();
    /**
     * @brief setDuplicating
     * @param value setter function sets the indication that the duplication button is pressed
     */
    void setDuplicating(bool value);

    PkPointF editorWidgetOffset();
    void setEditorWidgetOffset(PkPointF offset);

    void replaceHandle(KisPaintingAssistantHandleSP _handle, KisPaintingAssistantHandleSP _with);
    void addHandle(KisPaintingAssistantHandleSP handle, HandleType type);

    PkColor effectiveAssistantColor() const;
    bool useCustomColor();
    void setUseCustomColor(bool useCustomColor);
    void setAssistantCustomColor(PkColor color);
    PkColor assistantCustomColor();
    void setAssistantGlobalColorCache(const PkColor &color);

    // S-09/M5 GAP: painter/cursor/cache hooks remain owned by the desktop ABI.
    // This Qt-free layer intentionally exposes only model, geometry and persistence.
    void uncache();
    const PkList<KisPaintingAssistantHandleSP>& handles() const;
    PkList<KisPaintingAssistantHandleSP> handles();
    const PkList<KisPaintingAssistantHandleSP>& sideHandles() const;
    PkList<KisPaintingAssistantHandleSP> sideHandles();

    PkByteArray saveXml( PkMap<KisPaintingAssistantHandleSP, int> &handleMap);
    virtual void saveCustomXml(PkXmlStreamWriter* xml); //in case specific assistants have custom properties (like vanishing point)

    void loadXml(KoStore *store, PkMap<int, KisPaintingAssistantHandleSP> &handleMap, PkString path);
    virtual bool loadCustomXml(PkXmlStreamReader* xml);

    void saveXmlList(PkXmlDocument& doc, PkXmlElement& assistantsElement, int count);
    void findPerspectiveAssistantHandleLocation();
    KisPaintingAssistantHandleSP oppHandleOne();

    /**
      * Get the topLeft, bottomLeft, topRight and BottomRight corners of the assistant
      * Some assistants like the perspective grid have custom logic built around certain handles
      */
    const KisPaintingAssistantHandleSP topLeft() const;
    KisPaintingAssistantHandleSP topLeft();
    const KisPaintingAssistantHandleSP topRight() const;
    KisPaintingAssistantHandleSP topRight();
    const KisPaintingAssistantHandleSP bottomLeft() const;
    KisPaintingAssistantHandleSP bottomLeft();
    const KisPaintingAssistantHandleSP bottomRight() const;
    KisPaintingAssistantHandleSP bottomRight();
    const KisPaintingAssistantHandleSP topMiddle() const;
    KisPaintingAssistantHandleSP topMiddle();
    const KisPaintingAssistantHandleSP rightMiddle() const;
    KisPaintingAssistantHandleSP rightMiddle();
    const KisPaintingAssistantHandleSP leftMiddle() const;
    KisPaintingAssistantHandleSP leftMiddle();
    const KisPaintingAssistantHandleSP bottomMiddle() const;
    KisPaintingAssistantHandleSP bottomMiddle();


    /// determines if the assistant has enough handles to be considered created
    /// new assistants get in a "creation" phase where they are currently being made on the canvas
    /// it will return false if we are in the middle of creating the assistant.
    virtual bool isAssistantComplete() const;

    /// Transform the assistant using the given \p transform. Please note that \p transform
    /// should be in 'document' coordinate system.
    /// Used with image-wide transformations.
    virtual void transform(const PkTransform &transform);

public:
    static double norm2(const PkPointF& p);

    void setDecorationThickness(int thickness);

protected:
    explicit KisPaintingAssistant(const KisPaintingAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

    virtual PkRect boundingRect() const;

    /** Refresh assistant-owned state derived from handles without rendering. */
    virtual void updateModelState();
    void synchronizeModelState();

    void initHandles(PkList<KisPaintingAssistantHandleSP> _handles);
    PkList<KisPaintingAssistantHandleSP> m_handles;

    /**
     * @brief firstLocalHandle
     * Note: this doesn't guarantee it will be the topleft corner!
     * For that, use getLocalRect().topLeft()
     * The only purpose of those functions to exist is to be able to
     * put getLocalRect() function in the KisPaintingAssistant
     * instead of reimplementing it in every specific assistant.
     * @return the first handle of the rectangle of the limited area
     */
    virtual KisPaintingAssistantHandleSP firstLocalHandle() const;
    /**
     * @brief secondLocalHandle
     * Note: this doesn't guarantee it will be the bottomRight corner!
     * For that, use getLocalRect().bottomRight()
     * (and remember that for PkRect bottomRight() works differently than for PkRectF,
     * so don't convert to PkRect before accessing the corner)
     * @return
     */
    virtual KisPaintingAssistantHandleSP secondLocalHandle() const;
    /**
     * @brief getLocalRect
     * The function deals with local handles not being topLeft and bottomRight
     * gracefully and returns a correct rectangle.
     * Thanks to that the user can place handles in a "wrong" order or move them around
     * but the local rectangle will still be correct.
     * @return the rectangle of the area that the assistant is limited to
     */
    PkRectF getLocalRect() const;


public:
    /// clones the list of assistants
    /// the originally shared handles will still be shared
    /// the cloned assistants do not share any handle with the original assistants
    static PkList<KisPaintingAssistantSP> cloneAssistantList(const PkList<KisPaintingAssistantSP> &list);

protected:
    bool m_hasBeenInsideLocalRect {false};

private:
    struct Private;
    Private* const d;

};

/**
 * Allow to create a painting assistant.
 */
class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantFactory
{
public:
    KisPaintingAssistantFactory();
    virtual ~KisPaintingAssistantFactory();
    virtual PkString id() const = 0;
    virtual PkString name() const = 0;
    virtual KisPaintingAssistant* createPaintingAssistant() const = 0;

};

class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantFactoryRegistry : public KoGenericRegistry<KisPaintingAssistantFactory*>
{
  public:
    KisPaintingAssistantFactoryRegistry();
    ~KisPaintingAssistantFactoryRegistry() override;

    static KisPaintingAssistantFactoryRegistry* instance();

};

#endif
