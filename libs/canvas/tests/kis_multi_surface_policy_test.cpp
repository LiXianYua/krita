/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include "KisCanvasSurfaceColorSpacePolicy.h"
#include "KisMultiSurfaceStateManager.h"

#include <KisProofingConfiguration.h>

class KisMultiSurfacePolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLegacyConfigDefaults();
    void testThreeInitializationModes();
    void testSurfaceProofingScreenAndConfigTransitions();
    void testSurfaceDescriptionSelection();
    void testProtocolNegotiation();
};

void KisMultiSurfacePolicyTest::testLegacyConfigDefaults()
{
    const auto defaults = KisMultiSurfaceStateManager::legacyConfigDefaults();
    QCOMPARE(defaults.surfaceMode, KisCanvasSurfaceMode::Preferred);
    QCOMPARE(defaults.options.first, KoColorConversionTransformation::IntentPerceptual);
    QCOMPARE(defaults.options.second,
             KoColorConversionTransformation::ConversionFlags(
                 KoColorConversionTransformation::HighQuality |
                 KoColorConversionTransformation::BlackpointCompensation));

    QCOMPARE(KisCanvasSurfaceColorSpacePolicy::surfaceModeFromConfig(QStringLiteral("rec709g22")),
             KisCanvasSurfaceMode::Rec709g22);
    QCOMPARE(KisCanvasSurfaceColorSpacePolicy::surfaceModeFromConfig(QStringLiteral("rec709g10")),
             KisCanvasSurfaceMode::Rec709g10);
    QCOMPARE(KisCanvasSurfaceColorSpacePolicy::surfaceModeFromConfig(QStringLiteral("rec2020pq")),
             KisCanvasSurfaceMode::Rec2020pq);
    QCOMPARE(KisCanvasSurfaceColorSpacePolicy::surfaceModeFromConfig(QStringLiteral("unmanaged")),
             KisCanvasSurfaceMode::Unmanaged);
    QCOMPARE(KisCanvasSurfaceColorSpacePolicy::surfaceModeFromConfig(QStringLiteral("invalid")),
             KisCanvasSurfaceMode::Preferred);
}

void KisMultiSurfacePolicyTest::testThreeInitializationModes()
{
    const auto *srgb = reinterpret_cast<const KoColorProfile *>(quintptr(0x10));
    const auto *hdr = reinterpret_cast<const KoColorProfile *>(quintptr(0x20));
    const auto *root = reinterpret_cast<const KoColorProfile *>(quintptr(0x30));
    const auto *display = reinterpret_cast<const KoColorProfile *>(quintptr(0x40));

    KisMultiSurfaceStateManager manager;
    KisMultiSurfaceStateManager::InitializationContext context;
    context.isCanvasOpenGL = true;
    context.srgbProfile = srgb;
    context.legacyHdrProfile = hdr;
    context.rootSurfaceProfile = root;
    context.displayProfile = display;

    context.legacyHdrMode = true;
    auto state = manager.createInitializingConfig(context);
    QCOMPARE(state.multiConfig.canvasProfile, hdr);
    QCOMPARE(state.multiConfig.uiProfile, srgb);
    QVERIFY(!state.multiConfig.isCanvasHDR);

    context.isCanvasOpenGL = false;
    state = manager.createInitializingConfig(context);
    QCOMPARE(state.multiConfig.canvasProfile, srgb);

    context.legacyHdrMode = false;
    context.surfaceManagedByOs = true;
    state = manager.createInitializingConfig(context);
    QCOMPARE(state.multiConfig.canvasProfile, root);
    QCOMPARE(state.multiConfig.uiProfile, root);

    context.surfaceManagedByOs = false;
    state = manager.createInitializingConfig(context);
    QCOMPARE(state.multiConfig.canvasProfile, display);
    QCOMPARE(state.multiConfig.uiProfile, display);

    context.displayProfile = nullptr;
    state = manager.createInitializingConfig(context);
    QCOMPARE(state.multiConfig.canvasProfile, srgb);
    QCOMPARE(state.multiConfig.uiProfile, srgb);
}

