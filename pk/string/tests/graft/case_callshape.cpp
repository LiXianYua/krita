// This is not plugins/impex/csv/csv_loader.cpp, csv_saver.cpp, or
// libs/resources/KisResourceModel.cpp. It is a driver that reproduces their
// PkString call shapes verbatim. The real translation units require QObject,
// QIODevice, QVector, KisDocument, and resource-model targets outside R-31's
// pk/string lock, so they cannot be compiled against the string-only shim.

#include <PkString.h>

#include <cstdio>

PkString csvPathShape(PkString path)
{
    // csv_loader.cpp:75 / csv_saver.cpp:78
    if (path.right(4).toUpper() == ".CSV")
        path = path.left(path.size() - 4);
    return path;
}

bool resourceSortShape(const PkString& nameLeft, const PkString& nameRight)
{
    // KisResourceModel.cpp:1116
    return nameLeft.toLower() < nameRight.toLower();
}

int main()
{
    // Expected values were captured from Qt 5.15.7 with the same call shapes.
    const PkString path = csvPathShape(PkString("/tmp/Scene.CsV"));
    const PkString lower = PkString("\xC3\x84pfel").toLower();
    const bool less = resourceSortShape(PkString("\xC3\x84pfel"), PkString("zebra"));
    if (path != PkString("/tmp/Scene") || lower != PkString("\xC3\xA4pfel") || less) {
        std::printf("case graft FAILED path=%s lower=%s less=%d\n",
                    path.PkToUtf8().c_str(), lower.PkToUtf8().c_str(), static_cast<int>(less));
        return 1;
    }
    std::printf("case graft OK: csv_loader/csv_saver + KisResourceModel call shapes\n");
    return 0;
}
