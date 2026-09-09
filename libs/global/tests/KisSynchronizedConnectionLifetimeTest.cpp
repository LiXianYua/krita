/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSynchronizedConnection.h"

#include <PkThread.h>
#include <PkThreadCallQueue.h>

int main()
{
    PkThread::registerMainThread();
    PkThreadCallQueue::warmUpCurrentThread();

    int delivered = 0;
    {
        KisSynchronizedConnection<> connection([&delivered] {
            ++delivered;
        });
        connection.start();
        if (PkThreadCallQueue::pendingCount() != 1 || delivered != 0) {
            return 1;
        }
    }

    // Destroyed connections must discard their queued delivery.  The pump is
    // deliberately explicit: installing a production pump belongs to S-10.
    if (PkThreadCallQueue::processPendingCalls() != 1 || delivered != 0) {
        return 2;
    }

    {
        KisSynchronizedConnection<> connection([&delivered] {
            ++delivered;
        });
        connection.start();
        if (PkThreadCallQueue::processPendingCalls() != 1 || delivered != 1) {
            return 3;
        }
    }

    return 0;
}
