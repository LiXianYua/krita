// R-74：pk 测试栈无 QCoreApplication。原先构造 QCoreApplication 只为界定主线程
// （真 Qt 下测试入口建一个应用对象）；pk 栈的等价物是 PkThread::registerMainThread()
// + PkThreadCallQueue::warmUpCurrentThread()（同 sdk/tests/simpletest.h 的
// SIMPLE_MAIN_IMPL）。这是入口适配，测试体一字未改。
#include <PkThread.h>
#include <PkThreadCallQueue.h>

#include <KisDocument.h>

#include "../kis_gif_import.h"
#include "gif_multiframe_fixture.h"

#include <KisDocumentRegistry.h>
#include <PkScopedPointer.h>

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
    PkThread::registerMainThread();
    PkThreadCallQueue::warmUpCurrentThread();
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
