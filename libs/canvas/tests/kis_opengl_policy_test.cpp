/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include "opengl/KisOpenGLPolicy.h"

#include <algorithm>

Q_DECLARE_METATYPE(KisOpenGLPolicy::Renderer)

class KisOpenGLPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRendererConfigMapping_data();
    void testRendererConfigMapping();
    void testIntelDriverBlacklist_data();
    void testIntelDriverBlacklist();
    void testSurfaceRequestPolicy();
    void testCapabilityProbeSequence();
    void testRendererOrdering();
    void testDriverWorkarounds();
};

void KisOpenGLPolicyTest::testRendererConfigMapping_data()
{
    using Renderer = KisOpenGLPolicy::Renderer;
    QTest::addColumn<Renderer>("renderer");
    QTest::addColumn<QString>("config");

    QTest::newRow("none") << Renderer::None << QStringLiteral("none");
    QTest::newRow("software") << Renderer::Software << QStringLiteral("software");
    QTest::newRow("desktop") << Renderer::DesktopGL << QStringLiteral("desktop");
    QTest::newRow("angle") << Renderer::OpenGLES << QStringLiteral("angle");
    QTest::newRow("auto") << Renderer::Auto << QStringLiteral("auto");
}

void KisOpenGLPolicyTest::testRendererConfigMapping()
{
    QFETCH(KisOpenGLPolicy::Renderer, renderer);
    QFETCH(QString, config);

    QCOMPARE(KisOpenGLPolicy::rendererToConfig(renderer), config);
    QCOMPARE(KisOpenGLPolicy::rendererFromConfig(config), renderer);
    QCOMPARE(KisOpenGLPolicy::rendererFromConfig(QStringLiteral("unexpected")),
             KisOpenGLPolicy::Renderer::Auto);
}

void KisOpenGLPolicyTest::testIntelDriverBlacklist_data()
{
    QTest::addColumn<QString>("renderer");
    QTest::addColumn<QString>("version");
    QTest::addColumn<bool>("windows");
    QTest::addColumn<bool>("blacklisted");

    QTest::newRow("lower-exclusive") << QStringLiteral("Intel HD") << QStringLiteral("20.19.15.4636") << true << false;
    QTest::newRow("bad-range") << QStringLiteral("Intel HD") << QStringLiteral("20.19.15.4637") << true << true;
    QTest::newRow("upper-exclusive") << QStringLiteral("Intel HD") << QStringLiteral("20.19.15.4729") << true << false;
    QTest::newRow("known-bad") << QStringLiteral("Intel HD") << QStringLiteral("20.19.15.4358") << true << true;
    QTest::newRow("new-format-bad-range") << QStringLiteral("Intel Arc") << QStringLiteral("31.0.101.4637") << true << false;
    QTest::newRow("unknown-intel") << QStringLiteral("Intel Arc") << QStringLiteral("rolling") << true << true;
    QTest::newRow("case-sensitive") << QStringLiteral("intel HD") << QStringLiteral("20.19.15.4637") << true << false;
    QTest::newRow("non-windows") << QStringLiteral("Intel HD") << QStringLiteral("20.19.15.4637") << false << false;
}

void KisOpenGLPolicyTest::testIntelDriverBlacklist()
{
    QFETCH(QString, renderer);
    QFETCH(QString, version);
    QFETCH(bool, windows);
    QFETCH(bool, blacklisted);

    QCOMPARE(KisOpenGLPolicy::intelDriverPolicy(renderer, version, windows).blacklisted, blacklisted);
}

void KisOpenGLPolicyTest::testSurfaceRequestPolicy()
{
    using namespace KisOpenGLPolicy;

    auto desktop = surfaceRequest(Renderer::DesktopGL, Platform::Other, false, false, false);
    QCOMPARE(desktop.majorVersion, 3);
    QCOMPARE(desktop.minorVersion, 3);
    QCOMPARE(desktop.profile, Profile::Compatibility);
    QCOMPARE(desktop.depthBufferBits, 24);
    QCOMPARE(desktop.stencilBufferBits, 8);
    QCOMPARE(desktop.swapInterval, 0);

    auto inhibited = surfaceRequest(Renderer::DesktopGL, Platform::Windows, true, true, true);
    QCOMPARE(inhibited.profile, Profile::None);
    QVERIFY(inhibited.deprecatedFunctions);
    QVERIFY(inhibited.debugContext);
    QCOMPARE(inhibited.swapInterval, 1);

    auto mac = surfaceRequest(Renderer::DesktopGL, Platform::MacOS, false, false, false);
    QCOMPARE(mac.majorVersion, 4);
    QCOMPARE(mac.minorVersion, 1);
    QCOMPARE(mac.profile, Profile::Core);

    auto angle = surfaceRequest(Renderer::OpenGLES, Platform::Windows, false, false, false);
    QCOMPARE(angle.majorVersion, 3);
    QCOMPARE(angle.minorVersion, 0);
    QCOMPARE(angle.profile, Profile::None);
    QCOMPARE(angle.angleRenderer, AngleRenderer::D3d11);

    auto software = surfaceRequest(Renderer::Software, Platform::Windows, false, false, false);
    QCOMPARE(software.angleRenderer, AngleRenderer::D3d11Warp);
}

