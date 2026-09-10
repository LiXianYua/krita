/*
 * Native selection cursor payloads.  The descriptor is deliberately free of
 * Qt resource identifiers; a UI host can consume the asset bytes directly.
 */
#ifndef SELECTION_TOOL_CURSOR_H
#define SELECTION_TOOL_CURSOR_H

#include <cstddef>
#include <string_view>
#include <array>

class KisCanvasToolServices;
class QCursor;
class KisCanvasCursorToken;

struct SelectionToolCursorDescriptor {
    std::string_view name;
    std::string_view assetPath;
    std::string_view sha256;
    std::size_t byteCount;
    int hotspotX;
    int hotspotY;
};

using SelectionToolCursorDescriptorList = std::array<SelectionToolCursorDescriptor, 40>;

const SelectionToolCursorDescriptor &selectionToolCursorDescriptor(std::string_view name);
const SelectionToolCursorDescriptorList &selectionToolCursorDescriptors();
QCursor selectionToolCursor(const KisCanvasToolServices *host,
                            const SelectionToolCursorDescriptor &descriptor);
KisCanvasCursorToken selectionToolCursorToken(const KisCanvasToolServices *host,
                                              const SelectionToolCursorDescriptor &descriptor);

#endif
