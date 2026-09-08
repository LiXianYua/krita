/* SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "PkArrayData.h"
#include <memory>

// Private-data pointer facade over the single project-wide COW substrate.
// The holder supports forward-declared payloads; only actual construction or
// mutable access instantiates the payload's constructor/copy constructor.
template<class T>
class PkSharedDataPointer
{
    struct Holder {
        std::unique_ptr<T> pointer;
        Holder() = default;
        explicit Holder(T *value) : pointer(value) {}
        Holder(const Holder &other)
            : pointer(other.pointer ? new T(*other.pointer) : nullptr) {}
        Holder(Holder &&) = default;
    };
    PkArrayData<Holder> m_data;
public:
    PkSharedDataPointer() = default;
    explicit PkSharedDataPointer(T *value) : m_data(Holder(value)) {}
    PkSharedDataPointer(const PkSharedDataPointer &) = default;
    PkSharedDataPointer &operator=(const PkSharedDataPointer &) = default;
    const T *operator->() const { return m_data.PkConst().pointer.get(); }
    T *operator->() { return m_data.PkMut().pointer.get(); }
    const T &operator*() const { return *m_data.PkConst().pointer; }
    T &operator*() { return *m_data.PkMut().pointer; }
};
