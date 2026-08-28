/*
 *  SPDX-FileCopyrightText: 2008, 2011 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 *  SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant.h"
#include "kis_painting_assistant_handle_p.h"
#include "kis_dom_utils.h"

#include <KoStore.h>

#include <PkXmlElement.h>
#include <PkXmlDocument.h>
#include <charconv>
#include <memory>
#include <utility>

namespace
{

PkColor defaultAssistantColor()
{
    // The settings lookup is a desktop concern.  Keep the historical fallback
    // value in the Qt-free model; M5 may inject the configured color.
    return PkColor(176, 176, 176, 255);
}

PkString decimalString(int value)
{
    return PkString("%1").arg(value);
}

PkString fixedThree(double value)
{
    char buffer[64];
    const std::to_chars_result result =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 3);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(result.ec == std::errc(), PkString());
    return PkString::PkFromUtf8(buffer, static_cast<int>(result.ptr - buffer));
}

}

void KisPaintingAssistantHandle::mergeWith(KisPaintingAssistantHandleSP handle)
{
    if(this->handleType()== HandleType::NORMAL || handle.data()->handleType()== HandleType::SIDE) {
        return;
    }


    for (KisPaintingAssistant* assistant : handle->d->assistants) {
        if (!assistant->handles().contains(this)) {
            assistant->replaceHandle(handle, this);
        }
    }
}

void KisPaintingAssistantHandle::uncache()
{
    for (KisPaintingAssistant* assistant : d->assistants) {
        assistant->uncache();
    }
}

struct KisPaintingAssistant::Private {
    Private();
    explicit Private(const Private &rhs);
    KisPaintingAssistantHandleSP reuseOrCreateHandle(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap, KisPaintingAssistantHandleSP origHandle, KisPaintingAssistant *q, bool registerAssistant = true);
    PkList<KisPaintingAssistantHandleSP> handles, sideHandles;

    KisPaintingAssistantHandleSP topLeft, bottomLeft, topRight, bottomRight, topMiddle, bottomMiddle, rightMiddle, leftMiddle;

    // share everything except handles between the clones
    struct SharedData {
        PkString id;
        PkString name;
        bool isSnappingActive {true};
        bool outlineVisible {true};
        bool isLocal {false};
        bool isLocked {false};
        //The isDuplicating flag only exists to draw the duplicate button depressed when pressed
        bool isDuplicating {false};
        bool followBrushPosition {false};
        bool adjustedPositionValid {false};
        PkPointF adjustedBrushPosition;

        PkPointF editorWidgetOffset {PkPointF(0, 0)};

        PkColor assistantGlobalColorCache = PkColor(255, 0, 0);

        bool useCustomColor {false};
        PkColor assistantCustomColor {defaultAssistantColor()};
    };

    PkSharedPointer<SharedData> s;

    int decorationThickness{1};

};

KisPaintingAssistant::Private::Private()
    : s(new SharedData)
{
}

KisPaintingAssistant::Private::Private(const Private &rhs)
    : s(rhs.s)
{
}

void KisPaintingAssistant::copySharedData(KisPaintingAssistantSP assistant)
{
    // Clones do not get a copy of the shared data, so this function is necessary to copy
    // the SharedData struct from the old assistant to this one. The function returns a
    // reference to a new SharedData object copied from the original
    this->d->s = PkSharedPointer<KisPaintingAssistant::Private::SharedData>(new KisPaintingAssistant::Private::SharedData);
    PkSharedPointer<KisPaintingAssistant::Private::SharedData> sd = assistant->d->s;
    *this->d->s = *sd;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::Private::reuseOrCreateHandle(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap, KisPaintingAssistantHandleSP origHandle, KisPaintingAssistant *q, bool registerAssistant)
{
    KisPaintingAssistantHandleSP mappedHandle = handleMap.value(origHandle);
    if (!mappedHandle) {
        if (origHandle) {
            mappedHandle = KisPaintingAssistantHandleSP(new KisPaintingAssistantHandle(*origHandle));
            mappedHandle->setType(origHandle->handleType());
            handleMap.insert(origHandle, mappedHandle);
        } else {
            mappedHandle = KisPaintingAssistantHandleSP();
        }
    }
    if (mappedHandle && registerAssistant) {
        mappedHandle->registerAssistant(q);
    }
    return mappedHandle;
}

bool KisPaintingAssistant::useCustomColor()
{
    return d->s->useCustomColor;
}

void KisPaintingAssistant::setUseCustomColor(bool useCustomColor)
{
    d->s->useCustomColor = useCustomColor;
}

void KisPaintingAssistant::setAssistantCustomColor(PkColor color)
{
    d->s->assistantCustomColor = color;
}

PkColor KisPaintingAssistant::assistantCustomColor()
{
    return d->s->assistantCustomColor;
}

void KisPaintingAssistant::setAssistantGlobalColorCache(const PkColor &color)
{
    d->s->assistantGlobalColorCache = color;
}

PkColor KisPaintingAssistant::effectiveAssistantColor() const
{
    return d->s->useCustomColor ? d->s->assistantCustomColor : d->s->assistantGlobalColorCache;
}

KisPaintingAssistant::KisPaintingAssistant(const PkString& id, const PkString& name) : d(new Private)
{
    d->s->id = id;
    d->s->name = name;
    d->s->isSnappingActive = true;
    d->s->outlineVisible = true;
}

KisPaintingAssistant::KisPaintingAssistant(
    const KisPaintingAssistant &rhs,
    PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : m_hasBeenInsideLocalRect(rhs.m_hasBeenInsideLocalRect)
    , d(new Private(*(rhs.d)))
{
    for (const KisPaintingAssistantHandleSP &origHandle : rhs.d->handles) {
        d->handles << d->reuseOrCreateHandle(handleMap, origHandle, this);
    }
    for (const KisPaintingAssistantHandleSP &origHandle : rhs.d->sideHandles) {
        d->sideHandles << d->reuseOrCreateHandle(handleMap, origHandle, this);
    }
#define _REUSE_H(name) d->name = d->reuseOrCreateHandle(handleMap, rhs.d->name, this, /* registerAssistant = */ false)
    _REUSE_H(topLeft);
    _REUSE_H(bottomLeft);
    _REUSE_H(topRight);
    _REUSE_H(bottomRight);
    _REUSE_H(topMiddle);
    _REUSE_H(bottomMiddle);
    _REUSE_H(rightMiddle);
    _REUSE_H(leftMiddle);
