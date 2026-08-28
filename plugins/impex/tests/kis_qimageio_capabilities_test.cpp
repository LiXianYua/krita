/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../qimageio/kis_qimageio_export.h"

#if defined(KIS_QIMAGEIO_CAPABILITIES_TEST_SHELL)
#include <KisExportCheckRegistry.h>
#endif

#include <cstdlib>
#include <iostream>

#if defined(KIS_QIMAGEIO_CAPABILITIES_TEST_SHELL)
namespace
{

class TestExportCheck final : public KisExportCheckBase
{
public:
    TestExportCheck()
        : KisExportCheckBase(PkString("ColorModelPerLayerCheck/RGBA/U8"), SUPPORTED)
    {
    }

    bool checkNeeded(KisImageSP) const override
    {
        return false;
    }

    Level check(KisImageSP) const override
    {
        return SUPPORTED;
    }
};

class TestExportCheckFactory final : public KisExportCheckFactory
{
public:
    KisExportCheckBase *create(KisExportCheckBase::Level, const PkString &) override
    {
        return new TestExportCheck;
    }

    PkString id() const override
    {
        return PkString("ColorModelPerLayerCheck/RGBA/U8");
    }
};

} // namespace

KisExportCheckRegistry::KisExportCheckRegistry() = default;

KisExportCheckRegistry::~KisExportCheckRegistry() = default;

KisExportCheckRegistry *KisExportCheckRegistry::instance()
{
    static KisExportCheckRegistry registry;
    static bool initialized = false;
    if (!initialized) {
        registry.add(new TestExportCheckFactory);
        initialized = true;
    }
    return &registry;
}

extern "C" void __wrap__ZN21KisImportExportFilter23addSupportedColorModelsE6PkListISt4pairI4KoIDS2_EERK8PkStringN18KisExportCheckBase5LevelE(
    KisImportExportFilter *,
    PkList<std::pair<KoID, KoID> >,
    const PkString &,
    KisExportCheckBase::Level)
{
}
#endif

int main()
{
    KisQImageIOExport filter(nullptr, PkVariantList());
    filter.setMimeType(PkString("image/bmp"));

    const auto capabilities = filter.exportChecks();
    if (capabilities.empty()) {
        std::cerr << "QImageIO export capability initialization returned no checks\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
