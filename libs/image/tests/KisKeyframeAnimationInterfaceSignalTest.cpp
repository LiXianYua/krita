/*
 *  SPDX-FileCopyrightText: 2020 Saurabh Kumar <saurabhk660@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisKeyframeAnimationInterfaceSignalTest.h"

#include <simpletest.h>


void KisKeyframeAnimationInterfaceSignalTest::initTestCase()
{
    m_image1 = new KisImage(0, 100, 100, nullptr, "image1");
    m_image2 = new KisImage(0, 100, 100, nullptr, "image2");
    m_layer = new KisPaintLayer(m_image1, "layer1", OPACITY_OPAQUE_U8);
    m_image1->addNode(m_layer);
    m_channel = m_layer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
}

void KisKeyframeAnimationInterfaceSignalTest::init()
{
    m_channel->addKeyframe(0);

    //delete all  keyframes other than 1st
    while (m_channel->keyframeAt(m_channel->firstKeyframeTime()) != m_channel->keyframeAt(m_channel->lastKeyframeTime())) {
        m_channel->removeKeyframe(m_channel->lastKeyframeTime());
    }
    QCOMPARE(m_channel->keyframeCount(), 1);
}

void KisKeyframeAnimationInterfaceSignalTest::testSignalFromKeyframeChannelToInterface()
{

    QCOMPARE(m_channel->keyframeCount(), 1);

    //add keyframe
    PkObject connectionReceiver;
    int frameAddedCount = 0;
    int frameRemovedCount = 0;
    QVERIFY(PkObject::connect(m_image1->animationInterface(), &KisImageAnimationInterface::sigKeyframeAdded,
                              &connectionReceiver,
                              [&frameAddedCount](const KisKeyframeChannel *, int) { ++frameAddedCount; }).isValid());
    QVERIFY(PkObject::connect(m_image1->animationInterface(), &KisImageAnimationInterface::sigKeyframeRemoved,
                              &connectionReceiver,
                              [&frameRemovedCount](const KisKeyframeChannel *, int) { ++frameRemovedCount; }).isValid());

    m_channel->addKeyframe(2);
    QCOMPARE(frameAddedCount, 1);

    //remove keyframe
    m_channel->removeKeyframe(5);
    QCOMPARE(frameRemovedCount, 1);
}

void KisKeyframeAnimationInterfaceSignalTest::testSignalOnImageReset()
{
    m_image1->removeNode(m_layer);
    m_image2->addNode(m_layer);

    QCOMPARE(m_layer->image(), KisImageWSP(m_image2));

    //test the connections between m_channel and new image's animation interface
    QVERIFY(!PkObject::connect(m_channel, &KisKeyframeChannel::sigAddedKeyframe,
                              m_image2->animationInterface(), &KisImageAnimationInterface::sigKeyframeAdded,
                              PkConnectionType::Unique).isValid());

    //test signals from the old and new images after image reset
    PkObject connectionReceiver;
    int oldFrameAddedCount = 0;
    int oldFrameRemovedCount = 0;
    int newFrameAddedCount = 0;
    int newFrameRemovedCount = 0;
    QVERIFY(PkObject::connect(m_image1->animationInterface(), &KisImageAnimationInterface::sigKeyframeAdded,
                              &connectionReceiver,
                              [&oldFrameAddedCount](const KisKeyframeChannel *, int) { ++oldFrameAddedCount; }).isValid());
    QVERIFY(PkObject::connect(m_image1->animationInterface(), &KisImageAnimationInterface::sigKeyframeRemoved,
                              &connectionReceiver,
                              [&oldFrameRemovedCount](const KisKeyframeChannel *, int) { ++oldFrameRemovedCount; }).isValid());
    QVERIFY(PkObject::connect(m_image2->animationInterface(), &KisImageAnimationInterface::sigKeyframeAdded,
                              &connectionReceiver,
                              [&newFrameAddedCount](const KisKeyframeChannel *, int) { ++newFrameAddedCount; }).isValid());
    QVERIFY(PkObject::connect(m_image2->animationInterface(), &KisImageAnimationInterface::sigKeyframeRemoved,
                              &connectionReceiver,
                              [&newFrameRemovedCount](const KisKeyframeChannel *, int) { ++newFrameRemovedCount; }).isValid());

    m_channel->addKeyframe(2);
    m_channel->removeKeyframe(2);

    QCOMPARE(oldFrameAddedCount, 0);
    QCOMPARE(oldFrameRemovedCount, 0);

    QCOMPARE(newFrameAddedCount, 1);
    QCOMPARE(newFrameRemovedCount, 1);

    QCOMPARE(m_channel->keyframeCount(), 1);
}

SIMPLE_TEST_MAIN(KisKeyframeAnimationInterfaceSignalTest)
