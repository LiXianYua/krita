#ifndef KISSAFEDOCUMENTLOADERTEST_H
#define KISSAFEDOCUMENTLOADERTEST_H

#include <QObject>

class KisSafeDocumentLoaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void test();
    void testFileLost();
    void testQueuedDeliveryHonorsReceiverLifetime();
    void testTemporaryCopiesArePrivateAndArchiveUsesMergedImage();
    void testDestroyWithDebouncePending();
    void testDestroyWithDelayedLoadPending();
    void testDestroyWithWatcherEventQueued();
    void testSharedPathSurvivesOneLoaderDestruction();
    void testDebounceAndRetryCounts();
};

#endif // KISSAFEDOCUMENTLOADERTEST_H