void KisMultiSurfacePolicyTest::testSurfaceProofingScreenAndConfigTransitions()
{
    const auto *srgb = reinterpret_cast<const KoColorProfile *>(quintptr(0x10));
    const auto *root = reinterpret_cast<const KoColorProfile *>(quintptr(0x20));
    const auto *screen = reinterpret_cast<const KoColorProfile *>(quintptr(0x30));

    KisMultiSurfaceStateManager manager;
    KisMultiSurfaceStateManager::InitializationContext context;
    context.isCanvasOpenGL = true;
    context.surfaceManagedByOs = true;
    context.srgbProfile = srgb;
    context.rootSurfaceProfile = root;
    auto state = manager.createInitializingConfig(context);

    KisDisplayConfig canvasConfig(root,
                                  KoColorConversionTransformation::IntentPerceptual,
                                  KoColorConversionTransformation::HighQuality |
                                      KoColorConversionTransformation::BlackpointCompensation,
                                  true);
    state = manager.onCanvasSurfaceFormatChanged(state, canvasConfig, true, false);
    QCOMPARE(state.multiConfig.canvasProfile, root);
    QVERIFY(state.multiConfig.isCanvasHDR);

    state = manager.onGuiSurfaceFormatChanged(state, screen, true, false);
    QCOMPARE(state.multiConfig.uiProfile, screen);

    KisProofingConfigurationSP proofing(new KisProofingConfiguration);
    proofing->displayFlags.setFlag(KoColorConversionTransformation::SoftProofing, true);
    proofing->displayFlags.setFlag(KoColorConversionTransformation::GamutCheck, true);
    state = manager.onProofingChanged(state, proofing);
    QCOMPARE(state.multiConfig.intent, KoColorConversionTransformation::IntentAbsoluteColorimetric);
    QVERIFY(state.multiConfig.conversionFlags.testFlag(KoColorConversionTransformation::SoftProofing));
    QVERIFY(state.multiConfig.conversionFlags.testFlag(KoColorConversionTransformation::GamutCheck));

    const auto oldManagedState = state;
    state = manager.onConfigChanged(state,
                                    KisCanvasSurfaceMode::Rec709g22,
                                    KisMultiSurfaceStateManager::legacyConfigDefaults().options,
                                    screen,
                                    srgb,
                                    false,
                                    true);
    QCOMPARE(state.multiConfig.canvasProfile, oldManagedState.multiConfig.canvasProfile);

    state = manager.onConfigChanged(state,
                                    KisCanvasSurfaceMode::Unmanaged,
                                    KisMultiSurfaceStateManager::legacyConfigDefaults().options,
                                    screen,
                                    srgb,
                                    false,
                                    false);
    QCOMPARE(state.multiConfig.canvasProfile, screen);
    QCOMPARE(state.multiConfig.uiProfile, screen);

    const auto managedScreenState = manager.onScreenChanged(state, root, srgb, false, true);
    QCOMPARE(managedScreenState, state);
    const auto unmanagedScreenState = manager.onScreenChanged(state, root, srgb, false, false);
    QCOMPARE(unmanagedScreenState.multiConfig.canvasProfile, root);
    QCOMPARE(unmanagedScreenState.multiConfig.uiProfile, root);
}

void KisMultiSurfacePolicyTest::testSurfaceDescriptionSelection()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription preferred;
    preferred.colorSpace.primaries = NamedPrimaries::Unknown;
    preferred.colorSpace.transferFunction = NamedTransferFunction::Unknown;

    auto selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QVERIFY(selected.requestedDescription);
    QCOMPARE(selected.requestedDescription->colorSpace.primaries, NamedPrimaries::SRgb);
    QCOMPARE(selected.requestedDescription->colorSpace.transferFunction, NamedTransferFunction::Gamma22);

    preferred.colorSpace.luminance = Luminance(0, 1000, 80);
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(selected.requestedDescription->colorSpace.primaries, NamedPrimaries::Bt2020);
    QCOMPARE(selected.requestedDescription->colorSpace.transferFunction, NamedTransferFunction::St2084Pq);
    QCOMPARE(selected.requestedDescription->colorSpace.luminance, Luminance(0, 10000, 80));

    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Rec709g22,
        preferred,
        [](const SurfaceDescription &description) {
            return description.colorSpace.transferFunction == NamedTransferFunction::SRgb;
        },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(selected.requestedDescription->colorSpace.transferFunction, NamedTransferFunction::SRgb);

    int profileAttempts = 0;
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Rec2020pq,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [&profileAttempts](const SurfaceDescription &description) {
            ++profileAttempts;
            return description.colorSpace.primaries == NamedPrimaries::SRgb &&
                description.colorSpace.transferFunction == NamedTransferFunction::SRgb;
        });
    QCOMPARE(profileAttempts, 4);
    QVERIFY(selected.hasProfile);
}

void KisMultiSurfacePolicyTest::testProtocolNegotiation()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription preferred;
    preferred.colorSpace.primaries = NamedPrimaries::SRgb;
    preferred.colorSpace.transferFunction = NamedTransferFunction::Gamma22;

    NegotiationInput input;
    input.ready = true;
    input.surfaceMode = KisCanvasSurfaceMode::Preferred;
    input.options = {KoColorConversionTransformation::IntentRelativeColorimetric,
                     KoColorConversionTransformation::HighQuality |
                         KoColorConversionTransformation::BlackpointCompensation};
    input.compositorPreferred = preferred;

    auto result = negotiate(
        input,
        [](RenderIntent intent) { return intent == RenderIntent::Perceptual; },
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(result.intent, RenderIntent::Perceptual);
    QCOMPARE(result.command, ProtocolCommand::Set);
    QVERIFY(result.requestedDescription);

    input.currentDescription = result.requestedDescription;
    input.currentIntent = result.intent;
    result = negotiate(input,
                       [](RenderIntent intent) { return intent == RenderIntent::Perceptual; },
                       [](const SurfaceDescription &) { return true; },
                       [](const SurfaceDescription &) { return true; });
    QCOMPARE(result.command, ProtocolCommand::None);

    input.surfaceMode = KisCanvasSurfaceMode::Unmanaged;
    result = negotiate(input,
                       [](RenderIntent) { return true; },
                       [](const SurfaceDescription &) { return true; },
                       [](const SurfaceDescription &) { return true; });
    QCOMPARE(result.command, ProtocolCommand::Unset);
    QVERIFY(!result.requestedDescription);

    input.ready = false;
    result = negotiate(input,
                       [](RenderIntent) { return true; },
                       [](const SurfaceDescription &) { return true; },
                       [](const SurfaceDescription &) { return true; });
    QVERIFY(result.deferred);
    QCOMPARE(result.command, ProtocolCommand::None);
}

SIMPLE_TEST_MAIN(KisMultiSurfacePolicyTest)

#include "kis_multi_surface_policy_test.moc"
