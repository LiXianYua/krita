/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __SCHEDULER_UTILS_H
#define __SCHEDULER_UTILS_H

// 内核 API 已迁 Pk（KisBaseRectsWalker::requestedRect() 返 PkRect、各测试替身的
// debugName/debugId 返 PkString），测试侧的类型跟着走。
// 本头是 qt 桶（消费方都用 QtTest 的 QCOMPARE/QTest::qSleep），所以这里补的是
// **真 Qt 头**（IMPACT §1 的桶纪律），不碰 pk/*/compat。
#include <PkRect.h>
#include <PkStringList.h>
#include "kis_merge_walker.h"
#include "kis_stroke_strategy.h"
#include "kis_stroke_job.h"
#include "kis_spontaneous_job.h"
#include "kis_stroke.h"
#include "kis_image.h"


// dbgKrita 在 qt 桶解析成真 QDebug，所以 PkRect 需要一个打印器才能落进日志分支。
// 形制照抄 libs/canvas/tests/kis_coordinates_converter_test.cpp:46 的
// `QDebug operator<<(QDebug, const PkRect&)`；那边是单 TU，这里是共享头，故加 inline。
inline QDebug operator<<(QDebug debug, const PkRect &rect)
{
    return debug.nospace() << "PkRect(" << rect.x() << ", " << rect.y() << ", "
                           << rect.width() << ", " << rect.height() << ")";
}

// 内核返回 PkString，而 SCOMPARE/COMPARE_NAME 这两条宏的实参既可能是 PkString
// （getJobName 的返回值）、QString（预先拼好的字符串），也可能是 const char* 字面量。
// 三个重载各收一路，避免 const char* 落到两个用户定义转换上产生歧义。
// PkString 分支形制照抄 libs/image/tests/kis_file_layer_test.cpp:64 的 toQString。
inline QString pkSchedulerQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), int(utf8.size()));
}

inline QString pkSchedulerQString(const char *value)
{
    return QString::fromUtf8(value);
}

inline QString pkSchedulerQString(const QString &value)
{
    return value;
}

// PkStringList 版的同一件事：kis_strokes_queue_test 的 checkExecutedJobs() 拿
// QStringList 当期望值，而 globalExecutedDabs 已迁 PkStringList，逐元素过一遍
// 才能落进 QCOMPARE。逐元素转换不丢元素、不换顺序，比较强度与原 QStringList
// 对 QStringList 时相同。
inline QStringList pkSchedulerQStringList(const PkStringList &values)
{
    QStringList result;
    result.reserve(int(values.size()));
    for (int i = 0; i < int(values.size()); i++) {
        result << pkSchedulerQString(values[i]);
    }
    return result;
}

// KisTestingStrokeStrategy 的前缀形参是 QLatin1String（内核 KisStrokeStrategy 的
// template 构造就收这个类型），而 PkString 没有吃 QLatin1String 的构造 ——
// 这里按「Latin-1 字节 → BMP 码点 1:1」转（PkString.h:146 的 fromLatin1 口径）。
inline PkString pkSchedulerPkString(const QLatin1String &value)
{
    return PkString::fromLatin1(value.data(), int(value.size()));
}

#define SCOMPARE(s1, s2)                                        \
    QCOMPARE(pkSchedulerQString(s1), pkSchedulerQString(s2))

#define COMPARE_WALKER(item, walker)            \
    QCOMPARE(item->walker(), walker)
#define COMPARE_NAME(item, name)                                        \
    QCOMPARE(pkSchedulerQString(getJobName(item->strokeJob())),         \
             pkSchedulerQString(name))
#define VERIFY_EMPTY(item)                                      \
    QVERIFY(!item->isRunning())

void executeStrokeJobs(KisStroke *stroke) {
    KisStrokeJob *job;

    while((job = stroke->popOneJob())) {
        job->run();
        delete job;
    }
}

