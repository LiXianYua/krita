/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// The helper remains a UI-test utility, but its value/storage boundary follows
// the migrated production APIs. Qt is confined to the test harness diagnostics.


#ifndef __STROKE_TESTING_UTILS_H
#define __STROKE_TESTING_UTILS_H

#include <PkImage.h>
#include <PkSize.h>
#include <PkString.h>
#include <KoCanvasResourceProvider.h>
#include "kis_node.h"
#include "kis_types.h"
#include "kis_stroke_strategy.h"
#include "kis_resources_snapshot.h"


class KisUndoStore;


namespace utils {

    KisImageSP createImage(KisUndoStore *undoStore, const PkSize &imageSize);
    KoCanvasResourceProvider* createResourceManager(KisImageWSP image,
                                             KisNodeSP node = 0,
                                             const PkString &presetFileName = PkString("autobrush_300px.kpp"));

    class StrokeTester
    {
    public:
        StrokeTester(const PkString &name, const PkSize &imageSize,
                     const PkString &presetFileName = PkString("autobrush_300px.kpp"));
        virtual ~StrokeTester();

        void testSimpleStroke();
        void testSimpleStrokeCancelled();
        void test();
        void benchmark();
        void testSimpleStrokeNoVerification();

        void setNumIterations(int value);
        void setBaseFuzziness(int value);
        void setCancelOnIteration(int value);

        int lastStrokeTime() const;

    protected:
        KisStrokeId strokeId() {
            return m_strokeId;
        }

        virtual void modifyResourceManager(KoCanvasResourceProvider *manager,
                                           KisImageWSP image, int iteration);

        virtual void initImage(KisImageWSP image, KisNodeSP activeNode, int iteration);

        // overload
        virtual void modifyResourceManager(KoCanvasResourceProvider *manager,
                                           KisImageWSP image);

        // overload
        virtual void initImage(KisImageWSP image, KisNodeSP activeNode);
        virtual void beforeCheckingResult(KisImageWSP image, KisNodeSP activeNode);
        virtual void iterationEndedCallback(KisImageWSP image, KisNodeSP activeNode, int iteration);

        virtual KisStrokeStrategy* createStroke(KisResourcesSnapshotSP resources,
                                                KisImageWSP image) = 0;

        virtual void addPaintingJobs(KisImageWSP image,
                                     KisResourcesSnapshotSP resources,
                                     int iteration);

        // overload
        virtual void addPaintingJobs(KisImageWSP image,
                                     KisResourcesSnapshotSP resources);

    private:
        void testOneStroke(bool cancelled, bool indirectPainting,
                           bool externalLayer, bool testUpdates = false);

        PkImage doStroke(bool cancelled,
                        bool externalLayer, bool testUpdates = false,
                        bool needQImage = true);

        PkString formatTestName(const PkString &baseName, bool cancelled,
                                bool indirectPainting, bool externalLayer);
        PkString referenceFile(const PkString &testName);
        PkString dumpReferenceFile(const PkString &testName);
        PkString resultFile(const PkString &testName);

    private:
        KisStrokeId m_strokeId;
        PkString m_name;
        PkSize m_imageSize;
        PkString m_presetFilename;
        int m_numIterations;
        int m_baseFuzziness;
        int m_strokeTime = 0;
        int m_cancelOnIteration = 0;
    };
}

#endif /* __STROKE_TESTING_UTILS_H */
