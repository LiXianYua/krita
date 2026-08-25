/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISIMPORTUSERFEEDBACKINTERFACE_H
#define KISIMPORTUSERFEEDBACKINTERFACE_H

#include <functional>

#include "kritaimpex_export.h"

class PkWidget;

/**
 * Sometimes the importing filter may face some werd issue that needs
 * user's input/decision.
 */
class KRITAIMPEX_EXPORT KisImportUserFeedbackInterface
{
public:
    using AskCallback = std::function<bool(PkWidget*)>;

    enum Result {
        Success = 0,
        UserCancelled,
        SuppressedByBatchMode
    };

public:
    KisImportUserFeedbackInterface() = default;


    virtual ~KisImportUserFeedbackInterface();

    /**
     * @brief ask the user a question about the loading process
     *
     * @param callback a functor that actually asks the user
     * @return the result of the operation
     */
    virtual Result askUser(AskCallback callback) = 0;

private:
    KisImportUserFeedbackInterface(const KisImportUserFeedbackInterface&) = delete;
    KisImportUserFeedbackInterface(KisImportUserFeedbackInterface&&) = delete;
    KisImportUserFeedbackInterface& operator=(const KisImportUserFeedbackInterface&) = delete;
    KisImportUserFeedbackInterface& operator=(KisImportUserFeedbackInterface&&) = delete;
};

#endif // KISIMPORTUSERFEEDBACKINTERFACE_H
