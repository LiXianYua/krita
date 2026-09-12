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
    // 修复轮（评审 Minor）：Mono/MonoLSB 空表时 colorTable()/color(i) 合成默认黑/白表
    void monoDefaultColorTableSynthesized();

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
    // ---- 修复轮 1：const scanLine/bits 重载 ----
    void constScanLineConstBitsOverloadsDoNotDetach();
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

    // ---- R-75：文件 I/O 与原地像素操作 ----
    // 文件 I/O 的定义在 pkimageio（pk/image/PkImageFileIo.cpp），所以 test_pkimage
    // 额外链了 pkimageio（见 pk/image/CMakeLists.txt）；本文件仍只测 PkImage 的
    // 公开行为，不碰编解码内部。
    void invertPixelsArgb32BothModes();
    void invertPixelsPremultipliedBothModes();
    void invertPixelsDefaultModeIsInvertRgb();
    void invertPixelsRgb32HasNoAlphaChannel();
    void invertPixelsDetaches();

    void fileIoRoundTripLoadSaveLoadIsPixelExact();
    void fileIoConstructFromPathMatchesLoad();
    void fileIoUnopenablePathIsNullAndDoesNotThrow();
    void fileIoNonPngFixtureLoads();
    void fileIoSaveFailureModesReturnFalse();
};
