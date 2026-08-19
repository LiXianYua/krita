#pragma once

#include <QObject>

class ImageCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstruction();
    void constructArgb32();
    void constructIndexed8();
    void constructMono();
    void isNullThreeWays();
    void rectAndSize();
    void colorCount();

    // ---- Task 2：像素访问与写入 ----
    void pixelArgb32FillAndSetPixelRoundtrip();
    void pixelRgb32ForcesOpaqueAlpha();
    void pixelRgba8888ByteOrderAndRoundtrip();
    void pixelRgba64Roundtrip();
    void pixelGrayscale8QGrayFormula();
    void pixelIndexed8SetPixelIsIndexNotColor();
    void pixelMonoBitOrderIsMsbFirst();
    void pixelMonoLsbBitOrderIsLsbFirst();
    void fillGlobalColorExactValues();
    void fillUintIsRawPassthroughOnArgb32();
    void outOfBoundsCoordinatesAreSafe();
    void colorTableAccessors();
    void allGrayBehavior();
    void allGrayIndexedAndAlpha8Semantics();

    // ---- Task 2：detach 时机 ----
    void scanLineDetachesConstScanLineDoesNot();
    void bitsDetachesConstBitsDoesNot();
    void pixelDoesNotDetachSetPixelDoes();
    void outOfBoundsSetPixelDoesNotDetach();
    void fillDetaches();
    void colorTableWritersDetachReadersDoNot();

    // ---- Task 3：格式转换、派生操作 ----
    void copyIsUnconditionalDeepCopy();
    void convertToFormatSameFormatShares();
    void convertToFormatCrossFormatRoundtrip();
    void convertToMutatesInPlace();

    // ---- Fix round 1：convertToFormat(Format, colorTable) 重载 ----
    void convertToFormatWithColorTableNearestColorMatch();
    void convertToFormatWithColorTableSmallPaletteNearestByRgbDistance();
    void convertToFormatWithColorTableEmptyTableIsSafe();

    void operatorEqualityFourScenarios();
    void devicePixelRatioAccessorsAndPassthrough();

    void scaledFastNearestNeighborMagnifyAndShrink();
    void scaledKeepAspectRatioClampsToOne();
    void scaledSameSizeShares();

    void transformedIdentityShares();
    void transformedTranslateCancelsBoundingRectOffset();
    void transformedRotate90ComposesToIdentity();
    void transformedShearOutOfBoundsIsTransparent();
    void transformedSmoothBilinearBlendsNeighbors();
    void transformedSmoothIndexedFallsBackToNearest();
};