#undef _REUSE_H
}

bool KisPaintingAssistant::isSnappingActive() const
{
    return d->s->isSnappingActive;
}

void KisPaintingAssistant::setSnappingActive(bool set)
{
    d->s->isSnappingActive = set;
}

void KisPaintingAssistant::endStroke()
{
    d->s->adjustedPositionValid = false;
    d->s->followBrushPosition = false;
    m_hasBeenInsideLocalRect = false;
}

void KisPaintingAssistant::setAdjustedBrushPosition(const PkPointF position)
{
    d->s->adjustedBrushPosition = position;
    d->s->adjustedPositionValid = true;
}

void KisPaintingAssistant::setFollowBrushPosition(bool follow)
{
    d->s->followBrushPosition = follow;
}

PkPointF KisPaintingAssistant::getEditorPosition() const
{
    return getDefaultEditorPosition() + d->s->editorWidgetOffset;
}

bool KisPaintingAssistant::canBeLocal() const
{
    return false;
}

bool KisPaintingAssistant::isLocal() const
{
    return d->s->isLocal;
}

void KisPaintingAssistant::setLocal(bool value)
{
    d->s->isLocal = value;
}

bool KisPaintingAssistant::isLocked()
{
    return d->s->isLocked;
}

void KisPaintingAssistant::setLocked(bool value)
{
    d->s->isLocked = value;
}

void KisPaintingAssistant::setDuplicating(bool value)
{
    d->s->isDuplicating = value;
}

bool KisPaintingAssistant::isDuplicating()
{
    return d->s->isDuplicating;
}

PkPointF KisPaintingAssistant::editorWidgetOffset()
{
    return d->s->editorWidgetOffset;
}

