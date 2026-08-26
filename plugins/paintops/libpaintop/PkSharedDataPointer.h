/*
 *  SPDX-FileCopyrightText: 2026 paint_app migration
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PKSHAREDDATAPOINTER_H
#define PKSHAREDDATAPOINTER_H

// 零 Qt 的共享数据指针（shared-data pointer）剥离替代（S-07-b Group A）。
// pk/pointer 无对应物（R 线缺口登记），本 shim 放 libpaintop 内。
//
// COW via clone：存储 std::shared_ptr<T>；非 const operator-> / data() 在
// use_count()>1 时先 detach（reset(d->clone())）；const 路径（constData()、
// const operator->、operator*）直返，不分裂。
//
// 特化形态照原 shared-data pointer：primary 模板只声明 `T *clone()`（无定义），
// 只在 `PkSharedDataPointer<KisSensorPackInterface>::clone()` 的显式特化里定义
// （见 KisSensorPackInterface.h）。其它类型若 odr-use clone() 会链接错，但壳闭包
// 内没有。
#include <memory>

template <typename T>
class PkSharedDataPointer
{
public:
    PkSharedDataPointer() noexcept = default;
    explicit PkSharedDataPointer(T *data) noexcept : d(data) {}
    PkSharedDataPointer(const PkSharedDataPointer &) = default;
    PkSharedDataPointer(PkSharedDataPointer &&) noexcept = default;
    PkSharedDataPointer &operator=(const PkSharedDataPointer &) = default;
    PkSharedDataPointer &operator=(PkSharedDataPointer &&) noexcept = default;
    ~PkSharedDataPointer() = default;

    T &operator*() const { return *d; }
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

    bool operator==(const PkSharedDataPointer &o) const { return d == o.d; }
    bool operator!=(const PkSharedDataPointer &o) const { return d != o.d; }

    // 只在显式特化里定义（KisSensorPackInterface）
    T *clone();

private:
    std::shared_ptr<T> d;
};

#endif // PKSHAREDDATAPOINTER_H