bool checkWalker(KisBaseRectsWalkerSP walker, const PkRect &rect, int lod = 0) {
    if(walker->requestedRect() == rect && walker->levelOfDetail() == lod) {
        return true;
    }
    else {
        dbgKrita << "walker rect:" << walker->requestedRect();
        dbgKrita << "expected rect:" << rect;
        dbgKrita << "walker lod:" << walker->levelOfDetail();
        dbgKrita << "expected lod:" << lod;
        return false;
    }
}

class KisNoopSpontaneousJob : public KisSpontaneousJob
{
public:
    KisNoopSpontaneousJob(bool overridesEverything = false, int lod = 0)
        : m_overridesEverything(overridesEverything),
          m_lod(lod)
    {
    }

    void run() override {
    }

    bool overrides(const KisSpontaneousJob *otherJob) override {
        Q_UNUSED(otherJob);
        return m_overridesEverything;
    }

    int levelOfDetail() const override {
        return m_lod;
    }

    PkString debugName() const override {
        return "KisNoopSpontaneousJob";
    }

private:
    bool m_overridesEverything;
    int m_lod;
};

static PkStringList globalExecutedDabs;

class KisNoopDabStrategy : public KisStrokeJobStrategy
{
public:
    KisNoopDabStrategy(PkString name)
    : m_name(name),
      m_isMarked(false)
    {}

    void run(KisStrokeJobData *data) override {
        Q_UNUSED(data);

        globalExecutedDabs << m_name;
    }

    virtual PkString name(KisStrokeJobData *data) const {
        Q_UNUSED(data);
        return m_name;
    }

    void setMarked() {
        m_isMarked = true;
    }

    bool isMarked() const {
        return m_isMarked;
    }

    PkString debugId() const override {
        return "KisNoopDabStrategy";
    }

private:
    PkString m_name;
    bool m_isMarked;
};

class KisTestingStrokeJobData : public KisStrokeJobData
{
public:
    KisTestingStrokeJobData(Sequentiality sequentiality = SEQUENTIAL,
                            Exclusivity exclusivity = NORMAL,
                            bool addMutatedJobs = false,
                            const PkString &customSuffix = PkString())
        : KisStrokeJobData(sequentiality, exclusivity),
          m_addMutatedJobs(addMutatedJobs),
          m_customSuffix(customSuffix)
    {
    }

    KisTestingStrokeJobData(const KisTestingStrokeJobData &rhs)
        : KisStrokeJobData(rhs),
          m_addMutatedJobs(rhs.m_addMutatedJobs)
    {
    }

    KisStrokeJobData* createLodClone(int levelOfDetail) override {
        Q_UNUSED(levelOfDetail);
        return new KisTestingStrokeJobData(*this);
    }

    bool m_addMutatedJobs = false;
    bool m_isMutated = false;
    PkString m_customSuffix;
};

class KisMutatableDabStrategy : public KisNoopDabStrategy
{
public:
    KisMutatableDabStrategy(const PkString &name, KisStrokeStrategy *parentStrokeStrategy)
        : KisNoopDabStrategy(name),
          m_parentStrokeStrategy(parentStrokeStrategy)
    {
    }

    void run(KisStrokeJobData *data) override {
        KisTestingStrokeJobData *td = dynamic_cast<KisTestingStrokeJobData*>(data);

        if (td && td->m_isMutated) {
            globalExecutedDabs << PkString("%1_mutated").arg(name(data));
        } else if (td && td->m_addMutatedJobs) {
            globalExecutedDabs << name(data);

            for (int i = 0; i < 3; i++) {
                KisTestingStrokeJobData *newData =
                    new KisTestingStrokeJobData(td->sequentiality(), td->exclusivity(), false);
                newData->m_isMutated = true;
                m_parentStrokeStrategy->addMutatedJob(newData);
            }
        } else {
            globalExecutedDabs << name(data);
        }
    }

    virtual PkString name(KisStrokeJobData *data) const override {
        const PkString baseName = KisNoopDabStrategy::name(data);

        KisTestingStrokeJobData *td = dynamic_cast<KisTestingStrokeJobData*>(data);
        return !td || td->m_customSuffix.isEmpty() ? baseName : PkString("%1_%2").arg(baseName).arg(td->m_customSuffix);
    }

