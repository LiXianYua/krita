/*------------------------------------------------------------------------
  Junction: Concurrent data structures in C++
  Copyright (c) 2016 Jeff Preshing
  Distributed under the Simplified BSD License.
  Original location: https://github.com/preshing/junction
  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the LICENSE file for more information.
------------------------------------------------------------------------*/

#ifndef MAPTRAITS_H
#define MAPTRAITS_H

#include <cstdint>

inline unsigned long long roundUpPowerOf2(unsigned long long v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

inline bool isPowerOf2(unsigned long long v)
{
    return (v & (v - 1)) == 0;
}

inline unsigned int avalanche(unsigned int h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline unsigned int deavalanche(unsigned int h)
{
    h ^= h >> 16;
    h *= 0x7ed1b41d;
    h ^= (h ^ (h >> 13)) >> 13;
    h *= 0xa5cb9243;
    h ^= h >> 16;
    return h;
}

template <class T>
struct DefaultKeyTraits {
    typedef T Key;
    typedef unsigned int Hash;
    static const Key NullKey = Key(0);
    static const Hash NullHash = Hash(0);

    static Hash hash(T key)
    {
        return avalanche(Hash(key));
    }

    static Key dehash(Hash hash)
    {
        return (T) deavalanche(hash);
    }
};

template <class T>
struct DefaultValueTraits {
    typedef T Value;
    typedef std::intptr_t IntType;
    static const IntType NullValue = 0;
    static const IntType Redirect = 1;
};

#endif // MAPTRAITS_H
