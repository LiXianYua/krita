/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include "KisCanvasSurfaceColorSpacePolicy.h"
#include "KisMultiSurfaceStateManager.h"

#include <KisProofingConfiguration.h>
#include <surfacecolormanagement/KisSurfaceColorimetry.h>

#include <type_traits>

static_assert(std::is_same_v<KisCanvasSurfaceColorSpacePolicy::SurfaceDescription,
                             KisSurfaceColorimetry::SurfaceDescription>);

class KisMultiSurfacePolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLegacyConfigDefaults();
    void testThreeInitializationModes();
    void testSurfaceProofingScreenAndConfigTransitions();
    void testSurfaceDescriptionSelection();
    void testCustomColorimetryAndTransfer();
    void testMasteringMetadataComparison();
    void testTerminalSrgbFallbacks();
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
    preferred.colorSpace.primaries = NamedPrimaries::primaries_unknown;
    preferred.colorSpace.transferFunction = NamedTransferFunction::transfer_function_unknown;

    auto selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QVERIFY(selected.requestedDescription);
    QVERIFY(std::holds_alternative<NamedPrimaries>(selected.requestedDescription->colorSpace.primaries));
    QCOMPARE(std::get<NamedPrimaries>(selected.requestedDescription->colorSpace.primaries),
             NamedPrimaries::primaries_srgb);
    QVERIFY(std::holds_alternative<NamedTransferFunction>(selected.requestedDescription->colorSpace.transferFunction));
    QCOMPARE(std::get<NamedTransferFunction>(selected.requestedDescription->colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_gamma22);

    preferred.colorSpace.luminance = Luminance(0, 1000, 80);
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(std::get<NamedPrimaries>(selected.requestedDescription->colorSpace.primaries),
             NamedPrimaries::primaries_bt2020);
    QCOMPARE(std::get<NamedTransferFunction>(selected.requestedDescription->colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_st2084_pq);
    QCOMPARE(selected.requestedDescription->colorSpace.luminance, Luminance(0, 10000, 80));

    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Rec709g22,
        preferred,
        [](const SurfaceDescription &description) {
            return std::holds_alternative<NamedTransferFunction>(description.colorSpace.transferFunction) &&
                std::get<NamedTransferFunction>(description.colorSpace.transferFunction) ==
                    NamedTransferFunction::transfer_function_srgb;
        },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(std::get<NamedTransferFunction>(selected.requestedDescription->colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_srgb);

    int profileAttempts = 0;
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Rec2020pq,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [&profileAttempts](const SurfaceDescription &description) {
            ++profileAttempts;
            return std::holds_alternative<NamedPrimaries>(description.colorSpace.primaries) &&
                std::get<NamedPrimaries>(description.colorSpace.primaries) == NamedPrimaries::primaries_srgb &&
                std::holds_alternative<NamedTransferFunction>(description.colorSpace.transferFunction) &&
                std::get<NamedTransferFunction>(description.colorSpace.transferFunction) ==
                    NamedTransferFunction::transfer_function_srgb;
        });
    QCOMPARE(profileAttempts, 4);
    QCOMPARE(selected.profileSource, ProfileSource::Generated);
}

void KisMultiSurfacePolicyTest::testCustomColorimetryAndTransfer()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription preferred;
    preferred.colorSpace.primaries = KisColorimetryUtils::Colorimetry::DisplayP3;
    preferred.colorSpace.transferFunction = uint32_t(23456);
    preferred.colorSpace.luminance = Luminance(100, 80, 80);
    preferred.masteringInfo = MasteringInfo();

    const auto selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });

    QVERIFY(selected.requestedDescription);
    QVERIFY(std::holds_alternative<KisColorimetryUtils::Colorimetry>(
        selected.requestedDescription->colorSpace.primaries));
    QCOMPARE(std::get<KisColorimetryUtils::Colorimetry>(
                 selected.requestedDescription->colorSpace.primaries),
             KisColorimetryUtils::Colorimetry::DisplayP3);
    QVERIFY(std::holds_alternative<uint32_t>(
        selected.requestedDescription->colorSpace.transferFunction));
    QCOMPARE(std::get<uint32_t>(selected.requestedDescription->colorSpace.transferFunction),
             uint32_t(23456));
    QCOMPARE(selected.requestedDescription->colorSpace.luminance, preferred.colorSpace.luminance);
    QVERIFY(!selected.requestedDescription->masteringInfo);
}

void KisMultiSurfacePolicyTest::testMasteringMetadataComparison()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription requested;
    requested.colorSpace.primaries = NamedPrimaries::primaries_srgb;
    requested.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;

    SurfaceDescription current = requested;
    MasteringInfo mastering;
    mastering.primaries = KisColorimetryUtils::Colorimetry::BT2020;
    mastering.luminance = MasteringLuminance(100, 1000);
    mastering.maxCll = 1200;
    mastering.maxFall = 400;
    current.masteringInfo = mastering;
    QVERIFY(current != requested);

    NegotiationInput input;
    input.ready = true;
    input.surfaceMode = KisCanvasSurfaceMode::Preferred;
    input.compositorPreferred = requested;
    input.currentDescription = current;
    input.currentIntent = RenderIntent::render_intent_perceptual;

    const auto result = negotiate(input,
                                  [](RenderIntent) { return true; },
                                  [](const SurfaceDescription &) { return true; },
                                  [](const SurfaceDescription &) { return true; });
    QCOMPARE(result.command, ProtocolCommand::Set);
}

