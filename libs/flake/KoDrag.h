/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KODRAG_H
#define KODRAG_H

#include "kritaflake_export.h"

#include <PkClipboardData.h>
#include <PkList.h>

class KoDragPrivate;
class KoShape;

/**
 * Class for simplifying adding a odf to the clip board
 *
 * For saving the odf a KoDragOdfSaveHelper class is used.
 * It implements the writing of the body of the document. The
 * setOdf takes care of saving styles and all the other
 * common stuff.
 *
 * KoDrag only produces the payload. Installing it into the platform clipboard
 * is the host's job.
 */
class KRITAFLAKE_EXPORT KoDrag
{
public:
    KoDrag();
    ~KoDrag();

    /**
     * Load SVG data into the current clipboard payload
     */
    bool setSvg(const PkList<KoShape*> shapes);

    /**
     * Add additional mimeTypes
     */
    void setData(const PkString &mimeType, const PkByteArray &data);

    /**
     * Get the clipboard payload
     *
     * This transfers the ownership of the payload to the caller: the KoDrag no
     * longer holds it, so a later call without an intervening setSvg()/setData()
     * returns an empty payload.
     */
    PkClipboardData takeClipboardData();

private:
    KoDragPrivate * const d;
};

#endif /* KODRAG_H */
