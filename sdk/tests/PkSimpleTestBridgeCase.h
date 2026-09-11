#pragma once

#include <PkTestObject.h>

class PkSimpleTestBridgeCase : public PkTestObject
{
public:
    void testMainThreadQueueIsReady();
    void testUnpumpedCallStaysPending();
};
