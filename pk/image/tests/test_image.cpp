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

void ImageCase::allGrayIndexedAndAlpha8Semantics()
{
    // 对齐真 Qt QImage::allGray()（qimage.cpp:2680-2745，5.15）——oracle 对拍
    // 逼出原实现的 bug：Mono/MonoLSB 无脑 true、Alpha8 无脑 true 都是错的。

    // Mono 颜色表黑+白（都是灰）→ true。显式 setColorTable 让 colorTable 非空。
    PkImage mono(2, 2, PkImage::Format_Mono);
    mono.setColorTable({0xFF000000u, 0xFFFFFFFFu});
    PK_VERIFY(mono.allGray());

    // Mono 颜色表含非灰项（R=1 G=2 B=3）→ false。
    PkImage monoColor(2, 2, PkImage::Format_Mono);
    monoColor.setColorTable({0xFF000000u, 0xFF010203u});
    PK_VERIFY(!monoColor.allGray());

    // Indexed8 颜色表含非灰项 → false（不是只看像素索引）。
    PkImage indexedColor(2, 2, PkImage::Format_Indexed8);
    indexedColor.setColorTable({0xFF000000u, 0xFF010203u});
    PK_VERIFY(!indexedColor.allGray());

    // Indexed8 颜色表全灰 → true。
    PkImage indexedGray(2, 2, PkImage::Format_Indexed8);
    indexedGray.setColorTable({0xFF000000u, 0xFF808080u, 0xFFFFFFFFu});
    PK_VERIFY(indexedGray.allGray());

    // Alpha8：纯 alpha 通道，不是灰度 → false。
    PkImage alpha8(2, 2, PkImage::Format_Alpha8);
    PK_VERIFY(!alpha8.allGray());
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

// ---------------------------------------------------------------------------
// 修复轮 1：const scanLine/bits 重载——真 Qt QImage 的 const 重载（qimage.h:226/
// 217），compareQImagesImpl 拿 const QImage& 调 .scanLine(y) 解析到它。PkImage
// 补上后，断言转发目标指针与 constScanLine/constBits 相同，且绝不 detach。
// ---------------------------------------------------------------------------

void ImageCase::constScanLineConstBitsOverloadsDoNotDetach()
{
    PkImage a(2, 2, PkImage::Format_ARGB32);
    PkImage b(a);
    PK_VERIFY(a.PkIsSharedWith(b));
    const long before = a.PkUseCount();

    const PkImage &ca = a;

    // const scanLine(int) const 转发 constScanLine，返回同一行指针。
    const uint8_t *row = ca.scanLine(1);
    const uint8_t *rowRef = ca.constScanLine(1);
    PK_VERIFY(row == rowRef);

    // const bits() const 转发 constBits，返回同一缓冲指针。
    const uint8_t *bits = ca.bits();
    const uint8_t *bitsRef = ca.constBits();
    PK_VERIFY(bits == bitsRef);

    // 全程绝不 detach：共享关系保持，use count 不变。
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), before);
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

// ---------------------------------------------------------------------------
// Task 3：格式转换、派生操作
// ---------------------------------------------------------------------------

void ImageCase::copyIsUnconditionalDeepCopy()
{
    // 探针第 7 组：即使原本没有共享者，copy() 也必须强制产生新分配。
    PkImage a(2, 2, PkImage::Format_ARGB32);
    a.fill(0xFF112233u);
    PK_COMPARE(a.PkUseCount(), 1L); // 未共享

    PkImage b = a.copy();
    PK_VERIFY(!a.PkIsSharedWith(b)); // 仍然强制深拷贝
    PK_COMPARE(b.pixel(0, 0), 0xFF112233u); // 内容相同

    // 也验证"本来就共享"的常规场景：copy() 之后同样不再共享。
    PkImage c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    PkImage d = c.copy();
    PK_VERIFY(!c.PkIsSharedWith(d));
    PK_COMPARE(d.pixel(0, 0), 0xFF112233u);

    // null 的 copy() 结果仍是 null。
    PkImage n;
    PkImage nCopy = n.copy();
    PK_VERIFY(nCopy.isNull());
}

void ImageCase::convertToFormatSameFormatShares()
{
    // 探针第 6 组：目标格式与源格式相同时共享，不拷贝。
    PkImage src(2, 2, PkImage::Format_ARGB32);
    PkImage same = src.convertToFormat(PkImage::Format_ARGB32);
    PK_VERIFY(src.PkIsSharedWith(same));
}

