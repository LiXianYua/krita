#ifndef _KIS_MASK_KEYFRAME_ROUNDTRIP_TEST_H_
#define _KIS_MASK_KEYFRAME_ROUNDTRIP_TEST_H_

#include <simpletest.h>

class KisMaskKeyframeRoundtripTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMaskToXmlRoundTrip();
    void testKeyframeToXmlRoundTrip();
};

#endif
