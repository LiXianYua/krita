/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_KRA_LOAD_VISITOR_H_
#define KIS_KRA_LOAD_VISITOR_H_

#include <PkStringList.h>

// kritaimage
#include "kis_types.h"
#include "kis_node_visitor.h"

#include "kritalibkra_export.h"

class KisFilterConfiguration;
class KoStore;
class KoShapeControllerBase;
class KoColorProfile;
class KisImportUserFeedbackInterface;
class KisNodeFilterInterface;

class KRITALIBKRA_EXPORT KisKraLoadVisitor : public KisNodeVisitor
{
public:


    KisKraLoadVisitor(KisImageSP image,
                      KoStore *store,
                      KoShapeControllerBase *shapeController,
                      PkMap<KisNode *, PkString> &layerFilenames,
                      PkMap<KisNode *, PkString> &keyframeFilenames,
                      const PkString & name,
                      int syntaxVersion,
                      KisImportUserFeedbackInterface *feedbackInterface = nullptr);

public:
    void setExternalUri(const PkString &uri);

    bool visit(KisNode*) override {
        return true;
    }
    bool visit(KisExternalLayer *) override;
    bool visit(KisPaintLayer *layer) override;
    bool visit(KisGroupLayer *layer) override;
    bool visit(KisAdjustmentLayer* layer) override;
    bool visit(KisGeneratorLayer* layer) override;
    bool visit(KisCloneLayer *layer) override;
    bool visit(KisFilterMask *mask) override;
    bool visit(KisTransformMask *mask) override;
    bool visit(KisTransparencyMask *mask) override;
    bool visit(KisSelectionMask *mask) override;
    bool visit(KisColorizeMask *mask) override;

    PkStringList errorMessages() const;
    PkStringList warningMessages() const;

private:

    bool loadPaintDevice(KisPaintDeviceSP device, const PkString& location);

    template<class DevicePolicy>
    bool loadPaintDeviceFrame(KisPaintDeviceSP device, const PkString &location, DevicePolicy policy);

    bool loadProfile(KisPaintDeviceSP device,  const PkString& location);
    bool loadFilterConfiguration(KisFilterConfigurationSP kfc, const PkString& location);
    const KoColorProfile* loadProfile(const PkString& location, const PkString &colorModelId, const PkString &colorDepthId);
    void fixOldFilterConfigurations(KisFilterConfigurationSP kfc);
    bool loadMetaData(KisNode* node);
    void initSelectionForMask(KisMask *mask);
    bool loadSelection(const PkString& location, KisSelectionSP dstSelection);
    PkString getLocation(KisNode* node, const PkString& suffix = PkString());
    PkString getLocation(const PkString &filename, const PkString &suffix = PkString());
    void loadNodeKeyframes(KisNode *node);

    /**
     * Load deprecated filters.
     * Most deprecated filters can be handled by this, but the brightnesscontact to perchannels
     * conversion needs to be handled in the perchannel class because those filters
     * have their own xml loading functionality.
     */
    void loadDeprecatedFilter(KisFilterConfigurationSP cfg);

private:
    KisImageSP m_image;
    KoStore *m_store;
    bool m_external;
    PkString m_uri;
    PkMap<KisNode *, PkString> m_layerFilenames;
    PkMap<KisNode *, PkString> m_keyframeFilenames;
    PkString m_name;
    int m_syntaxVersion;
    PkStringList m_errorMessages;
    PkStringList m_warningMessages;
    KoShapeControllerBase *m_shapeController;
    PkMap<PkString, const KoColorProfile *> m_profileCache;
    KisImportUserFeedbackInterface *m_feedbackInterface {nullptr};
};

#endif // KIS_KRA_LOAD_VISITOR_H_

