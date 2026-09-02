/* This file is part of the KDE project
   SPDX-FileCopyrightText: 1998, 1999, 2000 Torben Weis <weis@kde.org>
   SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2006 Martin Pfeiffer <hubipete@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KO_DOCUMENT_INFO_H
#define KO_DOCUMENT_INFO_H

#include <PkObject.h>
#include <compat/QObject>
#include <PkMap.h>
#include <PkString.h>
#include <PkStringList.h>

#include "kritaimpex_export.h"

class PkXmlDocument;
class PkXmlElement;
class KoStore;

/**
 * @short The class containing all meta information about a document
 *
 * @author Torben Weis <weis@kde.org>
 * @author David Faure <faure@kde.org>
 * @author Martin Pfeiffer <hubipete@gmx.net>
 * @see KoDocumentInfoDlg
 *
 * This class contains the meta information for a document. They are
 * stored in two PkMap and can be accessed through aboutInfo() and authorInfo().
 * The about info can be changed with setAboutInfo() and setAuthorInfo()
 */
class KRITAIMPEX_EXPORT KoDocumentInfo : public PkObject
{
public:
    /**
     * The constructor
     * @param parent a pointer to the parent object
     */
    explicit KoDocumentInfo(PkObject *parent = 0);
    explicit KoDocumentInfo(const KoDocumentInfo &rhs, PkObject *parent = 0);

    /** The destructor */
    ~KoDocumentInfo() override;
    /**
     * Load the KoDocumentInfo from an Calligra-1.3 DomDocument
     * @param doc the PkXmlDocument to load from
     * @return true if success
     */
    bool load(const PkXmlDocument& doc);

    /**
     * Save the KoDocumentInfo to an Calligra-1.3 DomDocument
     * @param autosaving whether the owning document is being autosaved
     * @param documentModified whether the owning document has unsaved changes
     * @return the PkXmlDocument to which was saved
     */
    PkXmlDocument save(PkXmlDocument &doc, bool autosaving, bool documentModified);

    /**
     * Set information about the author.
     * This will override any information retrieved from the author profile
     * But it does not change the author profile
     * Note: authorInfo() will not return the new value until the document has been
     * saved by the user.(autosave doesn't count)
     * @param info the kind of information to set
     * @param data the data to set for this information
     */
    void setAuthorInfo(const PkString& info, const PkString& data);

    /**
     * Obtain information about the author
     * @param info the kind of information to obtain
     * @return a PkString with the information
     */
    PkString authorInfo(const PkString& info) const;

    /**
     * @brief authorContactInfo
     * @return returns list of contact info for author.
     */
    PkStringList authorContactInfo() const;

    /**
     * Set information about the document
     * @param info the kind of information to set
     * @param data the data to set for this information
     */
    void setAboutInfo(const PkString& info, const PkString& data);

    /**
     * Obtain information about the document
     * @param info the kind of information to obtain
     * @return a PkString with the information
     */
    PkString aboutInfo(const PkString& info) const;

    /**
     * Obtain the generator of the document, as it was loaded from the document
     */
    PkString originalGenerator() const;

    /**
     * Sets the original generator of the document. This does not affect what gets
     * saved to a document in the meta:generator field, it only changes what
     * originalGenerator() will return.
     */
    void setOriginalGenerator(const PkString& generator);

    /** Resets part of the meta data */
    void resetMetaData();

    /** Takes care of updating the document info from configuration correctly */
    void updateParameters(bool documentModified);

private:
    /// Bumps the editing cycles count and save date, and then calls updateParameters
    void updateParametersAndBumpNumCycles(bool autosaving, bool documentModified);

    /**
     * Set information about the author
     * This sets what is actually saved to file. The public method setAuthorInfo() can be used to set
     * values that override what is fetched from the author profile. During saveParameters() author
     * profile and any overrides is combined resulting in calls to this method.
     * @param info the kind of information to set
     * @param data the data to set for this information
     */
    void setActiveAuthorInfo(const PkString& info, const PkString& data);

    /**
     * Load the information about the document from a Calligra-1.3 file
     * @param e the element to load from
     * @return true if success
     */
    bool loadAboutInfo(const PkXmlElement& e);

    /**
     * Save the information about the document to a Calligra-1.3 file
     * @param doc the PkXmlDocument to save in
     * @return the PkXmlElement to which was saved
     */
    PkXmlElement saveAboutInfo(PkXmlDocument& doc);

    /**
     * Load the information about the document from a Calligra-1.3 file
     * @param e the element to load from
     * @return true if success
     */
    bool loadAuthorInfo(const PkXmlElement& e);

    /**
     * Save the information about the author to a Calligra-1.3 file
     * @param doc the PkXmlDocument to save in
     * @return the PkXmlElement to which was saved
     */
    PkXmlElement saveAuthorInfo(PkXmlDocument& doc);

    /** A PkStringList containing all tags for the document information */
    PkStringList m_aboutTags;
    /** A PkStringList containing all tags for the author information */
    PkStringList m_authorTags;
    /** A PkStringList containing all valid contact tags */
    PkStringList m_contactTags;
    /** A PkMap with the contact modes and their type in the second string */
    PkMap <PkString, PkString> m_contact;
    /** The map containing information about the author */
    PkMap<PkString, PkString> m_authorInfo;
    /** The map containing information about the author set programmatically*/
    PkMap<PkString, PkString> m_authorInfoOverride;
    /** The map containing information about the document */
    PkMap<PkString, PkString> m_aboutInfo;
    /** The original meta:generator of the document */
    PkString m_generator;

public:
    void infoUpdated(const PkString &info, const PkString &data);
};

#endif
