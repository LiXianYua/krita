#include "KoFontGlyphModel.h"

#include <PkObject.h>
#include <PkVariant.h>

#include <type_traits>
#include <utility>

static_assert(std::is_base_of<PkObject, KoFontGlyphModel>::value,
              "the exported glyph model must have native object lifetime");
static_assert(std::is_same<
                  decltype(std::declval<const KoFontGlyphModel &>().data(
                      KoFontGlyphModel::Index(), KoFontGlyphModel::DisplayRole)),
                  PkVariant>::value,
              "the exported glyph model must expose native index/value types");

int main()
{
    const KoFontGlyphModel::Index invalid;
    return invalid.isValid() ? 1 : 0;
}
