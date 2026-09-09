#include "selection_tool_cursor.h"

#include <KisCanvasToolServices.h>
#include <PkString.h>

#include <array>

namespace {
using D = SelectionToolCursorDescriptor;

// These are the checked-in payload files formerly registered by RCC.  Keeping
// their byte counts and digests beside the descriptor makes accidental cursor
// replacement observable to native hosts and to the independent oracle.
constexpr SelectionToolCursorDescriptorList descriptors{{
#define CURSOR(n, x, y, size, hash) D{n, n, hash, size, x, y}
    CURSOR("tool_contiguous_selection_cursor.png", 6, 6, 514, "de0d11f5b69106197c3ce52d67e3a19c40dc47b07638428b379fecc76d2093d7"),
    CURSOR("tool_contiguous_selection_cursor_add.png", 6, 6, 9024, "cce46b30d5be1b6581e675995edab139cc9d6406e7aa6632a5cc5a545ee8aee4"),
    CURSOR("tool_contiguous_selection_cursor_inter.png", 6, 6, 774, "f2c7515075225f8586373ba2488b4ecf6c71f1343e179bf232773ae8f1afdd93"),
    CURSOR("tool_contiguous_selection_cursor_sub.png", 6, 6, 9015, "15b4d6b3978086a8909ef647408e679a7a490327a6f038e1bb0994e641ca3bc1"),
    CURSOR("tool_contiguous_selection_cursor_symdiff.png", 6, 6, 758, "a1a7c495f7e7aa83a6f2217edfdb98fb6e3b3c6bd6952a37d0e6416c024f1bd6"),
    CURSOR("tool_elliptical_selection_cursor.png", 6, 6, 228, "9163178a2ea28696ecee12fc5b9640fdf8514de02bfde9c53c5217eb71da19cb"),
    CURSOR("tool_elliptical_selection_cursor_add.png", 6, 6, 8920, "d1a878bdf38cfd303517228af9a5a8adf6f6af1d410528fd6ac4cc62b25396cc"),
    CURSOR("tool_elliptical_selection_cursor_inter.png", 6, 6, 639, "fb8d129ca353300a8001d030cc8ae939c71ca379029c8809d2d53713d87c228a"),
    CURSOR("tool_elliptical_selection_cursor_sub.png", 6, 6, 256, "2ffe64436db8f0582cf3d9abec662ecea294f26d7f413ac09416cc859be9085e"),
    CURSOR("tool_elliptical_selection_cursor_symdiff.png", 6, 6, 668, "33e7ee7e23e8a83c8052403cfd9d80fa3945c660adfef2653f60179d065f01b3"),
    CURSOR("tool_elliptical_selection_enclose_eraser_cursor.png", 6, 6, 317, "3e6f8297178be15c0020f030afc24a2f7109bb3097bb1a2966849cf29dc3c60c"),
    CURSOR("tool_eraser_selection_cursor.png", 6, 6, 247, "ed89cea6bf635ad83e8cd8647b581c5a1aa5861e58f2908f2bd98d96b8559cc5"),
    CURSOR("tool_magnetic_selection_cursor.png", 6, 6, 297, "f50a2808bd331726b78562c490688d0b69e5d0771c6158948463222f6b0fa2ed"),
    CURSOR("tool_magnetic_selection_cursor_add.png", 6, 6, 335, "0bfd4d5cc3420b2f5f42849b7b6788f8bf94c6b4e676a8e8173af5de3154b683"),
    CURSOR("tool_magnetic_selection_cursor_inter.png", 6, 6, 713, "53cca23c5c1e235b103420862afa2645409541482edfdd74dc5485e1502bd62e"),
    CURSOR("tool_magnetic_selection_cursor_sub.png", 6, 6, 319, "741caffa78b3c236c5eff436977ecf7c238d1458a6e7e86e7f20a1380fe62fb0"),
    CURSOR("tool_magnetic_selection_cursor_symdiff.png", 6, 6, 693, "5bb4b5156ecebe604497327fbd000ed68ffe4dc4901d18fc2c9c37c62b1c9676"),
    CURSOR("tool_outline_selection_cursor.png", 5, 5, 285, "d00acf4cbdde333a84b00547a1e333896043b8cbdcde5af8126f4bedf054239d"),
    CURSOR("tool_outline_selection_cursor_add.png", 5, 5, 8953, "be362b51b1420a266cd69d9651a0b0977097a08ed0c4514053813afaf45074dc"),
    CURSOR("tool_outline_selection_cursor_inter.png", 5, 5, 670, "2a63107c4dde5fd4fd3e02909bb48a7e698a8b2a8fde8957453f5559f651f73f"),
    CURSOR("tool_outline_selection_cursor_sub.png", 5, 5, 290, "70c7e60c5db6f09c769e0d4d8c06f2852f1668b9940664a05a410e83de0b7132"),
    CURSOR("tool_outline_selection_cursor_symdiff.png", 5, 5, 689, "00542482e13d1031bbcdcd69da2177bf48c82eafb45d402cac5ed9a7fb0c9f2a"),
    CURSOR("tool_outline_selection_enclose_eraser_cursor.png", 5, 5, 340, "7b2d1dfeffaf69be6be666004f0e125c714ce0c7ed0685f55c7ee7b82109cda1"),
    CURSOR("tool_polygonal_selection_cursor.png", 6, 6, 9025, "77e5d8a06ef3d9d72930dc98e86614e04d704eb99eb6025d5ebfe3ffff0cc1d4"),
    CURSOR("tool_polygonal_selection_cursor_add.png", 6, 6, 9047, "9aabcb0428ae9a54bd1fb1faeb71cc77dfa3f538657ccf515f381db412258bf5"),
    CURSOR("tool_polygonal_selection_cursor_inter.png", 6, 6, 765, "49834710a0a92e740548aed52218755907c9c0b105be1e61eeaa94e5118ca28a"),
    CURSOR("tool_polygonal_selection_cursor_sub.png", 6, 6, 382, "c53c8b12e1f39acf7cc62071e64d6a35a5080b193ff3cf0a0bcd85effc9c284e"),
    CURSOR("tool_polygonal_selection_cursor_symdiff.png", 6, 6, 787, "bd16a03d822b24396a86d820c9717f4e517c27236120c2bf6f2e82ad0b26c8a0"),
    CURSOR("tool_polygonal_selection_enclose_eraser_cursor.png", 6, 6, 432, "6fdacf3407fcf92e71559479af5cc976b920ae8f65026f2804acc199eb1ca6fa"),
    CURSOR("tool_rectangular_selection_cursor.png", 6, 6, 155, "28fb324436e82137c44abc19ba946b706c8a2cb2ba61572adb5f4fcd68f2ad49"),
    CURSOR("tool_rectangular_selection_cursor_add.png", 6, 6, 8884, "bfd3c2fc056d3071324cacf56c95ac6ea1afe28474b19e7198d92c1f7bb5449a"),
    CURSOR("tool_rectangular_selection_cursor_inter.png", 6, 6, 604, "ae6c8689bbb28286d1b88051f552856d9d945516bf0812f2d586e5946dde686f"),
    CURSOR("tool_rectangular_selection_cursor_sub.png", 6, 6, 209, "74a7a96c7a2cf3fa19a73dca06df08db3f7c6fbf9d5d80718faf83d97ba627e7"),
    CURSOR("tool_rectangular_selection_cursor_symdiff.png", 6, 6, 627, "d9b498b38f690b0795d422a473f719b73d6d33edd7961be8fe182dd39b060433"),
    CURSOR("tool_rectangular_selection_enclose_eraser_cursor.png", 6, 6, 257, "ddffea28013284328e4431e56e37c25533fa9e7667334049cc690649397984fd"),
    CURSOR("tool_similar_selection_cursor.png", 6, 6, 295, "a9a3aab2e8c4afa8e484bb9d6202c456c65db8645515925fe5f3735230893d9b"),
    CURSOR("tool_similar_selection_cursor_add.png", 6, 6, 9010, "fa3bbeea5277d0061d65b0905a06223a199d0c60db3993f3d9d53c114a241d3c"),
    CURSOR("tool_similar_selection_cursor_inter.png", 6, 6, 737, "724c7bac931e347e222eb62ed4a092aed217d83ebf37f8c1fcd042fecf3ae1be"),
    CURSOR("tool_similar_selection_cursor_sub.png", 6, 6, 338, "782751ef1d079c04841bf6b9c8de74b450e9773058f3d0bc9518ce76d5e3ffd2"),
    CURSOR("tool_similar_selection_cursor_symdiff.png", 6, 6, 747, "92063f1c376e14d18d2fdfe8330b55b28ab9d8f429c570ef2fe8895b56715bac"),
#undef CURSOR
}};
}

const SelectionToolCursorDescriptor &selectionToolCursorDescriptor(std::string_view name)
{
    for (const auto &descriptor : descriptors) {
        if (descriptor.name == name) {
            return descriptor;
        }
    }
    return descriptors.front();
}

const SelectionToolCursorDescriptorList &selectionToolCursorDescriptors()
{
    return descriptors;
}

QCursor selectionToolCursor(const KisCanvasToolServices *host,
                            const SelectionToolCursorDescriptor &descriptor)
{
    return host->toolLoadCursor(PkString(descriptor.name.data()), descriptor.hotspotX, descriptor.hotspotY);
}
