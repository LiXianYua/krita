/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CSV_SAVER_H_
#define CSV_SAVER_H_

#include <atomic>

#include <KisImportExportErrorCode.h>
#include <kis_types.h>

class CSVLayerRecord;
class KisDocument;
class PkStream;

class CSVSaver {
public:
    CSVSaver(KisDocument* doc, bool batchMode);
    ~CSVSaver();

    KisImportExportErrorCode buildAnimation(PkStream *io);
    KisImageSP image();
    void cancel();

private:
    KisImportExportErrorCode encode(PkStream *io);
    KisImportExportErrorCode getLayer(CSVLayerRecord* , KisDocument* , KisKeyframeSP, const PkString &, int, int);
    void createTempImage(KisDocument* );
    PkString convertToBlending(const PkString &);

private:
    KisImageSP m_image;
    KisDocument* m_doc;
    std::atomic_bool m_stop;
};

#endif