void KisOpenGLPolicyTest::testCapabilityProbeSequence()
{
    using namespace KisOpenGLPolicy;

    const auto linuxSequence = defaultProbeSequence(Platform::Other);
    QCOMPARE(linuxSequence.size(), 4);
    QCOMPARE(linuxSequence.at(0), ProbeRequest({Renderer::Auto, false}));
    QCOMPARE(linuxSequence.at(1), ProbeRequest({Renderer::DesktopGL, false}));
    QCOMPARE(linuxSequence.at(2), ProbeRequest({Renderer::DesktopGL, true}));
    QCOMPARE(linuxSequence.at(3), ProbeRequest({Renderer::OpenGLES, true}));

    const auto windowsSequence = defaultProbeSequence(Platform::Windows);
    QCOMPARE(windowsSequence.size(), 5);
    QCOMPARE(windowsSequence.constLast(), ProbeRequest({Renderer::Software, false}));

    const auto macSequence = defaultProbeSequence(Platform::MacOS);
    QCOMPARE(macSequence.size(), 1);
    QCOMPARE(macSequence.constFirst(), ProbeRequest({Renderer::Auto, false}));

    QCOMPARE(rendererCandidates(false, false),
             QVector<Renderer>({Renderer::DesktopGL, Renderer::OpenGLES}));
    QCOMPARE(rendererCandidates(true, false), QVector<Renderer>({Renderer::OpenGLES}));
    QCOMPARE(rendererCandidates(false, true),
             QVector<Renderer>({Renderer::DesktopGL, Renderer::OpenGLES, Renderer::Software}));
}

void KisOpenGLPolicyTest::testRendererOrdering()
{
    using namespace KisOpenGLPolicy;

    SelectionPreferences preferences;
    preferences.preferredColorSpace = ColorSpace::Bt2020Pq;
    preferences.preferredRendererByUser = Renderer::Auto;
    preferences.preferredRendererByHdr = Renderer::OpenGLES;
    preferences.preferredRendererByQt = Renderer::DesktopGL;
    preferences.desktopBlacklisted = true;
    preferences.userPreferredBitDepth = 10;

    QVector<FormatCandidate> candidates {
        {Renderer::Software, ColorSpace::Bt2020Pq, 10},
        {Renderer::DesktopGL, ColorSpace::Bt2020Pq, 10},
        {Renderer::OpenGLES, ColorSpace::SRgb, 8},
        {Renderer::OpenGLES, ColorSpace::Bt2020Pq, 10},
    };

    std::stable_sort(candidates.begin(), candidates.end(),
                     [&preferences](const auto &lhs, const auto &rhs) {
                         return isPreferred(lhs, rhs, preferences);
                     });

    QCOMPARE(candidates.at(0).renderer, Renderer::OpenGLES);
    QCOMPARE(candidates.at(0).colorSpace, ColorSpace::Bt2020Pq);
    QCOMPARE(candidates.at(1).renderer, Renderer::DesktopGL);
    QCOMPARE(candidates.constLast().renderer, Renderer::Software);

    preferences.preferredRendererByUser = Renderer::Software;
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&preferences](const auto &lhs, const auto &rhs) {
                         return isPreferred(lhs, rhs, preferences);
                     });
    QCOMPARE(candidates.at(0).renderer, Renderer::Software);
}

void KisOpenGLPolicyTest::testDriverWorkarounds()
{
    using namespace KisOpenGLPolicy;

    QVERIFY(needsFenceWorkaround(true, QStringLiteral("AMD Radeon"), false));
    QVERIFY(!needsFenceWorkaround(false, QStringLiteral("AMD Radeon"), false));
    QVERIFY(needsFenceWorkaround(false, QStringLiteral("NVIDIA"), true));

    QVERIFY(!shouldUseTextureBuffers(true, true));
    QVERIFY(shouldUseTextureBuffers(false, true));
    QVERIFY(!shouldInvalidateBuffers(true, false));
    QVERIFY(shouldInvalidateBuffers(true, true));

    QVERIFY(rejectAngleD3d9(true, true, QStringLiteral("ANGLE (Direct3D9)")));
    QVERIFY(rejectAngleD3d9(true, true, QStringLiteral("angle direct3d9")));
    QVERIFY(!rejectAngleD3d9(false, true, QStringLiteral("ANGLE (Direct3D9)")));
}

SIMPLE_TEST_MAIN(KisOpenGLPolicyTest)

#include "kis_opengl_policy_test.moc"
