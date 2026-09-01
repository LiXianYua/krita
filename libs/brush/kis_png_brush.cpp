/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QBuffer>
#include <QImageReader>
#include <QPainter>

#include "kis_png_brush.h"

#include <PkAuxTypes.h>
#include <PkStream.h>
#include <PkXmlElement.h>
#include <cstring>
#include <filesystem>

#include <kis_dom_utils.h>
#include "KisBrushStreamUtils.h"

// ── 图像编解码 GAP（登记，关闭条件 R-15/S-03-e libpng 通道）──────────────────
// PNG 解码走 Qt 通道：QImageReader 只能消费 QIODevice，PkMemoryStream 不是
// QIODevice，这里把 dev->readAll() 的 PkByteArray 拷进 QByteArray 再包 QBuffer
// 喂给 QImageReader（原 Qt 形态）。QImage 也随 QImageReader 保留为过渡，只在
// setBrushTipImage 边界经 toPkImage() 转成 PkImage。
static PkString toPkString(const QString &s)
{
    const QByteArray bytes = s.toUtf8();
    return PkString::PkFromUtf8(bytes.constData(), bytes.size());
}

// QImage→PkImage 桥。主树：QImage 与 PkImage 的 Format 枚举逐值同序
// （PkImage 是 QImage 的忠实移植），逐行按源 bytesPerLine 拷贝（两者行对齐规则
// 相同：((width*depth+31)/32)*4）；索引格式（Mono/MonoLSB/Indexed8）连颜色表一起拷。
// 壳内若 QImage 经 compat 即 PkImage，本函数退化为自拷贝，identity 语义，行为不变。
static PkImage toPkImage(const QImage &img)
{
    PkImage out(img.width(), img.height(), static_cast<PkImage::Format>(img.format()));
    if (img.colorCount() > 0) {
        std::vector<uint32_t> colors;
        colors.reserve(static_cast<size_t>(img.colorCount()));
        for (int i = 0; i < img.colorCount(); ++i) {
            colors.push_back(img.color(i));
        }
        out.setColorTable(colors);
    }
    for (int y = 0; y < img.height(); ++y) {
        memcpy(out.scanLine(y), img.constScanLine(y), img.bytesPerLine());
    }
    return out;
}

// QFileInfo(p).completeBaseName() 等价：文件名去掉最后一个扩展名。
// 复刻 S-02-b PkResourceStorageDesktop 的 std::filesystem 处理模式
// （Task 1 修复轮 pathFileName 同款）。
static PkString pathCompleteBaseName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).stem().string();
    return PkString::PkFromUtf8(name.c_str(), static_cast<int>(name.size()));
}

KisPngBrush::KisPngBrush(const PkString& filename)
    : KisColorfulBrush(filename)
{
    setBrushType(INVALID);
    setSpacing(0.25);
}

KisPngBrush::KisPngBrush(const KisPngBrush &rhs)
    : KisColorfulBrush(rhs)
{
}

KoResourceSP KisPngBrush::clone() const
{
    return KoResourceSP(new KisPngBrush(*this));
}

bool KisPngBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);

    // Workaround for some OS (Debian, Ubuntu), where loading directly from the PkStream
    // fails with "libpng error: IDAT: CRC error"
    PkByteArray data = kisBrushReadAll(dev);
    QByteArray bytes(data.data(), data.size());
    QBuffer buf(&bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, "PNG");

    if (!reader.canRead()) {
        dbgKrita << "Could not read brush" << QString::fromUtf8(filename().PkToUtf8().c_str()) << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    if (reader.textKeys().contains("brush_spacing")) {
        setSpacing(KisDomUtils::toDouble(toPkString(reader.text("brush_spacing"))));
    }

    if (reader.textKeys().contains("brush_name")) {
        setName(toPkString(reader.text("brush_name")));
    }
    else {
        setName(pathCompleteBaseName(filename()));
    }

    QImage image = reader.read();

    if (image.isNull()) {
        dbgKrita << "Could not create image for" << QString::fromUtf8(filename().PkToUtf8().c_str()) << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    setValid(true);

    bool hasAlpha = false;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if (qAlpha(image.pixel(x, y)) != 255) {
                hasAlpha = true;
                break;
            }
        }
    }

    const bool isAllGray = image.allGray();

    if (isAllGray && !hasAlpha) {
        // Make sure brush tips all have a white background
        // NOTE: drawing it over white background can probably be skipped now...
        //       Any images with an Alpha channel should be loaded as RGBA so
        //       they can have the lightness and gradient options available
        QImage base(image.size(), image.format());
        if ((int)base.format() < (int)QImage::Format_RGB32) {
            base.convertTo(QImage::Format_ARGB32);
        }
        QPainter gc(&base);
        gc.fillRect(base.rect(), Qt::white);
        gc.drawImage(0, 0, image);
        gc.end();
        QImage converted = base.convertToFormat(QImage::Format_Grayscale8);
        setBrushTipImage(toPkImage(converted));
        setBrushType(MASK);
        setBrushApplication(ALPHAMASK);
        setHasColorAndTransparency(false);
    }
    else {
        // see bug https://bugs.kde.org/show_bug.cgi?id=484115 if you want to edit this condition
        // keep it in sync with KisColorfulBrush code
        if ((int)image.format() != (int)QImage::Format_ARGB32) {
            image.convertTo(QImage::Format_ARGB32);
        }

        setBrushTipImage(toPkImage(image));
        setBrushType(IMAGE);
        setBrushApplication(isAllGray ? ALPHAMASK : LIGHTNESSMAP);
        setHasColorAndTransparency(!isAllGray);
    }


    setWidth(brushTipImage().width());
    setHeight(brushTipImage().height());

    return valid();
}

bool KisPngBrush::saveToDevice(PkStream *dev) const
{
    Q_UNUSED(dev);
    // GAP: PkImage::save 未交付（R-15/S-03-e libpng 通道）。PNG 编码待图像编解码任务。
    return false;
}

PkString KisPngBrush::defaultFileExtension() const
{
    return PkString(".png");
}

void KisPngBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    predefinedBrushToXML("png_brush", e);
    KisColorfulBrush::toXML(d, e);
}
