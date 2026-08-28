/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../ImageShape.h"
#include "../ImageShapePngData.h"
#include "../ImageShapePlugin.h"

#include <KoShapeRegistry.h>
#include <PkXmlDocument.h>
#include <SvgLoadingContext.h>
#include <SvgSavingContext.h>

#include <memory>
#include <type_traits>

static_assert(!std::is_copy_assignable_v<ImageShape>);

namespace
{
int imageMutationDetachesFromClone()
{
    ImageShape original;
    const PkImage originalImage(3, 2, PkImage::Format_ARGB32_Premultiplied);
    const PkImage replacementImage(5, 4, PkImage::Format_RGB32);
    const PkImage cloneReplacementImage(7, 6, PkImage::Format_Grayscale8);
    original.setImage(originalImage);

    std::unique_ptr<KoShape> cloneBase(original.cloneShape());
    ImageShape *clone = dynamic_cast<ImageShape *>(cloneBase.get());
    if (!clone) return 1;

    original.setImage(replacementImage);
    if (original.image() != replacementImage) return 2;
    if (clone->image() != originalImage) return 3;

    clone->setImage(cloneReplacementImage);
    if (clone->image() != cloneReplacementImage) return 4;
    if (original.image() != replacementImage) return 5;
    return 0;
}

int viewBoxMutationDetachesFromClone()
{
    ImageShape original;
    const PkTransform originalTransform = PkTransform::fromScale(2.0, 3.0);
    const PkTransform replacementTransform = PkTransform::fromTranslate(11.0, 13.0);
    const PkTransform cloneReplacementTransform = PkTransform::fromScale(17.0, 19.0);
    original.setViewBoxTransform(originalTransform);

    std::unique_ptr<KoShape> cloneBase(original.cloneShape());
    ImageShape *clone = dynamic_cast<ImageShape *>(cloneBase.get());
    if (!clone) return 10;

    original.setViewBoxTransform(replacementTransform);
    if (original.viewBoxTransform() != replacementTransform) return 11;
    if (clone->viewBoxTransform() != originalTransform) return 12;

    clone->setViewBoxTransform(cloneReplacementTransform);
    if (clone->viewBoxTransform() != cloneReplacementTransform) return 13;
    if (original.viewBoxTransform() != replacementTransform) return 14;
    return 0;
}

PkXmlElement imageElement(PkXmlDocument &document,
                          const PkImage &image,
                          const PkString &aspect)
{
    PkXmlElement element = document.createElement("image");
    element.setAttribute("x", "0");
    element.setAttribute("y", "0");
    element.setAttribute("width", "20");
    element.setAttribute("height", "10");
    element.setAttribute("xlink:href", ImageShapePngData::encodeDataUri(image));
    element.setAttribute("preserveAspectRatio", aspect);
    return element;
}

PkString savedAspect(ImageShape &shape)
{
    SvgSavingContext context;
    if (!shape.saveSvg(context)) return {};
    return context.shapeWriter().attribute("preserveAspectRatio");
}

int loadSvgDetachesImageParserAndTransformTogether()
{
    PkImage firstImage(2, 1, PkImage::Format_ARGB32);
    firstImage.setPixel(0, 0, 0xff102030u);
    firstImage.setPixel(1, 0, 0xff405060u);
    PkXmlDocument firstDocument;
    PkXmlElement firstElement = imageElement(firstDocument, firstImage, "xMinYMin meet");
    SvgLoadingContext loadingContext;

    ImageShape original;
    if (!original.loadSvg(firstElement, loadingContext)) return 30;
    std::unique_ptr<KoShape> cloneBase(original.cloneShape());
    ImageShape *clone = dynamic_cast<ImageShape *>(cloneBase.get());
    if (!clone) return 31;
    const PkImage clonedImage = clone->image();
    const PkTransform clonedTransform = clone->viewBoxTransform();

    PkImage secondImage(4, 2, PkImage::Format_RGB16);
    PkXmlDocument secondDocument;
    PkXmlElement secondElement = imageElement(secondDocument, secondImage, "xMaxYMax slice");
    if (!original.loadSvg(secondElement, loadingContext)) return 32;
    if (original.image() == clonedImage) return 33;
    if (clone->image() != clonedImage) return 34;
    if (original.viewBoxTransform() == clonedTransform) return 35;
    if (clone->viewBoxTransform() != clonedTransform) return 36;
    if (savedAspect(original) != "xMaxYMax slice") return 37;
    if (savedAspect(*clone) != "xMinYMin meet") return 38;
    return 0;
}

int registrationIsIdempotentAndLive()
{
    KoShapeRegistry *registry = KoShapeRegistry::instance();
    registerImageShape();
    const int countAfterFirstCall = registry->count();
    registerImageShape();
    if (registry->count() != countAfterFirstCall) return 20;
    if (!registry->contains(ImageShapeId)) return 21;
    return 0;
}
} // namespace

int main()
{
    const int imageResult = imageMutationDetachesFromClone();
    if (imageResult) return imageResult;
    const int viewBoxResult = viewBoxMutationDetachesFromClone();
    if (viewBoxResult) return viewBoxResult;
    const int loadResult = loadSvgDetachesImageParserAndTransformTogether();
    return loadResult ? loadResult : registrationIsIdempotentAndLive();
}
