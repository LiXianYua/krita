#include "kis_mask_keyframe_roundtrip_test.h"

#include <simpletest.h>

#include "kis_mask_generator.h"
#include "kis_base_mask_generator.h"
#include "kis_keyframe_channel.h"
#include "kis_scalar_keyframe_channel.h"
#include "kis_default_bounds.h"

#include <PkXmlDocument.h>
#include <PkXmlElement.h>

void KisMaskKeyframeRoundtripTest::testMaskToXmlRoundTrip()
{
    // S-06b 回归：PkString().arg() 空接收者无 %1 占位符返回空串，
    // toXML 写空属性 → fromXML 丢 mask 形状（diameter/ratio/hfade/vfade/spikes/antialiasEdges）。
    KisCircleMaskGenerator mask(10.0, 0.5, 0.25, 0.75, 4, true);

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("mask");
    doc.appendChild(root);
    mask.toXML(doc, root);

    // 修复后每个属性必须是十进制串（不能为空）。
    PK_VERIFY(!root.attribute("diameter").isEmpty());
    PK_VERIFY(!root.attribute("ratio").isEmpty());
    PK_VERIFY(!root.attribute("hfade").isEmpty());
    PK_VERIFY(!root.attribute("vfade").isEmpty());
    PK_VERIFY(!root.attribute("spikes").isEmpty());
    PK_VERIFY(!root.attribute("antialiasEdges").isEmpty());

    // 往返：fromXML 恢复等价 generator（valueAt 逐像素相等）。
    std::unique_ptr<KisMaskGenerator> mask2(KisMaskGenerator::fromXML(root));
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            PK_COMPARE(mask.valueAt(i, j), mask2->valueAt(i, j));
        }
    }
}

void KisMaskKeyframeRoundtripTest::testKeyframeToXmlRoundTrip()
{
    // S-06b 回归：KisKeyframeChannel::toXML 的 time/color-label 用空接收者
    // arg() 写空属性 → loadXML 恢复的时间错位/丢属性，保存→加载损坏。
    KisDefaultBoundsSP bounds = new KisDefaultBounds();
    KisScalarKeyframeChannel channel(KoID(), bounds);
    channel.addKeyframe(7);
    channel.addKeyframe(9);
    channel.keyframeAt(7)->setColorLabel(3);

    PkXmlDocument doc;
    PkXmlElement channelElement = channel.toXML(doc, "layer");

    int keyframeCount = 0;
    for (PkXmlElement kf = channelElement.firstChildElement();
         !kf.isNull(); kf = kf.nextSiblingElement()) {
        PK_VERIFY(!kf.attribute("time").isEmpty());
        ++keyframeCount;
    }
    PK_COMPARE(keyframeCount, 2);

    // 往返：loadXML 到新 channel 恢复两个关键帧及 color-label。
    // KisSharedPtr 无 operator bool，判空用 `!= nullptr`（走 operator!=(const T* p)）。
    KisScalarKeyframeChannel channel2(KoID(), bounds);
    channel2.loadXML(channelElement);
    PK_VERIFY(channel2.keyframeAt(7) != nullptr);
    PK_VERIFY(channel2.keyframeAt(9) != nullptr);
    PK_COMPARE(channel2.keyframeAt(7)->colorLabel(), 3);
}

SIMPLE_TEST_MAIN(KisMaskKeyframeRoundtripTest)
