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

// ---------------------------------------------------------------------------
// Task 2：像素访问与写入
// ---------------------------------------------------------------------------

void ImageCase::pixelArgb32FillAndSetPixelRoundtrip()
{
    PkImage img(2, 2, PkImage::Format_ARGB32);
    img.fill(0xFF112233u);
    PK_COMPARE(img.pixel(0, 0), 0xFF112233u);
    PK_COMPARE(img.pixel(1, 1), 0xFF112233u);

    img.setPixel(1, 1, 0x80AABBCCu);
    PK_COMPARE(img.pixel(1, 1), 0x80AABBCCu);
    // 没被 setPixel 碰过的像素不受影响
    PK_COMPARE(img.pixel(0, 0), 0xFF112233u);

    // pixelColor()/setPixelColor() 在直接色格式上与 pixel()/setPixel() 同义
    // （简化决策：没有 PkColor，复用同一套 uint32_t ARGB32 打包值）
    img.setPixelColor(0, 1, 0x11223344u);
    PK_COMPARE(img.pixelColor(0, 1), 0x11223344u);
    PK_COMPARE(img.pixel(0, 1), 0x11223344u);
}

void ImageCase::pixelRgb32ForcesOpaqueAlpha()
{
    PkImage img(1, 1, PkImage::Format_RGB32);
    // 写入的 alpha 字节（0x00）应被强制不透明（0xff），Format_RGB32 的存储
    // 语义就是"永远不透明"。
    img.setPixel(0, 0, 0x00112233u);
    PK_COMPARE(img.pixel(0, 0), 0xFF112233u);

    // 默认零初始化像素也应读回不透明（alpha 字节的原始位是 0）
    PkImage untouched(1, 1, PkImage::Format_RGB32);
    PK_COMPARE(untouched.pixel(0, 0), 0xFF000000u);
}

void ImageCase::pixelRgba8888ByteOrderAndRoundtrip()
{
    PkImage img(1, 1, PkImage::Format_RGBA8888);
    // A=0x11 R=0x22 G=0x33 B=0x44
    img.setPixel(0, 0, 0x11223344u);
    PK_COMPARE(img.pixel(0, 0), 0x11223344u);

    // 内存字节序固定 R,G,B,A（与 ARGB32 的字长打包顺序不同，这是 8888 格式
    // 系列的定义特征）。
    const uint8_t *row = img.constScanLine(0);
    PK_VERIFY(row != nullptr);
    PK_COMPARE(static_cast<int>(row[0]), 0x22); // R
    PK_COMPARE(static_cast<int>(row[1]), 0x33); // G
    PK_COMPARE(static_cast<int>(row[2]), 0x44); // B
    PK_COMPARE(static_cast<int>(row[3]), 0x11); // A
}

void ImageCase::pixelRgba64Roundtrip()
{
    PkImage img(1, 1, PkImage::Format_RGBA64);
    // 8bit -> 16bit 展宽用 c*257（(c<<8)|c），16bit -> 8bit 降采样取高字节（>>8）：
    // 对 0..255 范围的整数这一对操作是精确互逆的（(c*257)>>8 == c），所以
    // 经过 setPixel/pixel 的 8 位往返值不失真。
    img.setPixel(0, 0, 0xAABBCCDDu);
    PK_COMPARE(img.pixel(0, 0), 0xAABBCCDDu);
}

void ImageCase::pixelGrayscale8QGrayFormula()
{
    PkImage img(1, 1, PkImage::Format_Grayscale8);

    // r=g=b 的输入是灰度化公式的不动点，往返精确
    img.setPixel(0, 0, 0xFF808080u);
    PK_COMPARE(img.pixel(0, 0), 0xFF808080u);

    // 纯红：qGray(255,0,0) = (255*11+0*16+0*5)/32 = 2805/32 = 87 = 0x57
    // （公式逐字照抄真 Qt qcolor.h 的 qGray(int,int,int)）
    img.setPixel(0, 0, 0xFFFF0000u);
    PK_COMPARE(img.pixel(0, 0), 0xFF575757u);
}

void ImageCase::pixelIndexed8SetPixelIsIndexNotColor()
{
    // 逐字照搬 brief 里的探针第 8 组：setPixel 写的是颜色表索引，不是颜色本身。
    PkImage img(2, 2, PkImage::Format_Indexed8);
    img.setColorCount(2);
    img.setColor(0, 0xFF000000u);
    img.setColor(1, 0xFFFFFFFFu);
    img.setPixel(0, 0, 1);

    PK_COMPARE(img.pixelIndex(0, 0), 1);
    PK_COMPARE(img.pixel(0, 0), 0xFFFFFFFFu);

    // 未写过的像素默认索引 0 -> 颜色表第 0 项
    PK_COMPARE(img.pixelIndex(1, 1), 0);
    PK_COMPARE(img.pixel(1, 1), 0xFF000000u);
}