void KisPaintingAssistant::setEditorWidgetOffset(PkPointF offset)
{
    d->s->editorWidgetOffset = offset;
}






void KisPaintingAssistant::initHandles(PkList<KisPaintingAssistantHandleSP> _handles)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->handles.isEmpty());
    d->handles = _handles;
    for (KisPaintingAssistantHandleSP handle : _handles) {
        handle->registerAssistant(this);
    }
}

KisPaintingAssistant::~KisPaintingAssistant()
{
    for (KisPaintingAssistantHandleSP handle : d->handles) {
        handle->unregisterAssistant(this);
    }
    if(!d->sideHandles.isEmpty()) {
        for (KisPaintingAssistantHandleSP handle : d->sideHandles) {
            handle->unregisterAssistant(this);
        }
    }
    delete d;
}

const PkString& KisPaintingAssistant::id() const
{
    return d->s->id;
}

const PkString& KisPaintingAssistant::name() const
{
    return d->s->name;
}

void KisPaintingAssistant::replaceHandle(KisPaintingAssistantHandleSP _handle, KisPaintingAssistantHandleSP _with)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->handles.contains(_handle));
    d->handles[d->handles.indexOf(_handle)] = _with;
    KIS_SAFE_ASSERT_RECOVER_RETURN(!d->handles.contains(_handle));
    _handle->unregisterAssistant(this);
    _with->registerAssistant(this);
}

void KisPaintingAssistant::addHandle(KisPaintingAssistantHandleSP handle, HandleType type)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!d->handles.contains(handle));
    if (HandleType::SIDE == type) {
        d->sideHandles.append(handle);
    } else {
        d->handles.append(handle);
    }

    handle->registerAssistant(this);
    handle.data()->setType(type);
}



void KisPaintingAssistant::uncache()
{
    // S-09/M5 GAP: there is no pixmap cache in the core layer.
}

PkRect KisPaintingAssistant::boundingRect() const
{
    PkRectF r;
    for (KisPaintingAssistantHandleSP h : handles()) {
        r = r.united(PkRectF(*h, PkSizeF(1,1)));
    }
    return r.adjusted(-2, -2, 2, 2).toAlignedRect();
}

bool KisPaintingAssistant::isAssistantComplete() const
{
    return true;
}

void KisPaintingAssistant::transform(const PkTransform &transform)
{
    for (KisPaintingAssistantHandleSP handle : handles()) {
        if (handle->chiefAssistant() != this) continue;

        *handle = transform.map(*handle);
    }

    for (KisPaintingAssistantHandleSP handle : sideHandles()) {
        if (handle->chiefAssistant() != this) continue;

        *handle = transform.map(*handle);
    }

    uncache();
}

PkByteArray KisPaintingAssistant::saveXml(PkMap<KisPaintingAssistantHandleSP, int> &handleMap)
{
    PkString text;
    PkXmlStreamWriter xml(&text);
    xml.writeStartDocument();
    xml.writeStartElement("assistant");
    xml.writeAttribute("type",d->s->id);
    xml.writeAttribute("active", decimalString(d->s->isSnappingActive));
    xml.writeAttribute("useCustomColor", decimalString(d->s->useCustomColor));
    xml.writeAttribute("customColor",  KisDomUtils::qColorToQString(d->s->assistantCustomColor));
    xml.writeAttribute("locked", decimalString(d->s->isLocked));
    xml.writeAttribute("editorWidgetOffset_X", fixedThree(d->s->editorWidgetOffset.x()));
    xml.writeAttribute("editorWidgetOffset_Y", fixedThree(d->s->editorWidgetOffset.y()));



    saveCustomXml(&xml); // if any specific assistants have custom XML data to save to

    // write individual handle data
    xml.writeStartElement("handles");
    for (const KisPaintingAssistantHandleSP &handle : d->handles) {
        int id = handleMap.size();
        if (!handleMap.contains(handle)){
            handleMap.insert(handle, id);
        }
        id = handleMap.value(handle);
        xml.writeStartElement("handle");
        xml.writeAttribute("id", decimalString(id));
        xml.writeAttribute("x", fixedThree(handle->x()));
        xml.writeAttribute("y", fixedThree(handle->y()));
        xml.writeEndElement();
    }
    xml.writeEndElement();
    if (!d->sideHandles.isEmpty()) { // for vanishing points only
	xml.writeStartElement("sidehandles");
	PkMap<KisPaintingAssistantHandleSP, int> sideHandleMap;
	for (KisPaintingAssistantHandleSP handle : d->sideHandles) {
	    int id = sideHandleMap.size();
	    sideHandleMap.insert(handle, id);
	    xml.writeStartElement("sidehandle");
	    xml.writeAttribute("id", decimalString(id));
	    xml.writeAttribute("x", fixedThree(handle->x()));
	    xml.writeAttribute("y", fixedThree(handle->y()));
	    xml.writeEndElement();
      }
    }

    xml.writeEndElement();
    xml.writeEndDocument();
    const std::string utf8 = text.PkToUtf8();
    return PkByteArray(utf8.data(), static_cast<int>(utf8.size()));
}

