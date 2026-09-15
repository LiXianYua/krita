/*
 *  SPDX-FileCopyrightText: 2026 paint_app migration
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIBPAINTOPSHAREDDATAPOINTER_H
#define LIBPAINTOPSHAREDDATAPOINTER_H

// 零 Qt 的共享数据指针（shared-data pointer）剥离替代（S-07-b Group A）。
// pk/pointer 无对应物；权威实现在 `pk/container`（R-81 实测订正），本 shim 放 libpaintop 内。
// ⚠ 权威的那一份在 `pk/container/PkSharedDataPointer.h`（R-02 交付物，412 个 TU 在用），
//   与本垫片**同名不同物、不可互换**：那份没有 data()/constData()/detach()/clone() 通路。
//   R-81 把本文件改名为 LibPaintopSharedDataPointer.h，让那个名字全树只剩一个含义。
//
// COW via clone：存储 std::shared_ptr<T>；非 const operator-> / data() 在
// use_count()>1 时先 detach（reset(d->clone())）；const 路径（constData()、
// const operator->、const operator*）直返，不分裂；非 const operator* 也走 detach()。
//
// 特化形态照原 shared-data pointer：primary 模板只声明 `T *clone()`（无定义），
// 只在 `LibPaintopSharedDataPointer<KisSensorPackInterface>::clone()` 的显式特化里定义
// （见 KisSensorPackInterface.h）。其它类型若 odr-use clone() 会链接错，但壳闭包
// 内没有。
#include <memory>

template <typename T>
class LibPaintopSharedDataPointer
{
public:
    LibPaintopSharedDataPointer() noexcept = default;
    explicit LibPaintopSharedDataPointer(T *data) noexcept : d(data) {}
    LibPaintopSharedDataPointer(const LibPaintopSharedDataPointer &) = default;
    LibPaintopSharedDataPointer(LibPaintopSharedDataPointer &&) noexcept = default;
    LibPaintopSharedDataPointer &operator=(const LibPaintopSharedDataPointer &) = default;
    LibPaintopSharedDataPointer &operator=(LibPaintopSharedDataPointer &&) noexcept = default;
    ~LibPaintopSharedDataPointer() = default;

    T &operator*() { detach(); return *d; }
    const T &operator*() const { return *d; }
    const T *operator->() const { return d.get(); }
    T *operator->() { detach(); return d.get(); }

    const T *constData() const { return d.get(); }
    T *data() { detach(); return d.get(); }

    bool isNull() const { return !d; }
    void detach()
    {
        if (d && d.use_count() > 1) {
            d.reset(clone());
        }
    }

    bool operator==(const LibPaintopSharedDataPointer &o) const { return d == o.d; }
    bool operator!=(const LibPaintopSharedDataPointer &o) const { return d != o.d; }

    // 只在显式特化里定义（KisSensorPackInterface）
    T *clone();

private:
    std::shared_ptr<T> d;
};

#endif // LIBPAINTOPSHAREDDATAPOINTER_H