void ImageCase::pixelMonoBitOrderIsMsbFirst()
{
    PkImage img(8, 1, PkImage::Format_Mono);
    img.setColor(0, 0xFF000000u);
    img.setColor(1, 0xFFFFFFFFu);

    // Mono 是 MSB-first：x=0 对应字节的第 7 位（最高位）
    img.setPixel(0, 0, 1);
    const uint8_t *row = img.constScanLine(0);
    PK_VERIFY(row != nullptr);
    PK_COMPARE(static_cast<int>(row[0]), 0x80);

    PK_COMPARE(img.pixelIndex(0, 0), 1);
    PK_COMPARE(img.pixel(0, 0), 0xFFFFFFFFu);
    PK_COMPARE(img.pixelIndex(1, 0), 0);
    PK_COMPARE(img.pixel(1, 0), 0xFF000000u);
}

void ImageCase::pixelMonoLsbBitOrderIsLsbFirst()
{
    PkImage img(8, 1, PkImage::Format_MonoLSB);
    img.setColor(0, 0xFF000000u);
    img.setColor(1, 0xFFFFFFFFu);

    // MonoLSB 是 LSB-first：x=0 对应字节的第 0 位（最低位）
    img.setPixel(0, 0, 1);
    const uint8_t *row = img.constScanLine(0);
    PK_VERIFY(row != nullptr);
    PK_COMPARE(static_cast<int>(row[0]), 0x01);

    PK_COMPARE(img.pixelIndex(0, 0), 1);
    PK_COMPARE(img.pixel(0, 0), 0xFFFFFFFFu);
}

void ImageCase::fillGlobalColorExactValues()
{
    // 5 个真实调用点用到的值 + black 顺手做，精确 RGBA 逐字来自
    // pk/geometry/PkGlobal.h 顶部注释里的探针实测。
    PkImage img(1, 1, PkImage::Format_ARGB32);

    img.fill(Qt::white);
    PK_COMPARE(img.pixel(0, 0), 0xFFFFFFFFu);

    img.fill(Qt::black);
    PK_COMPARE(img.pixel(0, 0), 0xFF000000u);

    img.fill(Qt::red);
    PK_COMPARE(img.pixel(0, 0), 0xFFFF0000u);

    img.fill(Qt::gray);
    PK_COMPARE(img.pixel(0, 0), 0xFFA0A0A4u); // (160,160,164) 不是 (128,128,128)

    img.fill(Qt::transparent);
    PK_COMPARE(img.pixel(0, 0), 0x00000000u);
}

void ImageCase::fillUintIsRawPassthroughOnArgb32()
{
    // 逐字照搬 brief 探针第 10 组。
    PkImage img(2, 2, PkImage::Format_ARGB32);
    img.fill(Qt::red);
    PK_COMPARE(img.pixel(0, 0), 0xFFFF0000u);

    img.fill(static_cast<uint32_t>(0xFF112233u));
    PK_COMPARE(img.pixel(0, 0), 0xFF112233u);
    PK_COMPARE(img.pixel(1, 1), 0xFF112233u);
}

void ImageCase::outOfBoundsCoordinatesAreSafe()
{
    PkImage img(2, 2, PkImage::Format_ARGB32);
    img.fill(0xFFFFFFFFu);

    // 越界读：确定性返回 0，不崩溃
    PK_COMPARE(img.pixel(-1, -1), 0u);
    PK_COMPARE(img.pixel(2, 2), 0u);
    PK_COMPARE(img.pixel(0, 2), 0u);
    PK_COMPARE(img.pixelColor(-1, 0), 0u);
    PK_COMPARE(img.pixelIndex(-1, -1), 0);

    // 越界写：no-op，不崩溃，也不影响图内像素
    img.setPixel(-1, -1, 0x11223344u);
    img.setPixel(5, 5, 0x11223344u);
    PK_COMPARE(img.pixel(0, 0), 0xFFFFFFFFu);

    // 行号越界：scanLine/constScanLine 返回空指针，不做越界指针算术
    PK_VERIFY(img.constScanLine(-1) == nullptr);
    PK_VERIFY(img.constScanLine(2) == nullptr);
    PK_VERIFY(img.scanLine(100) == nullptr);
}

