/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "PkMessageLogger.h"
#include "compat/QDebug"


namespace TestUtil {

template <typename T>
struct TagWrapper
{
    static constexpr bool shouldPrintTag = true;
    TagWrapper(T t) : tag(t) {}

    T tag;
};

template <typename T>
QDebug operator<<(QDebug dbg, const TagWrapper<T> &t)
{
    dbg.nospace() << t.tag;
    return dbg.space();
}

namespace detail {
    struct notag {};
}

template <>
struct TagWrapper<detail::notag>
{
    TagWrapper(detail::notag) {}
    static constexpr bool shouldPrintTag = false;
};

template <typename TagType = detail::notag>
class MeasureAvgPortion
{
public:
    MeasureAvgPortion(int period, TagType tag = {}) : m_period(period), m_val(0), m_total(0), m_cycles(0), m_tagWrapper(tag) { }

    ~MeasureAvgPortion() { printValues(true); }

    void addVal(int x)
    {
        m_val += x;
        m_valMax = std::max(m_valMax, int64_t(x));
        m_valMin = std::min(m_valMin, int64_t(x));
    }

    void addTotal(int x)
    {
        m_total += x;
        m_totalMax = std::max(m_totalMax, int64_t(x));
        m_totalMin = std::min(m_totalMin, int64_t(x));
        m_cycles++;
        printValues();
    }

private:
    void printValues(bool force = false)
    {
        if (m_cycles > m_period || force) {
            if constexpr (TagWrapper<TagType>::shouldPrintTag) {
                // auto surface = reinterpret_cast<QPlatformSurface*>(m_tagWrapper.tag);
                // auto window = dynamic_cast<QPlatformWindow*>(surface);
                // if (window) {
                //     qDebug() << "=== stat for tag" << m_tag << window << "===";
                // } else {
                //     qDebug() << "=== stat for tag" << m_tag << "===";
                // }
                qDebug() << "=== stat for tag" << m_tagWrapper << "===";
            } else {
                qDebug() << "=== stat ===";
            }

            qDebug() << "Val / Total:" << double(m_val) / double(m_total);
            qDebug() << "Avg. Val:   " << double(m_val) / m_cycles
                     << "min:" << m_valMin << "max:" << m_valMax;
            qDebug() << "Avg. Total: " << double(m_total) / m_cycles
                     << "min:" << m_totalMin << "max:" << m_totalMax;
            qDebug().nospace() << "  (val: " << m_val << ", total: " << m_total
                               << ", cycles:" << m_cycles << ")";

            m_val = 0;
            m_total = 0;
            m_cycles = 0;
            m_totalMin = std::numeric_limits<int64_t>::max();
            m_totalMax = std::numeric_limits<int64_t>::min();
            m_valMin = std::numeric_limits<int64_t>::max();
            m_valMax = std::numeric_limits<int64_t>::min();
        }
    }

private:
    int m_period;
    int64_t m_val;
    int64_t m_valMax = std::numeric_limits<int64_t>::min();
    int64_t m_valMin = std::numeric_limits<int64_t>::max();
    int64_t m_total;
    int64_t m_totalMax = std::numeric_limits<int64_t>::min();
    int64_t m_totalMin = std::numeric_limits<int64_t>::max();
    int64_t m_cycles;
    TagWrapper<TagType> m_tagWrapper;
};

template <typename TagType>
struct PerObjectMetric
{
    struct DelayMeasure {
        MeasureAvgPortion<TagType> portion;
        std::chrono::steady_clock::time_point start;
        bool started = false;
    };

    auto getIterator(TagType tag) {
        auto it = hash.find(tag);
        if (it != hash.end()) {
            return it;
        } else {
            bool unused;
            std::tie(it, unused) = hash.emplace(tag, DelayMeasure{ { 60, tag }, {}, false });
            return it;
        }
    }

    void startFrame(TagType tag) {
        auto it = getIterator(tag);
        DelayMeasure &delay = it->second;

        const auto now = std::chrono::steady_clock::now();
        if (delay.started) {
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - delay.start).count();
            delay.portion.addTotal(static_cast<int>(elapsedMs));
        }
        delay.start = now;
        delay.started = true;
    }

    void endFrame(TagType tag) {
        auto it = getIterator(tag);
        DelayMeasure &delay = it->second;

        assert(delay.started);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - delay.start).count();
        delay.portion.addVal(static_cast<int>(elapsedMs));
    }

    using MapType = std::unordered_map<TagType, DelayMeasure>;

    MapType hash;
};

}

/// Usage:
//
// QDebug operator<<(QDebug dbg, const TestUtil::TagWrapper<QPlatformSurface *> &t)
// {
//     if (qApp) {
//         auto surface = reinterpret_cast<QPlatformSurface*>(t.tag);
//         auto window = dynamic_cast<QPlatformWindow*>(surface);
//         dbg.nospace() << window;
//     } else {
//         dbg.nospace() << "<deleted>";
//     }
//
//     return dbg.space();
// }
//
// static TestUtil::PerObjectMetric<QPlatformSurface*> SwapCounter;
