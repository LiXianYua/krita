/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CSV_LOADER_H_
#define CSV_LOADER_H_

#include <atomic>

#include "kis_image.h"
#include <KisImportExportErrorCode.h>
class KisDocument;

#include "csv_layer_record.h"

class CSVLoader {
public:
    CSVLoader(KisDocument* doc, bool batchMode);
    ~CSVLoader();

    KisImportExportErrorCode buildAnimation(PkStream *io, const PkString &filename);

    KisImageSP image();
    void cancel();

private:
    KisImportExportErrorCode decode(PkStream *io, const PkString &filename);
    KisImportExportErrorCode setLayer(CSVLayerRecord* , KisDocument* ,const PkString &);
    KisImportExportErrorCode createNewImage(int, int, float, const PkString &);
    PkString convertBlending(const PkString &);
    PkString validPath(const PkString &, const PkString &);

private:
    KisImageSP m_image;
    KisDocument* m_doc;
    std::atomic_bool m_stop;
};

#endif