void ImageCase::colorTableAccessors()
{
    PkImage img(2, 2, PkImage::Format_Indexed8);
    PK_COMPARE(img.colorTable().size(), static_cast<size_t>(0));

    img.setColorCount(3);
    PK_COMPARE(img.colorTable().size(), static_cast<size_t>(3));

    img.setColor(0, 0x11223344u);
    PK_COMPARE(img.color(0), 0x11223344u);

    // 越界读：确定性返回 0
    PK_COMPARE(img.color(-1), 0u);
    PK_COMPARE(img.color(5), 0u);

    // setColor 越界写自动扩容
    img.setColor(5, 0xAABBCCDDu);
    PK_COMPARE(img.colorTable().size(), static_cast<size_t>(6));
    PK_COMPARE(img.color(5), 0xAABBCCDDu);

    std::vector<uint32_t> table{0x1u, 0x2u, 0x3u};
    img.setColorTable(table);
    PK_COMPARE(img.colorTable().size(), static_cast<size_t>(3));
    PK_COMPARE(img.color(1), 0x2u);

    // 非索引格式：colorTable() 恒返回空表（探针第 9 组）
    PkImage argb(2, 2, PkImage::Format_ARGB32);
    PK_COMPARE(argb.colorTable().size(), static_cast<size_t>(0));
    // setColor/setColorTable 在非索引格式上不崩溃即可，行为不要求有意义
    argb.setColor(0, 0xFFFFFFFFu);
}

void ImageCase::allGrayBehavior()
{
    // 本身就是灰度/单色语义的格式：直接 true
    PkImage gray8(2, 2, PkImage::Format_Grayscale8);
    PK_VERIFY(gray8.allGray());

    PkImage mono(2, 2, PkImage::Format_Mono);
    PK_VERIFY(mono.allGray());

    // 直接色格式：逐像素判 R==G==B
    PkImage argbGray(2, 2, PkImage::Format_ARGB32);
    argbGray.fill(0xFF808080u);
    PK_VERIFY(argbGray.allGray());

    PkImage argbColor(2, 2, PkImage::Format_ARGB32);
    argbColor.fill(Qt::red);
    PK_VERIFY(!argbColor.allGray());
}

// ---------------------------------------------------------------------------
// Task 2：detach 时机——借鉴 pk/container 单测里 PkUseCount()/PkIsSharedWith()
// 的用法，钉住"写方法必须经 PkMut()、读方法绝不 detach"这条硬约束。
// ---------------------------------------------------------------------------

void ImageCase::scanLineDetachesConstScanLineDoesNot()
{
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PK_COMPARE(a.PkUseCount(), 1L);

    PkImage b(a);
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(b));

    (void)a.constScanLine(0);
    PK_VERIFY(a.PkIsSharedWith(b)); // const 路径绝不 detach

    (void)a.scanLine(0);
    PK_VERIFY(!a.PkIsSharedWith(b)); // 非 const 路径必须 detach
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(b.PkUseCount(), 1L);
}

void ImageCase::bitsDetachesConstBitsDoesNot()
{
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    (void)a.constBits();
    PK_VERIFY(a.PkIsSharedWith(b));

    (void)a.bits();
    PK_VERIFY(!a.PkIsSharedWith(b));
}

void ImageCase::pixelDoesNotDetachSetPixelDoes()
{
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    (void)a.pixel(0, 0);
    (void)a.pixelColor(0, 0);
    (void)a.pixelIndex(0, 0);
    PK_VERIFY(a.PkIsSharedWith(b));

    a.setPixel(0, 0, 0xFF000000u);
    PK_VERIFY(!a.PkIsSharedWith(b));
}

void ImageCase::outOfBoundsSetPixelDoesNotDetach()
{
    // 一次不会真正写入任何字节的越界调用不该逼共享缓冲区分裂。
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    a.setPixel(-1, -1, 0xFFFFFFFFu);
    a.setPixel(5, 5, 0xFFFFFFFFu);
    a.setPixelColor(-1, -1, 0xFFFFFFFFu);
    PK_VERIFY(a.PkIsSharedWith(b));
}

void ImageCase::fillDetaches()
{
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    a.fill(0xFFFFFFFFu);
    PK_VERIFY(!a.PkIsSharedWith(b));
    // b 未被 a 的写入影响（独立副本，COW 语义）
    PK_COMPARE(b.pixel(0, 0), 0x00000000u);
}

void ImageCase::colorTableWritersDetachReadersDoNot()
{
    PkImage a(2, 2, PkImage::Format_Indexed8);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    (void)a.colorTable();
    (void)a.color(0);
    PK_VERIFY(a.PkIsSharedWith(b));

    a.setColorCount(2);
    PK_VERIFY(!a.PkIsSharedWith(b));

    PkImage c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    a.setColor(0, 0xFF000000u);
    PK_VERIFY(!a.PkIsSharedWith(c));

    PkImage d(a);
    PK_VERIFY(a.PkIsSharedWith(d));
    a.setColorTable(std::vector<uint32_t>{0x1u, 0x2u});
    PK_VERIFY(!a.PkIsSharedWith(d));
}

PK_TEST_MAIN(ImageCase)
