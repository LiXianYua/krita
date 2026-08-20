/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2014 Victor Lafon <metabolic.ewilan@hotmail.fr>

   SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "KoResourceBundleManifest.h"

#include <KoXmlNS.h>
#include <KoXmlWriter.h>
#include <PkStringHash.h>
#include <PkSet.h>
#include <PkStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include "ResourceDebug.h"

namespace {

PkString resourceTypeToManifestType(const PkString &type)
{
    if (type.startsWith(PkString("ko_"))) {
        return type.mid(3);
    }
    if (type.startsWith(PkString("kis_"))) {
        return type.mid(4);
    }
    return type;
}

PkXmlElement firstChildWithLocalName(const PkXmlElement &parent,
                                     const PkString &localName)
{
    for (PkXmlElement child = parent.firstChildElement();
         !child.isNull();
         child = child.nextSiblingElement()) {
        if (child.localName() == localName &&
            child.namespaceURI() == KoXmlNS::manifest) {
            return child;
        }
    }
    return PkXmlElement();
}

PkString bundleRelativePath(const PkString &fullPath,
                            const PkString &mediaType)
{
    const PkString prefix = mediaType + PkString("/");
    return fullPath.startsWith(prefix) ? fullPath.mid(prefix.size()) : fullPath;
}

} // namespace

KoResourceBundleManifest::ResourceReference::ResourceReference(
    const PkString &resourcePathValue,
    const PkStringList &tagListValue,
    const PkString &fileTypeNameValue,
    const PkString &md5,
    int resourceIdValue,
    const PkString &filenameInBundleValue)
    : resourcePath(resourcePathValue)
    , tagList(tagListValue)
    , fileTypeName(fileTypeNameValue)
    , md5sum(md5)
    , resourceId(resourceIdValue)
    , filenameInBundle(filenameInBundleValue.isEmpty()
                           ? resourcePathValue
                           : filenameInBundleValue)
{
}

KoResourceBundleManifest::KoResourceBundleManifest() = default;

KoResourceBundleManifest::~KoResourceBundleManifest() = default;

bool KoResourceBundleManifest::load(PkStream *device)
{
    m_resources.clear();
    if (!device) {
        return false;
    }
    if (!device->isOpen() && !device->open(PkStream::ReadOnly)) {
        return false;
    }

    PkXmlDocument manifestDocument;
    PkString errorMessage;
    int errorLine = -1;
    int errorColumn = -1;
    if (!manifestDocument.setContent(device, true, &errorMessage,
                                     &errorLine, &errorColumn)) {
        qCWarning(RESOURCE_LOG) << "Error parsing manifest" << errorMessage
                                << "line" << errorLine
                                << "column" << errorColumn;
        return false;
    }

    const PkXmlElement root = manifestDocument.documentElement();
    if (root.localName() != PkString("manifest") ||
        root.namespaceURI() != KoXmlNS::manifest) {
        return false;
    }

    for (PkXmlElement element = root.firstChildElement();
         !element.isNull();
         element = element.nextSiblingElement()) {
        if (!parseFileEntry(element)) {
            qCWarning(RESOURCE_LOG) << "Skipping invalid manifest entry"
                                    << "line" << element.lineNumber();
        }
    }
    return true;
}

bool KoResourceBundleManifest::parseFileEntry(const PkXmlElement &element)
{
    if (element.localName() != PkString("file-entry") ||
        element.namespaceURI() != KoXmlNS::manifest) {
        return false;
    }

    const PkString fullPath = element.attributeNS(
        KoXmlNS::manifest, PkString("full-path"));
    const PkString mediaType = element.attributeNS(
        KoXmlNS::manifest, PkString("media-type"));
    const PkString md5sum = element.attributeNS(
        KoXmlNS::manifest, PkString("md5sum"));

    if (fullPath == PkString("/") &&
        mediaType == PkString("application/x-krita-resourcebundle")) {
        return true;
    }
    if (fullPath.isEmpty() || mediaType.isEmpty() || md5sum.isEmpty()) {
        return false;
    }

    PkStringList tagList;
    const PkXmlElement tagsElement = firstChildWithLocalName(
        element, PkString("tags"));
    for (PkXmlElement tagElement = tagsElement.isNull()
             ? PkXmlElement()
             : tagsElement.firstChildElement();
         !tagElement.isNull();
         tagElement = tagElement.nextSiblingElement()) {
        if (tagElement.localName() == PkString("tag") &&
            tagElement.namespaceURI() == KoXmlNS::manifest) {
            tagList.append(tagElement.text());
        }
    }

    addResource(mediaType,
                fullPath,
                tagList,
                md5sum,
                -1,
                bundleRelativePath(fullPath, mediaType));
    return true;
}

