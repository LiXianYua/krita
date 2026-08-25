/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
 * @file kis_annotation.h
 * @brief This file is part of the Krita application in calligra
 * @author Boudewijn Rempt
 * @author comments by hscott
 * @since 1.4 or 2005
 */

#ifndef _KIS_ANNOTATION_H_
#define _KIS_ANNOTATION_H_

#include <kis_shared.h>

#include <PkAuxTypes.h>
#include <PkString.h>

#include "kritaimage_export.h"

/**
 * @class KisAnnotation
 * @brief A data extension mechanism for Krita.
 *
 * An annotation can be of something like a PkByteArray or a PkString or
 * a more specific datatype that can be attached to an image (or maybe
 * later, if needed, to a layer) and contains data that must be
 * associated with an image for purposes of import/export.
 *
 * Annotations will be saved to krita images and may be exported in
 * filetypes that support them.
 *
 * Examples of annotations are EXIF data and ICC profiles.
 */
class KRITAIMAGE_EXPORT KisAnnotation : public KisShared
{

public:

    /**
     * creates a new annotation object. The annotation object cannot
     * be changed later.
     *
     * @param type a non-localized string identifying the type of the
     * annotation. There can only be one annotation of a given type attached
     * to an image.
     * @param description a localized string describing the annotation
     * @param data a binary blob containing the annotation data
     */
    KisAnnotation(const PkString & type, const PkString & description, const PkByteArray & data)
        : m_type(type)
        , m_description(description)
        , m_annotation(data) {}

    virtual ~KisAnnotation() {}

    virtual KisAnnotation* clone() const {
        return new KisAnnotation(*this);
    }

    /**
     * gets a non-localized string identifying the type of the
     * annotation.
     * @return a non-localized string identifying the type of the
     * annotation
     */
    const PkString & type() const {
        return m_type;
    }

    /**
     * gets a localized string describing the type of annotations for
     * used interface purposes.
     * @return a localized string describing the type of the
     * annotations for user interface purposes.
     */
    const PkString & description() const {
        return m_description;
    }

    /**
     * gets a binary blob representation of this annotation
     * @return a binary blob representation of this annotation
     */
    const PkByteArray & annotation() const {
        return m_annotation;
    }

    void setAnnotation(const PkByteArray ba) {
        m_annotation = ba;
    }

    /**
     * @brief displayText: override this to return an interpreted version of the annotation
     */
    virtual PkString displayText() const {
        return PkString::PkFromUtf8(m_annotation.data(), m_annotation.size());
    }

protected:
    KisAnnotation(const KisAnnotation &rhs)
     : KisShared(),
       m_type(rhs.m_type),
       m_description(rhs.m_description),
       m_annotation(rhs.m_annotation)
    {
    }

protected:

    PkString m_type;
    PkString m_description;
    PkByteArray m_annotation;

};

#endif // _KIS_ANNOTATION_H_
