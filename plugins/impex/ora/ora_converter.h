/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _ORA_CONVERTER_H_
#define _ORA_CONVERTER_H_

#include <KisImportExportErrorCode.h>
#include <PkVector.h>
#include <kis_types.h>

class KisDocument;
class PkStream;

class OraConverter
{
public:
    OraConverter(KisDocument *doc);
    ~OraConverter();

    KisImportExportErrorCode buildImage(PkStream *io);
    KisImportExportErrorCode buildFile(PkStream *io, KisImageSP image, vKisNodeSP activeNodes);
    /**
     * Retrieve the constructed image
     */
    KisImageSP image();
    vKisNodeSP activeNodes();
private:
    KisImageSP m_image;
    KisDocument *m_doc;
    vKisNodeSP m_activeNodes;
};

#endif
