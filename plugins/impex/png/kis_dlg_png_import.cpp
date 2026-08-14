/*
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "kis_dlg_png_import.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceEngine.h>
#include <KoID.h>
#include <KisSqueezedComboBox.h>

KisDlgPngImport::KisDlgPngImport(const QString &path, const QString &colorModelID, const QString &colorDepthID, QWidget *parent)
    : KoDialog(parent)
{
    setButtons(Ok);
    setDefaultButton(Ok);
    QWidget *page = new QWidget(this);
    dlgWidget.setupUi(page);
    setMainWidget(page);

    dlgWidget.lblFilename->setText(path);

    const QString colorSpaceId = KoColorSpaceRegistry::instance()->colorSpaceId(colorModelID, colorDepthID);
    dlgWidget.cmbProfile->clear();
    QList<const KoColorProfile *>  profileList = KoColorSpaceRegistry::instance()->profilesFor(colorSpaceId);
    QStringList profileNames;
    Q_FOREACH (const KoColorProfile *profile, profileList) {
        profileNames.append(profile->name());
    }
    std::sort(profileNames.begin(), profileNames.end());
    Q_FOREACH (QString stringName, profileNames) {
        dlgWidget.cmbProfile->addSqueezedItem(stringName);
    }
    KConfigGroup config = KSharedConfig::openConfig()->group("");
    const QString profile = config.readEntry(
        "pngImportProfile",
        KoColorSpaceRegistry::instance()->defaultProfileForColorSpace(colorSpaceId));
    dlgWidget.cmbProfile->setCurrent(profile);
}

QString KisDlgPngImport::profile() const
{
    QString p = dlgWidget.cmbProfile->currentUnsqueezedText();
    KConfigGroup config = KSharedConfig::openConfig()->group("");
    config.writeEntry("pngImportProfile", p);
    config.sync();
    return p;
}
