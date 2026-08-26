/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_LEVELS_FILTER_CONFIGURATION_H
#define KIS_LEVELS_FILTER_CONFIGURATION_H

#include <optional>

#include <filter/kis_color_transformation_configuration.h>
#include <kis_paint_device.h>
#include <KisLevelsCurve.h>
#include <KisAutoLevels.h>

#include "../colorsfilters/virtual_channel_info.h"

class KisLevelsFilterConfiguration : public KisColorTransformationConfiguration
{
public:
    KisLevelsFilterConfiguration(int channelCount, qint32 version, KisResourcesInterfaceSP resourcesInterface);
    KisLevelsFilterConfiguration(int channelCount, KisResourcesInterfaceSP resourcesInterface);
    KisLevelsFilterConfiguration(const KisLevelsFilterConfiguration &rhs);

    KisFilterConfigurationSP clone() const override;

    static inline PkString defaultName() { return "levels"; }
    static constexpr qint32 defaultVersion() { return 2; }
    static inline KisLevelsCurve defaultLevelsCurve() { return KisLevelsCurve(); }
    static constexpr bool defaultUseLightnessMode() { return true; }
    static constexpr bool defaultShowLogarithmicHistogram() { return false; }
    static constexpr qreal defaultAutoLevelsShadowsClipping() { return 0.1; }
    static constexpr qreal defaultAutoLevelsHighlightsClipping() { return 0.1; }
    static constexpr qreal defaultAutoLevelsMaximumInputBlackAndWhiteOffset() { return 100.0; }
    static constexpr qreal defaultAutoLevelsMidtonesAdjustmentAmount() { return 50.0; }

    using KisFilterConfiguration::fromXML;
    using KisFilterConfiguration::toXML;
    using KisFilterConfiguration::fromLegacyXML;
    
    void fromLegacyXML(const PkXmlElement& root) override;
    void fromXML(const PkXmlElement& e) override;
    void toXML(PkXmlDocument& doc, PkXmlElement& root) const override;

    void setProperty(const PkString &name, const PkVariant &value) override;

    const PkVector<KisLevelsCurve> levelsCurves() const;
    const KisLevelsCurve lightnessLevelsCurve() const;
    void setLevelsCurves(const PkVector<KisLevelsCurve> &newLevelsCurves);
    void setLightnessLevelsCurve(const KisLevelsCurve &newLightnessLevelsCurve);
    const PkVector<PkVector<quint16>>& transfers() const;
    const PkVector<quint16>& lightnessTransfer() const;

    bool useLightnessMode() const;
    bool showLogarithmicHistogram() const;
    void setUseLightnessMode(bool newUseLightnessMode);
    void setShowLogarithmicHistogram(bool newShowLogarithmicHistogram);

    bool isCompatible(const KisPaintDeviceSP) const override;

    void setDefaults();

    struct AutoLevelsDefaults
    {
        std::optional<qreal> maximumInputBlackAndWhiteOffset;
        std::optional<KisAutoLevels::MidtonesAdjustmentMethod> midtonesAdjustmentMethod;
        std::optional<qreal> midtonesAdjustmentAmount;
    };
    /// 自动色阶的经验参数（摘自 D-02-b 删除的 KisLevelsConfigWidget）
    /// 上游原注释：These were selected empirically, there is no strong reason why they should be like this
    /// 字段为 std::optional：某个字段是 nullopt 表示原代码在对应分支下根本不调用
    /// 对应的 setter，调用方应保留自己的既有值，不是被这个函数强行覆盖成某个值。
    static AutoLevelsDefaults autoLevelsDefaults(const KoColorSpace *cs, bool lightnessMode, const VirtualChannelInfo &channel);

private:
    PkVector<PkVector<quint16>> m_transfers;
    PkVector<quint16> m_lightnessTransfer;

    int channelCount() const;
    void setChannelCount(int newChannelCount);

    void setLightessLevelsCurveFromLegacyValues();
    void setLegacyValuesFromLightnessLevelsCurve();

    void updateTransfers();
    void updateLightnessTransfer();
};

#endif
