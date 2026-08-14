/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_import_profile_policy.h"

#include <QApplication>
#include <QCursor>
#include <QStack>

#include <KConfigGroup>
#include <KSharedConfig>

#include "kis_dlg_png_import.h"

namespace
{

constexpr int PasteAsk = 2;

class CursorOverrideHijacker
{
public:
    CursorOverrideHijacker()
    {
        while (QApplication::overrideCursor()) {
            m_cursorStack.push(*QApplication::overrideCursor());
            QApplication::restoreOverrideCursor();
        }
    }

    ~CursorOverrideHijacker()
    {
        while (!m_cursorStack.isEmpty()) {
            QApplication::setOverrideCursor(m_cursorStack.pop());
        }
    }

private:
    QStack<QCursor> m_cursorStack;
};

}

KisPngImportProfileDesktopPolicy::KisPngImportProfileDesktopPolicy(bool batchMode)
    : m_batchMode(batchMode)
{
}

QString KisPngImportProfileDesktopPolicy::chooseColorProfile(
    const KisPngImportProfileRequest &request)
{
    if (m_batchMode || qAppName().toLower().contains(QStringLiteral("test"))) {
        return QString();
    }

    const KConfigGroup config = KSharedConfig::openConfig()->group("");
    if (config.readEntry("pasteBehaviour", PasteAsk) != PasteAsk) {
        return QString();
    }

    KisDlgPngImport dialog(request.sourcePath,
                          request.colorModelId,
                          request.colorDepthId);
    CursorOverrideHijacker cursorOverride;
    Q_UNUSED(cursorOverride);
    dialog.exec();
    return dialog.profile();
}