void ImageCase::convertToFormatCrossFormatRoundtrip()
{
    // ARGB32 -> Grayscale8：复用 Task 2 的 rawPixelArgb/writeRawPixelArgb，走
    // 同一份 qGray 公式（(r*11+g*16+b*5)/32）。纯红 qGray(255,0,0)=87=0x57。
    PkImage src(1, 1, PkImage::Format_ARGB32);
    src.setPixel(0, 0, 0xFFFF0000u);
    PkImage gray = src.convertToFormat(PkImage::Format_Grayscale8);
    PK_VERIFY(!src.PkIsSharedWith(gray)); // 跨格式不共享
    PK_COMPARE(static_cast<int>(gray.format()), static_cast<int>(PkImage::Format_Grayscale8));
    PK_COMPARE(gray.pixel(0, 0), 0xFF575757u);

    // r=g=b 是灰度化公式的不动点，往返精确。
    PkImage srcMid(1, 1, PkImage::Format_ARGB32);
    srcMid.setPixel(0, 0, 0xFF808080u);
    PkImage grayMid = srcMid.convertToFormat(PkImage::Format_Grayscale8);
    PK_COMPARE(grayMid.pixel(0, 0), 0xFF808080u);

    // 再转回 ARGB32：灰度值本身应该原样透传成 R=G=B。
    PkImage backToArgb = grayMid.convertToFormat(PkImage::Format_ARGB32);
    PK_COMPARE(static_cast<int>(backToArgb.format()), static_cast<int>(PkImage::Format_ARGB32));
    PK_COMPARE(backToArgb.pixel(0, 0), 0xFF808080u);
}

void ImageCase::convertToMutatesInPlace()
{
    // convertTo() 语义：*this = convertToFormat(newFormat)。
    PkImage img(1, 1, PkImage::Format_ARGB32);
    img.setPixel(0, 0, 0xFFFF0000u);
    img.convertTo(PkImage::Format_Grayscale8);
    PK_COMPARE(static_cast<int>(img.format()), static_cast<int>(PkImage::Format_Grayscale8));
    PK_COMPARE(img.pixel(0, 0), 0xFF575757u);

    // 同格式 convertTo：结果仍与自身逻辑等价（内容不变）。
    PkImage same(2, 2, PkImage::Format_ARGB32);
    same.fill(0xFF112233u);
    same.convertTo(PkImage::Format_ARGB32);
    PK_COMPARE(same.pixel(0, 0), 0xFF112233u);
}

// ---------------------------------------------------------------------------
// Fix round 1：convertToFormat(Format, colorTable) 重载——评审后追加的真实缺口。
// libs/brush/kis_svg_brush.cpp:53 的等价简化版场景。
// ---------------------------------------------------------------------------

void ImageCase::convertToFormatWithColorTableNearestColorMatch()
{
    // 贴近真实调用点：kis_svg_brush.cpp:53 构造一个 256 项线性灰度调色板
    // （table[i] = qRgb(i,i,i)），把 ARGB32 图转成 Indexed8。调色板本身就是
    // 恒等映射（第 i 项就是灰度值 i），所以最近色匹配后的索引应精确等于源
    // 像素的灰度值本身。
    std::vector<uint32_t> table;
    table.reserve(256);
    for (int i = 0; i < 256; ++i) {
        uint32_t v = static_cast<uint32_t>(i);
        table.push_back(0xFF000000u | (v << 16) | (v << 8) | v);
    }

    PkImage src(3, 1, PkImage::Format_ARGB32);
    src.setPixel(0, 0, 0xFF101010u); // 灰度 0x10
    src.setPixel(1, 0, 0xFF808080u); // 灰度 0x80
    src.setPixel(2, 0, 0xFFF5F5F5u); // 灰度 0xF5

    PkImage indexed = src.convertToFormat(PkImage::Format_Indexed8, table);
    PK_VERIFY(!indexed.isNull());
    PK_COMPARE(static_cast<int>(indexed.format()), static_cast<int>(PkImage::Format_Indexed8));
    PK_COMPARE(indexed.pixelIndex(0, 0), 0x10);
    PK_COMPARE(indexed.pixelIndex(1, 0), 0x80);
    PK_COMPARE(indexed.pixelIndex(2, 0), 0xF5);

    // colorTable() 与传入的 palette 逐项一致（brief 明文要求：原样拷贝）。
    std::vector<uint32_t> got = indexed.colorTable();
    PK_COMPARE(got.size(), table.size());
    PK_COMPARE(got[0x10], table[0x10]);
    PK_COMPARE(got[0x80], table[0x80]);
    PK_COMPARE(got[0xF5], table[0xF5]);
}

