/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_BOOKMARKED_CONFIGURATION_MANAGER_H_
#define _KIS_BOOKMARKED_CONFIGURATION_MANAGER_H_

#include <PkList.h>
#include "kis_serializable_configuration.h"

class PkString;

#include "kritaimage_export.h"

class KRITAIMAGE_EXPORT KisBookmarkedConfigurationManager
{
public:
    static const char ConfigDefault[];
    static const char ConfigLastUsed[];
public:
    /**
     * @param configEntryGroup name of the configuration entry with the
     * bookmarked configurations.
     */
    KisBookmarkedConfigurationManager(const PkString & configEntryGroup, KisSerializableConfigurationFactory*);
    ~KisBookmarkedConfigurationManager();
    /**
     * Load the configuration.
     */
    KisSerializableConfigurationSP load(const PkString & configname) const;
    /**
     * Save the configuration.
     */
    void save(const PkString & configname, const KisSerializableConfigurationSP);
    /**
     * @return true if the configuration configname exists
     */
    bool exists(const PkString & configname) const;
    /**
     * @return the list of the names of configurations.
     */
    PkList<PkString> configurations() const;
    /**
     * @return the default configuration
     */
    KisSerializableConfigurationSP defaultConfiguration() const;
    /**
     * Remove a bookmarked configuration
     */
    void remove(const PkString & name);
    /**
     * Generate an unique name, for instance when the user is creating a new
     * entry.
     * @param base the base of the new name, including a "%1" for incrementing
     *      the number, for instance : "New Configuration %1", then this function
     *      will return the string where %1 will be replaced by the lowest number
     *      and be nonexistent in the lists of configuration
     */
    PkString uniqueName(const PkString & base);



private:
    PkString configEntryGroup() const;
private:
    struct Private;
    Private* const d;
};

#endif
