/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_LAYER_STYLE_SERIALIZER_H
#define __KIS_ASL_LAYER_STYLE_SERIALIZER_H

#include "kritaimage_export.h"

class PkStream;
class PkXmlDocument;
class KoPattern;
class KisResourceModel;

#include "kis_psd_layer_style.h"
#include "asl/kis_asl_callback_object_catcher.h"
#include "KisLocalStrokeResources.h"

#include <PkHash.h>        // PkHash<PkString, ...> 成员（PkVector/PkString 经 psd.h 传递可用）
#include <PkStringHash.h>  // qHash(const PkString&)：PkHash<PkString, V> 实例化需要

class KRITAIMAGE_EXPORT KisAslLayerStyleSerializer
{
public:
    KisAslLayerStyleSerializer();
    ~KisAslLayerStyleSerializer();

    void saveToDevice(PkStream &device);
    bool saveToFile(const PkString& filename);
    void readFromDevice(PkStream &device);
    bool readFromFile(const PkString& filename);

    void assignAllLayerStylesToLayers(KisNodeSP root, const PkString &storageLocation);
    static PkVector<KisPSDLayerStyleSP> collectAllLayerStyles(KisNodeSP root);

    PkVector<KisPSDLayerStyleSP> styles() const;
    void setStyles(const PkVector<KisPSDLayerStyleSP> &styles);

    PkHash<PkString, KoPatternSP> patterns() const;
    PkVector<KoAbstractGradientSP> gradients() const;
    PkHash<PkString, KisPSDLayerStyleSP> stylesHash();


    void registerPSDPattern(const PkXmlDocument &doc);
    void readFromPSDXML(const PkXmlDocument &doc);

    PkXmlDocument formXmlDocument() const;
    PkXmlDocument formPsdXmlDocument() const;

    bool isInitialized() {
        return m_initialized;
    }

    bool isValid() {
        return isInitialized() && m_isValid;
    }

    static void sideLoadLinkedResources(KisPSDLayerStyle *style, KisResourcesInterfaceSP resourcesInterface);
    static PkVector<KoResourceSignature> fetchLinkedResourceSignatures(const KisPSDLayerStyle *style);

private:
    void registerPatternObject(const KoPatternSP pattern, const  PkString& patternUuid);

    void assignPatternObject(const PkString &patternUuid, const PkString &patternName, std::function<void(KoPatternSP)> setPattern);
    void assignGradientObject(KoAbstractGradientSP gradient, std::function<void(KoAbstractGradientSP)> setGradient);

    static PkVector<KoResourceSignature> fetchAllPatternLinks(const KisPSDLayerStyle *style);
    static PkVector<KoPatternSP> fetchAllPatterns(const KisPSDLayerStyle *style, KisResourcesInterfaceSP resourcesInterface);

    void newStyleStarted(bool isPsdStructure);
    void connectCatcherToStyle(KisPSDLayerStyle *style, const PkString &prefix);

private:
    PkHash<PkString, KoPatternSP> m_patternsStore;

    KisAslCallbackObjectCatcher m_catcher;
    PkVector<KisPSDLayerStyleSP> m_stylesVector;
    PkVector<KoAbstractGradientSP> m_gradientsStore;
    PkHash<PkString, KisPSDLayerStyleSP> m_stylesHash;
    bool m_initialized {false};
    bool m_isValid {true};
    PkSharedPointer<KisLocalStrokeResources> m_localResourcesInterface;
};

#endif /* __KIS_ASL_LAYER_STYLE_SERIALIZER_H */