void ImageCase::convertToFormatWithColorTableSmallPaletteNearestByRgbDistance()
{
    // 小调色板（4 项，非灰度）：验证最近色匹配确实是 R/G/B 欧氏距离比较，不是
    // 灰度值捷径。四个源像素分别扰动到黑/红/绿/蓝附近，应各自匹配到对应索引。
    std::vector<uint32_t> table{
        0xFF000000u, // 0: 黑
        0xFFFF0000u, // 1: 红
        0xFF00FF00u, // 2: 绿
        0xFF0000FFu, // 3: 蓝
    };
    PkImage src(4, 1, PkImage::Format_ARGB32);
    src.setPixel(0, 0, 0xFF080000u); // 偏黑
    src.setPixel(1, 0, 0xFFE01010u); // 偏红
    src.setPixel(2, 0, 0xFF10E010u); // 偏绿
    src.setPixel(3, 0, 0xFF1010E0u); // 偏蓝

    PkImage indexed = src.convertToFormat(PkImage::Format_Indexed8, table);
    PK_COMPARE(indexed.pixelIndex(0, 0), 0);
    PK_COMPARE(indexed.pixelIndex(1, 0), 1);
    PK_COMPARE(indexed.pixelIndex(2, 0), 2);
    PK_COMPARE(indexed.pixelIndex(3, 0), 3);

    std::vector<uint32_t> got = indexed.colorTable();
    PK_VERIFY(got == table);
}

void ImageCase::convertToFormatWithColorTableEmptyTableIsSafe()
{
    // 空调色板边界情况：没有候选项可匹配，保持零初始化索引状态，不崩溃。
    PkImage src(2, 2, PkImage::Format_ARGB32);
    src.fill(0xFF123456u);
    std::vector<uint32_t> emptyTable;
    PkImage indexed = src.convertToFormat(PkImage::Format_Indexed8, emptyTable);
    PK_VERIFY(!indexed.isNull());
    PK_COMPARE(indexed.pixelIndex(0, 0), 0);
    PK_COMPARE(indexed.pixelIndex(1, 1), 0);
    PK_VERIFY(indexed.colorTable().empty());
}

void ImageCase::operatorEqualityFourScenarios()
{
    // 探针第 5 组：深度像素内容比较，不是只比共享指针。
    PkImage a(2, 2, PkImage::Format_ARGB32);
    a.fill(0xFF112233u);
    PkImage b(2, 2, PkImage::Format_ARGB32);
    b.fill(0xFF112233u);
    PK_VERIFY(!a.PkIsSharedWith(b)); // 两个独立实例，未共享
    PK_VERIFY(a == b); // 内容相同 -> true
    PK_VERIFY(!(a != b));

    PkImage c(2, 2, PkImage::Format_ARGB32);
    c.fill(0xFF445566u);
    PK_VERIFY(!(a == c)); // 内容不同 -> false
    PK_VERIFY(a != c);

    PkImage d(3, 2, PkImage::Format_ARGB32);
    d.fill(0xFF112233u);
    PK_VERIFY(!(a == d)); // 尺寸不同 -> false

    PkImage n1;
    PkImage n2;
    PK_VERIFY(n1 == n2); // 两个 null -> true（共享指针短路：都指向哨兵）

    // 共享指针相等的短路路径也要覆盖。
    PkImage e(a);
    PK_VERIFY(a.PkIsSharedWith(e));
    PK_VERIFY(a == e);
}

void ImageCase::devicePixelRatioAccessorsAndPassthrough()
{
    PkImage img(2, 2, PkImage::Format_ARGB32);
    PK_COMPARE(img.devicePixelRatio(), qreal(1.0)); // 默认值

    img.setDevicePixelRatio(2.0);
    PK_COMPARE(img.devicePixelRatio(), qreal(2.0));

    // 结论 4：scaled()/transformed() 之后原样透传，不重置为 1.0。
    PkImage scaledImg = img.scaled(PkSize(4, 4));
    PK_COMPARE(scaledImg.devicePixelRatio(), qreal(2.0));

    PkTransform t;
    t.rotate(90);
    PkImage rotated = img.transformed(t);
    PK_COMPARE(rotated.devicePixelRatio(), qreal(2.0));
}