void KisMultiSurfacePolicyTest::testTerminalSrgbFallbacks()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription preferred;
    preferred.colorSpace.primaries = KisColorimetryUtils::Colorimetry::DisplayP3;
    preferred.colorSpace.transferFunction = uint32_t(23000);

    QVector<SurfaceDescription> supportAttempts;
    auto selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [&supportAttempts](const SurfaceDescription &description) {
            supportAttempts.append(description);
            return std::holds_alternative<NamedPrimaries>(description.colorSpace.primaries) &&
                std::get<NamedPrimaries>(description.colorSpace.primaries) == NamedPrimaries::primaries_srgb &&
                std::holds_alternative<NamedTransferFunction>(description.colorSpace.transferFunction) &&
                std::get<NamedTransferFunction>(description.colorSpace.transferFunction) ==
                    NamedTransferFunction::transfer_function_gamma22;
        },
        [](const SurfaceDescription &) { return true; });
    QVERIFY(selected.requestedDescription);
    QCOMPARE(supportAttempts.size(), 3);
    QCOMPARE(std::get<NamedTransferFunction>(supportAttempts.at(1).colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_srgb);
    QCOMPARE(std::get<NamedTransferFunction>(supportAttempts.at(2).colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_gamma22);

    supportAttempts.clear();
    bool profileLookupAttempted = false;
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [&supportAttempts](const SurfaceDescription &description) {
            supportAttempts.append(description);
            return false;
        },
        [&profileLookupAttempted](const SurfaceDescription &) {
            profileLookupAttempted = true;
            return false;
        });
    QCOMPARE(supportAttempts.size(), 3);
    QVERIFY(!profileLookupAttempted);
    QVERIFY(!selected.requestedDescription);
    QCOMPARE(selected.profileSource, ProfileSource::BuiltInSrgb);
    QCOMPARE(selected.errorMessage,
             QStringLiteral("failed to find a suitable surface format for the compositor"));

    QVector<SurfaceDescription> profileAttempts;
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [&profileAttempts](const SurfaceDescription &description) {
            profileAttempts.append(description);
            return std::holds_alternative<NamedPrimaries>(description.colorSpace.primaries) &&
                std::get<NamedPrimaries>(description.colorSpace.primaries) == NamedPrimaries::primaries_srgb &&
                std::holds_alternative<NamedTransferFunction>(description.colorSpace.transferFunction) &&
                std::get<NamedTransferFunction>(description.colorSpace.transferFunction) ==
                    NamedTransferFunction::transfer_function_srgb;
        });
    QCOMPARE(selected.profileSource, ProfileSource::Generated);
    QCOMPARE(profileAttempts.size(), 4);
    QVERIFY(std::holds_alternative<KisColorimetryUtils::Colorimetry>(
        profileAttempts.at(1).colorSpace.primaries));
    QCOMPARE(std::get<NamedTransferFunction>(profileAttempts.at(1).colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_gamma22);
    QCOMPARE(std::get<NamedPrimaries>(profileAttempts.at(2).colorSpace.primaries),
             NamedPrimaries::primaries_srgb);
    QCOMPARE(std::get<NamedTransferFunction>(profileAttempts.at(3).colorSpace.transferFunction),
             NamedTransferFunction::transfer_function_srgb);

    profileAttempts.clear();
    selected = selectSurfaceDescription(
        KisCanvasSurfaceMode::Preferred,
        preferred,
        [](const SurfaceDescription &) { return true; },
        [&profileAttempts](const SurfaceDescription &description) {
            profileAttempts.append(description);
            return false;
        });
    QCOMPARE(profileAttempts.size(), 4);
    QVERIFY(!selected.requestedDescription);
    QCOMPARE(selected.profileSource, ProfileSource::BuiltInSrgb);
    QCOMPARE(selected.errorMessage,
             QStringLiteral("failed to create a profile for the compositor's preferred color space"));
}



void KisMultiSurfacePolicyTest::testProtocolNegotiation()
{
    using namespace KisCanvasSurfaceColorSpacePolicy;

    SurfaceDescription preferred;
    preferred.colorSpace.primaries = NamedPrimaries::primaries_srgb;
    preferred.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;

    NegotiationInput input;
    input.ready = true;
    input.surfaceMode = KisCanvasSurfaceMode::Preferred;
    input.options = {KoColorConversionTransformation::IntentRelativeColorimetric,
                     KoColorConversionTransformation::HighQuality |
                         KoColorConversionTransformation::BlackpointCompensation};
    input.compositorPreferred = preferred;

    auto result = negotiate(
        input,
        [](RenderIntent intent) { return intent == RenderIntent::render_intent_perceptual; },
        [](const SurfaceDescription &) { return true; },
        [](const SurfaceDescription &) { return true; });
    QCOMPARE(result.intent, RenderIntent::render_intent_perceptual);
    QCOMPARE(result.command, ProtocolCommand::Set);
    QVERIFY(result.requestedDescription);

    input.currentDescription = result.requestedDescription;
    input.currentIntent = result.intent;
    result = negotiate(input,
                       [](RenderIntent intent) { return intent == RenderIntent::render_intent_perceptual; },
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
    QCOMPARE(result.profileSource, ProfileSource::BuiltInSrgb);

    input.surfaceMode = KisCanvasSurfaceMode::Preferred;
    input.compositorPreferred = preferred;
    result = negotiate(input,
                       [](RenderIntent) { return true; },
                       [](const SurfaceDescription &) { return true; },
                       [](const SurfaceDescription &) { return false; });
    QVERIFY(!result.requestedDescription);
    QCOMPARE(result.profileSource, ProfileSource::BuiltInSrgb);

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