bool KoResourceBundleManifest::save(PkStream *device)
{
    if (!device) {
        return false;
    }
    if (!device->isOpen() && !device->open(PkStream::WriteOnly)) {
        return false;
    }

    KoXmlWriter writer(device);
    writer.startDocument("manifest:manifest");
    writer.startElement("manifest:manifest");
    writer.addAttribute("xmlns:manifest", KoXmlNS::manifest);
    writer.addAttribute("manifest:version", "1.2");
    writer.addManifestEntry(PkString("/"),
                            PkString("application/x-krita-resourcebundle"));

    for (const PkString &resourceType : m_resources.keys()) {
        const PkMap<PkString, ResourceReference> typeResources =
            m_resources.value(resourceType);
        for (const ResourceReference &resource : typeResources.values()) {
            const PkString manifestType = resourceTypeToManifestType(resourceType);
            writer.startElement("manifest:file-entry");
            writer.addAttribute("manifest:media-type", manifestType);
            writer.addAttribute("manifest:full-path",
                                manifestType + PkString("/") +
                                    resource.filenameInBundle);
            writer.addAttribute("manifest:md5sum", resource.md5sum);
            if (!resource.tagList.isEmpty()) {
                writer.startElement("manifest:tags");
                for (const PkString &tag : resource.tagList) {
                    writer.startElement("manifest:tag");
                    writer.addTextNode(tag);
                    writer.endElement();
                }
                writer.endElement();
            }
            writer.endElement();
        }
    }

    writer.endElement();
    writer.endDocument();
    return true;
}

void KoResourceBundleManifest::addResource(
    const PkString &fileTypeName,
    const PkString &fileName,
    const PkStringList &fileTagList,
    const PkString &md5,
    int resourceId,
    const PkString &filenameInBundle)
{
    m_resources[fileTypeName].insert(
        fileName,
        ResourceReference(fileName, fileTagList, fileTypeName, md5,
                          resourceId, filenameInBundle));
}

void KoResourceBundleManifest::removeResource(ResourceReference &resource)
{
    if (!m_resources.contains(resource.fileTypeName)) {
        return;
    }
    PkMap<PkString, ResourceReference> &typeResources =
        m_resources[resource.fileTypeName];
    typeResources.remove(resource.resourcePath);
    if (typeResources.isEmpty()) {
        m_resources.remove(resource.fileTypeName);
    }
}

PkStringList KoResourceBundleManifest::types() const
{
    return PkStringList(m_resources.keys());
}

PkStringList KoResourceBundleManifest::tags() const
{
    PkSet<PkString> uniqueTags;
    for (const PkString &type : m_resources.keys()) {
        for (const ResourceReference &reference :
             m_resources.value(type).values()) {
            for (const PkString &tag : reference.tagList) {
                uniqueTags.insert(tag);
            }
        }
    }
    return PkStringList(uniqueTags.values());
}

PkList<KoResourceBundleManifest::ResourceReference>
KoResourceBundleManifest::files(const PkString &type) const
{
    PkList<ResourceReference> resources;
    if (type.isEmpty()) {
        for (const PkString &resourceType : m_resources.keys()) {
            resources += m_resources.value(resourceType).values();
        }
    } else if (m_resources.contains(type)) {
        resources = m_resources.value(type).values();
    }
    return resources;
}

void KoResourceBundleManifest::removeFile(PkString fileName)
{
    const PkStringList resourceTypes = m_resources.keys();
    for (const PkString &type : resourceTypes) {
        PkMap<PkString, ResourceReference> &typeResources = m_resources[type];
        typeResources.remove(fileName);
        if (typeResources.isEmpty()) {
            m_resources.remove(type);
        }
    }
}
