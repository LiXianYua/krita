/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TILES_TEST_UTILS_H
#define TILES_TEST_UTILS_H

// 存储面已迁 Pk：KoStore 的虚函数形参是 PkString、其 write() 收 PkByteArray、
// KoStorePrivate::stream 是 PkStream*（libs/store/KoStore.h:240-275、
// libs/store/KoStore_p.h），KisPaintDeviceWriter 的 write() 也收 PkByteArray
// （libs/image/kis_paint_device_writer.h:16）。测试侧的类型跟着走。
// 本头被 4 个 qt 桶 target 消费（kis_tiled_data_manager_test / kis_low_memory_tests /
// kis_swapped_data_store_test / kis_tile_data_store_test，都带 -DQT_CORE_LIB、
// 无 pk/*/compat），所以这里补的是真 Pk 头（IMPACT §1 的桶纪律：不把
// pk/*/compat 拉进 qt 桶 TU 的 include 面）。
#include <KoStore_p.h>
#include <PkMemoryStream.h>
#include <kis_paint_device_writer.h>
#include <kis_debug.h>

class KisFakePaintDeviceWriter : public KisPaintDeviceWriter {
public:
    KisFakePaintDeviceWriter(KoStore *store)
        : m_store(store)
    {
    }

    // 基类形参是 const PkByteArray&（kis_paint_device_writer.h:16）；QByteArray 版
    // 不再 override 任何虚函数，故此处必须跟着换类型。PkByteArray 的对应 getter 是
    // size()（pk/container/PkByteArray.h:38；本类没有 Qt 的 length()）。
    bool write(const PkByteArray &data) override {
        return (m_store->write(data) == data.size());
    }

    // 这一路形参没变（KoStore::write 的第二重载仍是 (const char*, pk_int64)，
    // KoStore.h:121），保持原样。
    bool write(const char* data, qint64 length) override {
        return (m_store->write(data, length) == length);
    }

    KoStore *m_store;
};


class KoStoreFake : public KoStore
{
public:
    KoStoreFake() : KoStore(KoStore::Write) {
        // KoStorePrivate::stream 的类型是 PkStream*（KoStore_p.h），QBuffer 不再能赋进去。
        // 内存缓冲的对应物是 PkMemoryStream（libs/store/PkMemoryStream.h），
        // 开读写对应 QIODevice::ReadWrite → PkStream::ReadWrite（pk/port/PkStream.h:54）。
        d_ptr->stream = &m_buffer;
        d_ptr->isOpen = true;
        m_buffer.open(PkStream::ReadWrite);
    }
    ~KoStoreFake() override {
        // Oh, no, please do not clean anything! :)
        d_ptr->stream = 0;
        d_ptr->isOpen = false;
    }

    void startReading() {
        m_buffer.seek(0);
        d_ptr->mode = KoStore::Read;
    }

    // KoStore 的这 7 个纯虚函数形参已迁 PkString（KoStore.h:240-275）。
    bool openWrite(const PkString&) override { return true; }
    bool openRead(const PkString&) override { return true; }
    bool closeRead() override { return true; }
    bool closeWrite() override { return true; }
    bool enterRelativeDirectory(const PkString&) override { return true; }
    bool enterAbsoluteDirectory(const PkString&) override { return true; }
    bool fileExists(const PkString&) const override { return true; }
private:
    PkMemoryStream m_buffer;
};

bool memoryIsFilled(quint8 c, quint8 *mem, qint32 size)
{
    for(; size > 0; size--)
        if(*(mem++) != c) {
            dbgKrita << "Expected" << c << "but found" << *(mem-1);
            return false;
        }

    return true;
}

#define TILESIZE 64*64


#endif /* TILES_TEST_UTILS_H */
