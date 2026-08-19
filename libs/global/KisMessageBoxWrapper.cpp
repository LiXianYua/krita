/*
 *  SPDX-FileCopyrightText: 2025 Halla Rempt <halla@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisMessageBoxWrapper.h>

#include <PkString.h>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

namespace KisMessageBoxWrapper {

int doNotAskAgainMessageBoxWrapper(PkMessageBox *messageBox, const PkString &identifier)
{
    KConfigGroup cfg(KSharedConfig::openConfig(), "DoNotAskAgain");
    bool showMessage = cfg.readEntry(identifier, true);
    if (showMessage) {
        PkCheckBox *cb = new PkCheckBox(i18n("Don't ask this again"));
        messageBox->setCheckBox(cb);
        const int res = messageBox->exec();
        cfg.writeEntry(identifier, cb->checkState() == Pk::CheckState::Unchecked);
        return res;
    }
    else {
        return PkMessageBox::Yes;
    }
}

}