void KisPaintingAssistant::saveCustomXml(PkXmlStreamWriter* xml)
{
    (void)xml;
}

void KisPaintingAssistant::loadXml(KoStore* store, PkMap<int, KisPaintingAssistantHandleSP> &handleMap, PkString path)
{
    int id = 0;
    double x = 0.0, y = 0.0;
    store->open(path);
    const PkByteArray data = store->read(store->size());
    PkXmlStreamReader xml(PkString::PkFromUtf8(data.constData(), data.size()));
    PkMap<int, KisPaintingAssistantHandleSP> sideHandleMap;
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case PkXmlStreamReader::StartElement:
            if (xml.name() == "assistant") {

                auto active = xml.attributes().value("active");
                setSnappingActive( (active != "0")  );

                // load custom shared assistant properties
                if ( xml.attributes().hasAttribute("useCustomColor")) {
                    auto useCustomColor = xml.attributes().value("useCustomColor");

                    bool usingColor = false;
                    if (useCustomColor == "1") {
                        usingColor = true;
                    }


                    setUseCustomColor(usingColor);
                }

                if (xml.attributes().hasAttribute("editorWidgetOffset_X") && xml.attributes().hasAttribute("editorWidgetOffset_Y")) {
                    setEditorWidgetOffset(PkPointF(xml.attributes().value("editorWidgetOffset_X").toDouble(), xml.attributes().value("editorWidgetOffset_Y").toDouble()));
                }

                if ( xml.attributes().hasAttribute("customColor")) {
                    auto customColor = xml.attributes().value("customColor");
                    setAssistantCustomColor(KisDomUtils::qStringToQColor(customColor));

                }

                if ( xml.attributes().hasAttribute("locked")) {
                    auto locked = xml.attributes().value("locked");
                    setLocked(locked == "1");
                }

            }

            loadCustomXml(&xml);

            if (xml.name() == "handle") {
                PkString strId = xml.attributes().value("id"),
                        strX = xml.attributes().value("x"),
                        strY = xml.attributes().value("y");
                if (!strId.isEmpty() && !strX.isEmpty() && !strY.isEmpty()) {
                    id = strId.toInt();
                    x = strX.toDouble();
                    y = strY.toDouble();
                    if (!handleMap.contains(id)) {
                        handleMap.insert(id, new KisPaintingAssistantHandle(x, y));
                    }
                }
                addHandle(handleMap.value(id), HandleType::NORMAL);
            } else if (xml.name() == "sidehandle") {
                PkString strId = xml.attributes().value("id"),
                        strX = xml.attributes().value("x"),
                        strY = xml.attributes().value("y");
                if (!strId.isEmpty() && !strX.isEmpty() && !strY.isEmpty()) {
                    id = strId.toInt();
                    x = strX.toDouble();
                    y = strY.toDouble();
                    if (!sideHandleMap.contains(id)) {
                        sideHandleMap.insert(id, new KisPaintingAssistantHandle(x, y));
                    }
                }
                addHandle(sideHandleMap.value(id), HandleType::SIDE);

            }
            break;
        default:
            break;
        }
    }
    store->close();
}