    PkString debugId() const override {
        return "KisMutatableDabStrategy";
    }

private:
    KisStrokeStrategy *m_parentStrokeStrategy = 0;
};


class KisTestingStrokeStrategy : public KisStrokeStrategy
{
public:
    KisTestingStrokeStrategy(const QLatin1String &prefix = QLatin1String(),
                             bool exclusive = false,
                             bool inhibitServiceJobs = false,
                             bool forceAllowInitJob = false,
                             bool forceAllowCancelJob = false,
                             bool isLegacyStroke = false)
        : KisStrokeStrategy(prefix, kundo2_text_raw(pkSchedulerPkString(prefix))),
          m_prefix(pkSchedulerPkString(prefix)),
          m_inhibitServiceJobs(inhibitServiceJobs),
          m_forceAllowInitJob(forceAllowInitJob),
          m_forceAllowCancelJob(forceAllowCancelJob),
          m_cancelSeqNo(0),
          m_isLegacyStroke(isLegacyStroke)
    {
        setExclusive(exclusive);
    }

    KisTestingStrokeStrategy(const KisTestingStrokeStrategy &rhs, int levelOfDetail)
        : KisStrokeStrategy(rhs),
          m_prefix(rhs.m_prefix),
          m_inhibitServiceJobs(rhs.m_inhibitServiceJobs),
          m_forceAllowInitJob(rhs.m_forceAllowInitJob),
          m_forceAllowCancelJob(rhs.m_forceAllowCancelJob),
          m_cancelSeqNo(rhs.m_cancelSeqNo),
          m_isLegacyStroke(rhs.m_isLegacyStroke)
    {
        m_prefix = PkString("clone%1_%2").arg(levelOfDetail).arg(m_prefix);
    }

    KisStrokeJobStrategy* createInitStrategy() override {
        return m_forceAllowInitJob || !m_inhibitServiceJobs ?
            new KisNoopDabStrategy(m_prefix + "init") : 0;
    }

    KisStrokeJobStrategy* createFinishStrategy() override {
        return !m_inhibitServiceJobs ?
            new KisNoopDabStrategy(m_prefix + "finish") : 0;
    }

    KisStrokeJobStrategy* createCancelStrategy() override {
        return m_forceAllowCancelJob || !m_inhibitServiceJobs ?
            new KisNoopDabStrategy(m_prefix + "cancel") : 0;
    }

    KisStrokeJobStrategy* createDabStrategy() override {
        return new KisMutatableDabStrategy(m_prefix + "dab", this);
    }

    KisStrokeStrategy* createLodClone(int levelOfDetail) override {
        return !m_isLegacyStroke ? new KisTestingStrokeStrategy(*this, levelOfDetail) : 0;
    }

    class CancelData : public KisStrokeJobData
    {
    public:
        CancelData(int seqNo) : m_seqNo(seqNo) {}
        int seqNo() const { return m_seqNo; }

    private:
        int m_seqNo;
    };

    KisStrokeJobData* createCancelData() override {
        return new CancelData(m_cancelSeqNo++);
    }

    void setLegacyStroke(bool value) {
        m_isLegacyStroke = value;
    }

protected:
    PkString m_prefix;
    bool m_inhibitServiceJobs;
    int m_forceAllowInitJob;
    bool m_forceAllowCancelJob;
    int m_cancelSeqNo;
    bool m_isLegacyStroke = false;
};

inline PkString getJobName(KisStrokeJob *job) {
    KisNoopDabStrategy *pointer =
        dynamic_cast<KisNoopDabStrategy*>(job->testingGetDabStrategy());
    KIS_ASSERT(pointer);

    return pointer->name(job->testingGetDabData());
}

inline int cancelSeqNo(KisStrokeJob *job) {
    KisTestingStrokeStrategy::CancelData *pointer =
        dynamic_cast<KisTestingStrokeStrategy::CancelData*>
        (job->testingGetDabData());
    Q_ASSERT(pointer);

    return pointer->seqNo();
}

#endif /* __SCHEDULER_UTILS_H */
