/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PkThreadStorage<T> —— 每线程存储的零 Qt 垫片（S 线剥 Qt 用）。
    每线程一份存储；线程退出自动清理（thread_local + unique_ptr 天然在 TLS
    销毁时释放）。

    消费方三种形态：
      · KoColorConversionCache.cpp:70  每线程一个 FastPathCacheItem 指针缓存
        → PkThreadStorage<FastPathCacheItem>；`localData()` 返回 T*，
        `setLocalData(T*)` 接管所有权（Qt 侧存裸指针且不清理，本垫片存
        unique_ptr、线程退出/TLS 销毁时 delete，比原状严格更安全，行为不差）。
      · KoColorSpace_p.h:37  每线程一个 Vector<quint8> 指针缓存
        → PkThreadStorage<PkVector<quint8>>；同上指针形态，配合
        hasLocalData()/setLocalData() 使用。
      · compositeops/KoCompositeOpDissolve.h:24  KisRandomSource（值类型）：
        `hasLocalData()` 判空、`setLocalData(T 值)` 拷贝、取用时
        `localDataRef()` 返回 T&（Qt 侧对值类型取用返回引用；剥离时该处
        `localData().generate(...)` 改 `localDataRef().generate(...)`）。

    与 brief 模板的一处必要偏离：brief 写 `static thread_local ThreadLocal
    t_data;`，但 KoColorSpace_p.h 同一个类里有两个
    PkThreadStorage<PkVector<quint8>> 成员（conversionCache 与
    channelFlagsApplicationCache）——static 成员会让两个实例共享同一份线程
    存储，互相覆盖。故按实例地址在 thread_local map 里分槽（见 slotFor）。
    消费方三处都是长生命周期成员，无地址复用问题。
 */

#ifndef PK_THREADSTORAGE_H
#define PK_THREADSTORAGE_H

#include <memory>
#include <unordered_map>

template<class T>
class PkThreadStorage
{
public:
    PkThreadStorage() = default;
    ~PkThreadStorage() = default;
    PkThreadStorage(const PkThreadStorage &) = delete;
    PkThreadStorage &operator=(const PkThreadStorage &) = delete;

    // 返回当前线程已存储的 T 指针；未 set 过返回 nullptr（对齐 Qt 对指针形态
    // QThreadStorage<T*>::localData() 的语义：未 set 返回 null，不自动创建）。
    // Task 3 实测：自动创建在 FastPathCacheItem（= std::pair<KoColorConversion
    // CacheKey, KoCachedColorConversionTransformation>）上编不过——该 pair 的两个
    // 成员都无默认构造函数。指针消费方（KoColorConversionCache、KoColorSpace_p）
    // 都先 hasLocalData() 判空再取，语义与 Qt 一致。
    T *localData()
    {
        return slotFor(this).value.get();
    }
    T *localData() const
    {
        return const_cast<PkThreadStorage *>(this)->localData();
    }

    // 值类型消费方取引用（KoCompositeOpDissolve 的 localDataRef().generate()
    // 剥离形态）。未 set 过则自动创建默认构造的 T（对齐 Qt 对值形态
    // QThreadStorage<T>::localData() 首访自动构造的语义）。
    T &localDataRef()
    {
        auto &slot = slotFor(this);
        if (!slot.value) {
            slot.value = std::make_unique<T>();
        }
        return *slot.value.get();
    }

    bool hasLocalData() const
    {
        return static_cast<bool>(slotFor(this).value);
    }

    // 指针形态：接管所有权，线程退出/TLS 销毁时 delete。
    void setLocalData(T *data)
    {
        slotFor(this).value.reset(data);
    }

    // 值形态：拷贝赋值（对齐 Qt 对值类型存储的 setLocalData(T)）。
    void setLocalData(const T &value)
    {
        slotFor(this).value = std::make_unique<T>(value);
    }

private:
    struct ThreadLocal {
        std::unique_ptr<T> value;
    };

    static ThreadLocal &slotFor(const PkThreadStorage *self)
    {
        static thread_local std::unordered_map<const PkThreadStorage *, ThreadLocal> slots;
        return slots[self];
    }
};

#endif // PK_THREADSTORAGE_H
