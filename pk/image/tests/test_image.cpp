#include "image_case.h"

#include "../PkImage.h"

#include "pk_binder_image_case.inc"

void ImageCase::defaultConstruction()
{
    PkImage img;
    PK_VERIFY(img.isNull());
    PK_COMPARE(img.width(), 0);
    PK_COMPARE(img.height(), 0);
    PK_COMPARE(static_cast<int>(img.format()), static_cast<int>(PkImage::Format_Invalid));
}

void ImageCase::constructArgb32()
{
    PkImage img(7, 3, PkImage::Format_ARGB32);
    PK_VERIFY(!img.isNull());
    PK_COMPARE(img.depth(), 32);
    PK_COMPARE(img.bytesPerLine(), 28);
    PK_COMPARE(img.sizeInBytes(), static_cast<long long>(84));
}

void ImageCase::constructIndexed8()
{
    PkImage img(7, 3, PkImage::Format_Indexed8);
    PK_COMPARE(img.depth(), 8);
    PK_COMPARE(img.bytesPerLine(), 8);
    PK_COMPARE(img.sizeInBytes(), static_cast<long long>(24));
}

void ImageCase::constructMono()
{
    PkImage img(7, 3, PkImage::Format_Mono);
    PK_COMPARE(img.depth(), 1);
    PK_COMPARE(img.bytesPerLine(), 4);
    PK_COMPARE(img.sizeInBytes(), static_cast<long long>(12));
}

void ImageCase::isNullThreeWays()
{
    PK_VERIFY(PkImage(0, 5, PkImage::Format_ARGB32).isNull());
    PK_VERIFY(PkImage(5, 0, PkImage::Format_ARGB32).isNull());
    PK_VERIFY(PkImage(5, 5, PkImage::Format_Invalid).isNull());
}

void ImageCase::rectAndSize()
{
    PkImage img(3, 4, PkImage::Format_ARGB32);
    PkRect r = img.rect();
    PK_COMPARE(r.x(), 0);
    PK_COMPARE(r.y(), 0);
    PK_COMPARE(r.width(), 3);
    PK_COMPARE(r.height(), 4);
}

void ImageCase::colorCount()
{
    PkImage mono(2, 2, PkImage::Format_Mono);
    PK_COMPARE(mono.colorCount(), 2);

    PkImage indexed(2, 2, PkImage::Format_Indexed8);
    PK_COMPARE(indexed.colorCount(), 0);

    PkImage argb(2, 2, PkImage::Format_ARGB32);
    PK_COMPARE(argb.colorCount(), 0);
}

PK_TEST_MAIN(ImageCase)
