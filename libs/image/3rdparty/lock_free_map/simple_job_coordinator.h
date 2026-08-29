/*------------------------------------------------------------------------
  Junction: Concurrent data structures in C++
  Copyright (c) 2016 Jeff Preshing
  Distributed under the Simplified BSD License.
  Original location: https://github.com/preshing/junction
  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the LICENSE file for more information.
------------------------------------------------------------------------*/

#ifndef SIMPLEJOBCOORDINATOR_H
#define SIMPLEJOBCOORDINATOR_H

#include <PkMutex.h>
#include <PkWaitCondition.h>

#include "kis_assert.h"
#include "atomic.h"

#define SANITY_CHECK

class SimpleJobCoordinator
{
public:
    struct Job {
        virtual ~Job()
        {
        }

        virtual void run() = 0;
    };

private:
    Atomic<unsigned long long> m_job;
    PkMutex mutex;
    PkWaitCondition condVar;

public:
    SimpleJobCoordinator() : m_job(0)
    {
    }

    Job* loadConsume() const
    {
        return (Job*) m_job.load(Consume);
    }

    void storeRelease(Job* job)
    {
        {
            PkMutexLocker guard(&mutex);
            m_job.store(reinterpret_cast<unsigned long long>(job), Release);
        }

        condVar.wakeAll();
    }

    void participate()
    {
        unsigned long long prevJob = 0;

        for (;;) {
            unsigned long long job = m_job.load(Consume);
            if (job == prevJob) {
                PkMutexLocker guard(&mutex);

                for (;;) {
                    job = m_job.loadNonatomic(); // No concurrent writes inside lock
                    if (job != prevJob) {
                        break;
                    }

                    condVar.wait(&mutex);
                }
            }

            if (job == 1) {
                return;
            }

            reinterpret_cast<Job*>(job)->run();
            prevJob = job;
        }
    }

    void runOne(Job* job)
    {
#ifdef SANITY_CHECK
        KIS_ASSERT_RECOVER_NOOP(job != (Job*) m_job.load(Relaxed));
#endif // SANITY_CHECK
        storeRelease(job);
        job->run();
    }

    void end()
    {
        {
            PkMutexLocker guard(&mutex);
            m_job.store(1, Release);
        }

        condVar.wakeAll();
    }
};

#endif // SIMPLEJOBCOORDINATOR_H
