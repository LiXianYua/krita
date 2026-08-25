/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISEXPORTCHECKBASE_H
#define KISEXPORTCHECKBASE_H

#include <PkString.h>

#include <kis_types.h>

#include "kritaimpex_export.h"

/**
 * @brief The KisExportCheckBase class defines the interface
 * of the individual checks of an export filter's capabilities
 */
class KRITAIMPEX_EXPORT KisExportCheckBase
{
public:

    /// The level determines the level of support the export
    /// filter has for the given feature.
    enum Level {
        SUPPORTED,  //< The filter fully supports this
        PARTIALLY,  //< The filter can handle this, but the user needs to be warned about possible degradation
        UNSUPPORTED //< This cannot be saved using this filter
    };

    /**
     * @brief KisExportCheckBase
     * @param level the level of support the filter has for the given feature
     * @param customWarning A custom warning to use instead of the default one
     */
    KisExportCheckBase(const PkString &id, Level level, const PkString &customWarning = PkString(), bool perLayerCheck = false);

    virtual ~KisExportCheckBase();

    /// @return the unique id of the check
    virtual PkString id() const;

    /// @return whether the image uses this feature
    virtual bool checkNeeded(KisImageSP image) const = 0;

    /// @returns true if this check is only relevant for file formats that can save multiple layers
    virtual bool perLayerCheck() const;

    /// @return the level of support for this feature
    virtual Level check(KisImageSP image) const = 0;

    /// @return the message to show the user
    PkString warning() const;

protected:

    PkString m_id;
    Level m_level {UNSUPPORTED};
    PkString m_warning;
    bool m_perLayerCheck {false};

};

class KRITAIMPEX_EXPORT KisExportCheckFactory
{
public:
    virtual KisExportCheckBase *create(KisExportCheckBase::Level level, const PkString &customWarning = PkString()) = 0;
    virtual ~KisExportCheckFactory() {}
    virtual PkString id() const = 0;
};


#endif
