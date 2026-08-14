/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý lukast.dev @gmail.com
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <simpletest.h>

#include "kis_projection_benchmark.h"
#include "kis_benchmark_values.h"

#include <KoColor.h>

#include <kis_group_layer.h>
#include <kis_paint_device.h>
#include <KisDocument.h>
#include <KisDocumentRegistry.h>
#include <kis_image.h>

void KisProjectionBenchmark::initTestCase()
{

}

void KisProjectionBenchmark::cleanupTestCase()
{
}


void KisProjectionBenchmark::benchmarkProjection()
{
    QBENCHMARK{
        KisDocument *doc = KisDocumentRegistry::instance()->createDocument();
        doc->loadNativeFormat(QString(FILES_DATA_DIR) + '/' + "load_test.kra");
        doc->image()->initialRefreshGraph();
        doc->exportDocumentSync(QString(FILES_OUTPUT_DIR) + '/' + "save_test.kra", doc->mimeType());
        delete doc;
    }
}

void KisProjectionBenchmark::benchmarkLoading()
{
    QBENCHMARK{
        KisDocument *doc2 = KisDocumentRegistry::instance()->createDocument();
        doc2->loadNativeFormat(QString(FILES_DATA_DIR) + '/' + "load_test.kra");
        delete doc2;
    }
}


SIMPLE_TEST_MAIN(KisProjectionBenchmark)
