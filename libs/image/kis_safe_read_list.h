/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SAFE_READ_LIST_H_
#define KIS_SAFE_READ_LIST_H_


#include <PkContainerAlgo.h>

/**
 * \class KisSafeReadList
 *
 * This is a special wrapper around PkList class
 * Q: Why is it needed?
 * A: It guarantees thread-safety of all the read requests to the list.
 *    There is absolutely *no* guarantees for write requests though.
 * Q: Why pure PkList cannot guarantee it?
 * A: First, Qt does not guarantee thread-safety for PkList at all.
 *    Second, PkList is implicitly shared structure, therefore even
 *    with read, but non-const requests (e.g. non-const PkList::first()),
 *    PkList will perform internal write operations. That will lead to
 *    a race condition in an environment with 3 and more threads.
 */
template<class T> class KisSafeReadList : private PkList<T> {
public:
    KisSafeReadList() {}

    using typename PkList<T>::const_iterator;

    /**
     * All the methods of this class are split into two groups:
     * threadsafe and non-threadsafe. The methods from the first group
     * can be called concurrently with each other. The ones form
     * the other group can't be called concurrently (even with the
     * friends from the first group) and must have an exclusive
     * access to the list.
     */

    /**
     * The thread-safe group
     */

    inline const T& first() const {
        return PkList<T>::first();
    }

    inline const T& last() const {
        return PkList<T>::last();
    }

    inline const T& at(int i) const {
        return PkList<T>::at(i);
    }

    using PkList<T>::constBegin;
    using PkList<T>::constEnd;
    using PkList<T>::isEmpty;
    using PkList<T>::size;
    using PkList<T>::indexOf;
    using PkList<T>::contains;

    /**
     * The non-thread-safe group
     */

    using PkList<T>::append;
    using PkList<T>::prepend;
    using PkList<T>::insert;
    using PkList<T>::removeAt;
    using PkList<T>::clear;

private:
    KisSafeReadList(const KisSafeReadList &) = delete;
    KisSafeReadList &operator=(const KisSafeReadList &) = delete;
};


#define FOREACH_SAFE(_iter, _container)         \
    for(_iter = _container.constBegin();        \
        _iter != _container.constEnd();         \
        _iter++)


#endif /* KIS_SAFE_READ_LIST_H_ */
