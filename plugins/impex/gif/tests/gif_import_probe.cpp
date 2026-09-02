#include <KisDocument.h>

#include "../kis_gif_import.h"
#include "gif_multiframe_fixture.h"

#include <KisDocumentRegistry.h>
#include <PkScopedPointer.h>

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

namespace
{

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const std::vector<char> bytes = GifMultiframeFixture::create();
    require(!bytes.empty(), "two-image GIF fixture must encode");
    require(GifMultiframeFixture::hasExpectedStructure(bytes),
            "fixture must contain two descriptors with offset and local palette");

    GifTestMemoryStream input(bytes);
    require(input.open(PkStream::ReadOnly), "two-image input stream must open");
    PkScopedPointer<KisDocument> document(KisDocumentRegistry::instance()->createDocument());
    require(document, "GIF import probe must create a document");

    KisGIFImport importer(nullptr, PkVariantList());
    KisImportExportErrorCode result =
        importer.convert(document.data(), &input, KisPropertiesConfigurationSP());
    require(!(result == ImportExportCodes::FileFormatIncorrect),
            "KisGIFImport must not report FileFormatIncorrect for two valid descriptors");
    require(result.isOk(), "KisGIFImport must accept a valid multi-image GIF");
    require(document->image(), "KisGIFImport must install the decoded image");
    return 0;
}