bool KisPaintingAssistant::loadCustomXml(PkXmlStreamReader* xml)
{
    (void)xml;
    return true;
}

void KisPaintingAssistant::saveXmlList(PkXmlDocument& doc, PkXmlElement& assistantsElement,int count)
{
    if (d->s->id == "ellipse"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "ellipse");
        assistantElement.setAttribute("filename", PkString("ellipse%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "spline"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "spline");
        assistantElement.setAttribute("filename", PkString("spline%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "perspective"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "perspective");
        assistantElement.setAttribute("filename", PkString("perspective%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "vanishing point"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "vanishing point");
        assistantElement.setAttribute("filename", PkString("vanishing point%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "infinite ruler"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "infinite ruler");
        assistantElement.setAttribute("filename", PkString("infinite ruler%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "parallel ruler"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "parallel ruler");
        assistantElement.setAttribute("filename", PkString("parallel ruler%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "concentric ellipse"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "concentric ellipse");
        assistantElement.setAttribute("filename", PkString("concentric ellipse%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "fisheye-point"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "fisheye-point");
        assistantElement.setAttribute("filename", PkString("fisheye-point%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "ruler"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "ruler");
        assistantElement.setAttribute("filename", PkString("ruler%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "two point"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "two point");
        assistantElement.setAttribute("filename", PkString("two point%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "perspective ellipse"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "perspective ellipse");
        assistantElement.setAttribute("filename", PkString("perspective ellipse%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
    else if (d->s->id == "curvilinear-perspective"){
        PkXmlElement assistantElement = doc.createElement("assistant");
        assistantElement.setAttribute("type", "curvilinear-perspective");
        assistantElement.setAttribute("filename", PkString("curvilinear-perspective%1.assistant").arg(count));
        assistantsElement.appendChild(assistantElement);
    }
}

void KisPaintingAssistant::findPerspectiveAssistantHandleLocation() {
    PkList<KisPaintingAssistantHandleSP> hHandlesList;
    PkList<KisPaintingAssistantHandleSP> vHandlesList;
    uint vHole = 0,hHole = 0;
    KisPaintingAssistantHandleSP oppHandle;
    if (d->handles.size() == 4 && d->s->id == "perspective") {
        //get the handle opposite to the first handle
        oppHandle = oppHandleOne();
        //Sorting handles into two list, X sorted and Y sorted into hHandlesList and vHandlesList respectively.
        for (const KisPaintingAssistantHandleSP &handle : d->handles) {
            hHandlesList.append(handle);
            hHole = hHandlesList.size() - 1;
            vHandlesList.append(handle);
            vHole = vHandlesList.size() - 1;
            /*
             sort handles on the basis of X-coordinate
             */
            while(hHole > 0 && hHandlesList.at(hHole -1).data()->x() > handle.data()->x()) {
                std::swap(hHandlesList[hHole - 1], hHandlesList[hHole]);
                hHole = hHole - 1;
            }
            /*
             sort handles on the basis of Y-coordinate
             */
            while(vHole > 0 && vHandlesList.at(vHole -1).data()->y() > handle.data()->y()) {
                std::swap(vHandlesList[vHole - 1], vHandlesList[vHole]);
                vHole = vHole - 1;
            }
        }

        /*
         give the handles their respective positions
         */
        if(vHandlesList.at(0).data()->x() > vHandlesList.at(1).data()->x()) {
            d->topLeft = vHandlesList.at(1);
            d->topRight= vHandlesList.at(0);
        }
        else {
            d->topLeft = vHandlesList.at(0);
            d->topRight = vHandlesList.at(1);
        }
        if(vHandlesList.at(2).data()->x() > vHandlesList.at(3).data()->x()) {
            d->bottomLeft = vHandlesList.at(3);
            d->bottomRight = vHandlesList.at(2);
        }
        else {
            d->bottomLeft= vHandlesList.at(2);
            d->bottomRight = vHandlesList.at(3);
        }

        /*
         find if the handles that should be opposite are actually oppositely positioned
         */
        if (( (d->topLeft == d->handles.at(0).data() && d->bottomRight == oppHandle) ||
              (d->topLeft == oppHandle && d->bottomRight == d->handles.at(0).data()) ||
              (d->topRight == d->handles.at(0).data() && d->bottomLeft == oppHandle) ||
              (d->topRight == oppHandle && d->bottomLeft == d->handles.at(0).data()) ) )
        {}
        else {
            if(hHandlesList.at(0).data()->y() > hHandlesList.at(1).data()->y()) {
                d->topLeft = hHandlesList.at(1);
                d->bottomLeft= hHandlesList.at(0);
            }
            else {
                d->topLeft = hHandlesList.at(0);
                d->bottomLeft = hHandlesList.at(1);
            }
            if(hHandlesList.at(2).data()->y() > hHandlesList.at(3).data()->y()) {
                d->topRight = hHandlesList.at(3);
                d->bottomRight = hHandlesList.at(2);
            }
            else {
                d->topRight= hHandlesList.at(2);
                d->bottomRight = hHandlesList.at(3);
            }

        }
        /*
         Setting the middle handles as needed
         */
        if(!d->bottomMiddle && !d->topMiddle && !d->leftMiddle && !d->rightMiddle) {
  
            // Before re-adding the handles, clear old ones that have been
            // potentially loaded from disk and not re-associated with the
            // xxxMiddle pointers in d; otherwise those would stay in place.
            if(!d->sideHandles.isEmpty()) {
                for (KisPaintingAssistantHandleSP handle : d->sideHandles) {
                    handle->unregisterAssistant(this);
                }
                d->sideHandles.clear();
            }
          
            d->bottomMiddle = new KisPaintingAssistantHandle((d->bottomLeft.data()->x() + d->bottomRight.data()->x())*0.5,
                                                             (d->bottomLeft.data()->y() + d->bottomRight.data()->y())*0.5);
            d->topMiddle = new KisPaintingAssistantHandle((d->topLeft.data()->x() + d->topRight.data()->x())*0.5,
                                                          (d->topLeft.data()->y() + d->topRight.data()->y())*0.5);
            d->rightMiddle= new KisPaintingAssistantHandle((d->topRight.data()->x() + d->bottomRight.data()->x())*0.5,
                                                           (d->topRight.data()->y() + d->bottomRight.data()->y())*0.5);
            d->leftMiddle= new KisPaintingAssistantHandle((d->bottomLeft.data()->x() + d->topLeft.data()->x())*0.5,
                                                          (d->bottomLeft.data()->y() + d->topLeft.data()->y())*0.5);
            
            addHandle(d->rightMiddle, HandleType::SIDE);
            addHandle(d->leftMiddle, HandleType::SIDE);
            addHandle(d->bottomMiddle, HandleType::SIDE);
            addHandle(d->topMiddle, HandleType::SIDE);
        }
        else
        {
            d->bottomMiddle.data()->operator =(PkPointF((d->bottomLeft.data()->x() + d->bottomRight.data()->x())*0.5,
                                                       (d->bottomLeft.data()->y() + d->bottomRight.data()->y())*0.5));
            d->topMiddle.data()->operator =(PkPointF((d->topLeft.data()->x() + d->topRight.data()->x())*0.5,
                                                    (d->topLeft.data()->y() + d->topRight.data()->y())*0.5));
            d->rightMiddle.data()->operator =(PkPointF((d->topRight.data()->x() + d->bottomRight.data()->x())*0.5,
                                                      (d->topRight.data()->y() + d->bottomRight.data()->y())*0.5));
            d->leftMiddle.data()->operator =(PkPointF((d->bottomLeft.data()->x() + d->topLeft.data()->x())*0.5,
                                                     (d->bottomLeft.data()->y() + d->topLeft.data()->y())*0.5));
        }

    }
}

KisPaintingAssistantHandleSP KisPaintingAssistant::oppHandleOne()
{
    PkPointF intersection(0,0);
    if((PkLineF(d->handles.at(0).data()->toPoint(),d->handles.at(1).data()->toPoint()).intersects(PkLineF(d->handles.at(2).data()->toPoint(),d->handles.at(3).data()->toPoint()), &intersection) != PkLineF::NoIntersection)
            && (PkLineF(d->handles.at(0).data()->toPoint(),d->handles.at(1).data()->toPoint()).intersects(PkLineF(d->handles.at(2).data()->toPoint(),d->handles.at(3).data()->toPoint()), &intersection) != PkLineF::UnboundedIntersection))
    {
        return d->handles.at(1);
    }
    else if((PkLineF(d->handles.at(0).data()->toPoint(),d->handles.at(2).data()->toPoint()).intersects(PkLineF(d->handles.at(1).data()->toPoint(),d->handles.at(3).data()->toPoint()), &intersection) != PkLineF::NoIntersection)
            && (PkLineF(d->handles.at(0).data()->toPoint(),d->handles.at(2).data()->toPoint()).intersects(PkLineF(d->handles.at(1).data()->toPoint(),d->handles.at(3).data()->toPoint()), &intersection) != PkLineF::UnboundedIntersection))
    {
        return d->handles.at(2);
    }
    else
    {
        return d->handles.at(3);
    }
}

KisPaintingAssistantHandleSP KisPaintingAssistant::topLeft()
{
    return d->topLeft;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::topLeft() const
{
    return d->topLeft;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::bottomLeft()
{
    return d->bottomLeft;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::bottomLeft() const
{
    return d->bottomLeft;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::topRight()
{
    return d->topRight;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::topRight() const
{
    return d->topRight;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::bottomRight()
{
    return d->bottomRight;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::bottomRight() const
{
    return d->bottomRight;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::topMiddle()
{
    return d->topMiddle;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::topMiddle() const
{
    return d->topMiddle;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::bottomMiddle()
{
    return d->bottomMiddle;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::bottomMiddle() const
{
    return d->bottomMiddle;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::rightMiddle()
{
    return d->rightMiddle;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::rightMiddle() const
{
    return d->rightMiddle;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::leftMiddle()
{
    return d->leftMiddle;
}

const KisPaintingAssistantHandleSP KisPaintingAssistant::leftMiddle() const
{
    return d->leftMiddle;
}

const PkList<KisPaintingAssistantHandleSP>& KisPaintingAssistant::handles() const
{
    return d->handles;
}

PkList<KisPaintingAssistantHandleSP> KisPaintingAssistant::handles()
{
    return d->handles;
}

const PkList<KisPaintingAssistantHandleSP>& KisPaintingAssistant::sideHandles() const
{
    return d->sideHandles;
}

PkList<KisPaintingAssistantHandleSP> KisPaintingAssistant::sideHandles()
{
    return d->sideHandles;
}



KisPaintingAssistantHandleSP KisPaintingAssistant::firstLocalHandle() const
{
    return 0;
}

KisPaintingAssistantHandleSP KisPaintingAssistant::secondLocalHandle() const
{
    return 0;
}

PkRectF KisPaintingAssistant::getLocalRect() const
{
    if (!isLocal() || !firstLocalHandle() || !secondLocalHandle()) {
        return PkRectF();
    }

    KisPaintingAssistantHandleSP first = firstLocalHandle();
    KisPaintingAssistantHandleSP second = secondLocalHandle();

    PkPointF topLeft = PkPointF(qMin(first->x(), second->x()), qMin(first->y(), second->y()));
    PkPointF bottomRight = PkPointF(qMax(first->x(), second->x()), qMax(first->y(), second->y()));

    PkRectF rect(topLeft, bottomRight);
    return rect;
}

double KisPaintingAssistant::norm2(const PkPointF& p)
{
    return p.x() * p.x() + p.y() * p.y();
}

void KisPaintingAssistant::setDecorationThickness(int thickness)
{
    d->decorationThickness = thickness;
}

PkList<KisPaintingAssistantSP> KisPaintingAssistant::cloneAssistantList(const PkList<KisPaintingAssistantSP> &list)
{
    PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> handleMap;
    PkList<KisPaintingAssistantSP> clonedList;
    for (auto i = list.begin(); i != list.end(); ++i) {
        clonedList << (*i)->clone(handleMap);
    }
    return clonedList;
}
