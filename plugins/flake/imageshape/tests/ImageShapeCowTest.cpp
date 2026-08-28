/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../ImageShape.h"
#include "../ImageShapePlugin.h"

#include <KoShapeRegistry.h>

#include <memory>

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
    return viewBoxResult ? viewBoxResult : registrationIsIdempotentAndLive();
}