void ImageCase::scaledFastNearestNeighborMagnifyAndShrink()
{
    // 结论 3 探针①：3->7 宽度放大，src 索引序列 0,0,1,1,1,2,2
    // （公式 floor((dst_x+0.5)*3/7) 逐点吻合，真实探针输出）。
    PkImage src3(3, 1, PkImage::Format_ARGB32);
    const uint32_t colors3[3] = {0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu};
    for (int x = 0; x < 3; ++x) {
        src3.setPixel(x, 0, colors3[x]);
    }
    PkImage magnified = src3.scaled(PkSize(7, 1), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    PK_COMPARE(magnified.width(), 7);
    PK_COMPARE(magnified.height(), 1);
    const int expectedMagnifyIdx[7] = {0, 0, 1, 1, 1, 2, 2};
    for (int x = 0; x < 7; ++x) {
        PK_COMPARE(magnified.pixel(x, 0), colors3[expectedMagnifyIdx[x]]);
    }

    // 结论 3 探针②：7->3 缩小，src 索引序列 1,3,5
    // （公式 floor((dst_x+0.5)*7/3) 逐点吻合，真实探针输出）。
    PkImage src7(7, 1, PkImage::Format_ARGB32);
    uint32_t colors7[7];
    for (int x = 0; x < 7; ++x) {
        colors7[x] = 0xFF000000u | (static_cast<uint32_t>(x + 1) << 16);
        src7.setPixel(x, 0, colors7[x]);
    }
    PkImage shrunk = src7.scaled(PkSize(3, 1), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    PK_COMPARE(shrunk.width(), 3);
    PK_COMPARE(shrunk.height(), 1);
    const int expectedShrinkIdx[3] = {1, 3, 5};
    for (int x = 0; x < 3; ++x) {
        PK_COMPARE(shrunk.pixel(x, 0), colors7[expectedShrinkIdx[x]]);
    }
}

void ImageCase::scaledKeepAspectRatioClampsToOne()
{
    // PkSize(5,1).scaled(PkSize(1,1), KeepAspectRatio) 的 pre-clamp 结果是
    // (1,0)（PkSize::scaled 是 R-03 VERIFIED 交付，实测核对过）——PkImage::scaled()
    // 必须把高度 clamp 到至少 1，不能构造出一个高度为 0 的图像。
    PkImage src(5, 1, PkImage::Format_ARGB32);
    src.fill(0xFF112233u);
    PkImage dst = src.scaled(PkSize(1, 1), Qt::KeepAspectRatio, Qt::FastTransformation);
    PK_COMPARE(dst.width(), 1);
    PK_COMPARE(dst.height(), 1); // clamp 到至少 1，不是 0
    PK_VERIFY(!dst.isNull());
}

void ImageCase::scaledSameSizeShares()
{
    // 探针确认：目标尺寸与源尺寸相同时直接共享，不重新分配。
    PkImage src(3, 2, PkImage::Format_ARGB32);
    src.fill(0xFF112233u);
    PkImage same = src.scaled(PkSize(3, 2), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    PK_VERIFY(src.PkIsSharedWith(same));
}

void ImageCase::transformedIdentityShares()
{
    // identity 变换（默认构造的 PkTransform）直接共享短路，跳过整个映射循环。
    PkImage src(3, 2, PkImage::Format_ARGB32);
    src.fill(0xFF112233u);
    PkImage same = src.transformed(PkTransform(), Qt::FastTransformation);
    PK_VERIFY(src.PkIsSharedWith(same));

    // null 图像上的 identity 变换同样直接共享自身（isNull() 仍为 true）。
    PkImage n;
    PkImage nSame = n.transformed(PkTransform());
    PK_VERIFY(n.PkIsSharedWith(nSame));
}

void ImageCase::transformedTranslateCancelsBoundingRectOffset()
{
    // 结论 3 探针③：纯 translate(1,0) 下，boundingRect 跟着平移，boundingRect
    // 的偏移与逆映射里的偏移相减抵消，dst 与 src 变成恒等映射——验证的是
    // boundingRect.x()/y() 那个减法方向没有搞反。
    PkImage src(3, 2, PkImage::Format_ARGB32);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            src.setPixel(x, y, 0xFF000000u | (static_cast<uint32_t>(y * 3 + x + 1) << 16));
        }
    }
    PkTransform t = PkTransform::fromTranslate(1, 0);
    PkImage dst = src.transformed(t, Qt::FastTransformation);
    PK_COMPARE(dst.width(), src.width());
    PK_COMPARE(dst.height(), src.height());
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            PK_COMPARE(dst.pixel(x, y), src.pixel(x, y));
        }
    }
}

void ImageCase::transformedRotate90ComposesToIdentity()
{
    // 不需要手算旋转后的具体像素坐标：连续应用 4 次 90° 旋转必须等于恒等
    // 变换，这是几何本身的性质，直接拿真实实现的可组合性当断言。
    PkImage src(3, 2, PkImage::Format_ARGB32);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            src.setPixel(x, y, 0xFF000000u | (static_cast<uint32_t>(y * 3 + x + 1) << 16));
        }
    }
    PkTransform t;
    t.rotate(90);

    PkImage r1 = src.transformed(t, Qt::FastTransformation);
    PK_COMPARE(r1.width(), src.height()); // 结论 2：mapRect 决定的包围盒，宽高互换
    PK_COMPARE(r1.height(), src.width());

    PkImage r2 = r1.transformed(t, Qt::FastTransformation);
    PkImage r3 = r2.transformed(t, Qt::FastTransformation);
    PkImage r4 = r3.transformed(t, Qt::FastTransformation);

    PK_COMPARE(r4.width(), src.width());
    PK_COMPARE(r4.height(), src.height());
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            PK_COMPARE(r4.pixel(x, y), src.pixel(x, y));
        }
    }
}

void ImageCase::transformedShearOutOfBoundsIsTransparent()
{
    // 结论 2 + 结论 3 探针④：3x3 源图 shear(0.5,0.0) 后输出尺寸 5x3（真实探针
    // 核对过的具体数值，与独立调用 mapRect(PkRect(0,0,3,3)) 算出的 x=0 y=0 w=5
    // h=3 完全一致）；越界区域（顶行右侧，顶行 shear 位移为 0，只有源列
    // 0..2 有效）输出 0x00000000（全透明黑）。
    PkImage src(3, 3, PkImage::Format_ARGB32);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            src.setPixel(x, y, 0xFF000000u | (static_cast<uint32_t>(y * 3 + x + 1) << 16));
        }
    }
    PkTransform t;
    t.shear(0.5, 0.0);
    PkImage dst = src.transformed(t, Qt::FastTransformation);
    PK_COMPARE(dst.width(), 5);
    PK_COMPARE(dst.height(), 3);

    PK_COMPARE(dst.pixel(4, 0), 0x00000000u); // 顶行右侧越界：全透明黑
    PK_COMPARE(dst.pixel(0, 0), src.pixel(0, 0)); // 顶行左侧不受剪切影响，恒等映射
}

void ImageCase::transformedSmoothBilinearBlendsNeighbors()
{
    // 岔路 B：Smooth 模式是已声明偏离，不追求与 Qt 位对齐，只要求良定义——
    // 断言的是本实现内部一致的双线性混合结果（黑->白渐变，四个分量各自线性
    // 插值），不是跟真 Qt 对拍。
    PkImage src(2, 1, PkImage::Format_ARGB32);
    src.setPixel(0, 0, 0xFF000000u); // 黑
    src.setPixel(1, 0, 0xFFFFFFFFu); // 白
    PkTransform t = PkTransform::fromScale(2.0, 1.0);
    PkImage dst = src.transformed(t, Qt::SmoothTransformation);
    PK_COMPARE(dst.width(), 4);
    PK_COMPARE(dst.height(), 1);
    PK_COMPARE(dst.pixel(0, 0), 0xFF000000u);
    PK_COMPARE(dst.pixel(1, 0), 0xFF404040u); // 25% 权重混入白色
    PK_COMPARE(dst.pixel(2, 0), 0xFFBFBFBFu); // 75% 权重混入白色
    PK_COMPARE(dst.pixel(3, 0), 0xFFFFFFFFu); // clamp-to-edge：右边界重复采样白色
}

void ImageCase::transformedSmoothIndexedFallsBackToNearest()
{
    // Smooth 模式对索引格式退化成最近邻：混合调色板索引没有良定义的颜色语义。
    PkImage src(2, 1, PkImage::Format_Indexed8);
    src.setColorCount(2);
    src.setColor(0, 0xFF000000u);
    src.setColor(1, 0xFFFFFFFFu);
    src.setPixel(0, 0, 0);
    src.setPixel(1, 0, 1);

    PkTransform t = PkTransform::fromScale(2.0, 1.0);
    PkImage dst = src.transformed(t, Qt::SmoothTransformation);
    PK_COMPARE(dst.width(), 4);
    const int expectedIdx[4] = {0, 0, 1, 1}; // 同 Fast 模式的最近邻索引序列
    for (int x = 0; x < 4; ++x) {
        PK_COMPARE(dst.pixelIndex(x, 0), expectedIdx[x]);
    }
}

PK_TEST_MAIN(ImageCase)
