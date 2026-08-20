/*
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KORESOURCEPATHS_H
#define KORESOURCEPATHS_H

#include <PkScopedPointer.h>
#include <PkString.h>
#include <PkStringList.h>

#include <PkFlags.h>

#include <kritaresources_export.h>


/**
 * The usual place to look for assets is the platform AppDataLocation.
 * This corresponds to XDG_DATA_DIRS on Linux. To ensure your installation and
 * path are configured correctly, ensure your files are located in the directories
 * contained in this variable:
 *
 * platform resource-storage locations for AppDataLocation.
 *
 * This can be overridden in Krita's configuration.
 *
 * Unfortunately, we are mixing up two things in the appdatalocation:
 *
 *  * resources: brushes, presets and so on
 *  * assets: color themes, icc profiles and other weird stuff
 *
 * There are many debug lines that can be uncommented for more specific installation
 * checks. In the future these should be converted to qloggingcategory to enable
 * convenient enable/disable functionality.
 *
 * Note: DO NOT USE THIS CLASS WHEN LOCATING RESOURCES LIKE BRUSHES OR GRADIENTS. Use
 * KisResourceLocator instead.
 */
class KRITARESOURCES_EXPORT KoResourcePaths
{
public:

    KoResourcePaths();
    virtual ~KoResourcePaths();

    enum SearchOption { NoSearchOptions = 0,
                        Recursive = 1,
                        IgnoreExecBit = 4
                      };
    PK_DECLARE_FLAGS(SearchOptions, SearchOption)



    static PkString getApplicationRoot();

    /**
     * @brief getAppDataLocation Returns the configured AppDataLocation. The
     * user can configure the location where resources and other user writable items are stored
     * now.
     *
     * @return the configured location for the appdata folder
     */
    static PkString s_overrideAppDataLocation; // This is set from KisApplicationArguments
    static PkString getAppDataLocation();

    /**
     * @brief getAllAppDataLocationsForWindowsStore Use this to get both private and general appdata folders
     * which also considers user's choice of custom resource folder
     * Used in GeneralTab in kis_dlg_preferences, and KisViewManager::openResourceDirectory().
     * @param standardLocation - location in standard %AppData%
     * @param privateLocation - location in private app %AppData% location, only relevant for Windows Store
     * @return either both appdata locations, or just the custom resource folder
     */
    static void getAllUserResourceFoldersLocationsForWindowsStore(PkString& standardLocation, PkString& privateLocation);

    /**
     * Adds suffixes for asset types.
     *
     * You may add as many as you need, but it is advised that there
     * is exactly one to make writing definite.
     *
     * The later a suffix is added, the higher its priority. Note, that the
     * suffix should end with / but doesn't have to start with one (as prefixes
     * should end with one). So adding a suffix for app_pics would look
     * like KoStandardPaths::addResourceType("app_pics", "data", "app/pics");
     *
     * @param type Specifies a short descriptive string to access
     * files of this type.
     * @param basetype Specifies an already known type, or 0 if none
     * @param relativename Specifies a directory relative to the basetype
     * @param priority if true, the directory is added before any other,
     * otherwise after
     */
    static void addAssetType(const PkString &type, const char *basetype,
                                const PkString &relativeName, bool priority = true);


    /**
     * Adds absolute path at the beginning of the search path for
     * particular types (for example in case of icons where
     * the user specifies extra paths).
     *
     * You shouldn't need this function in 99% of all cases besides
     * adding user-given paths.
     *
     * @param type Specifies a short descriptive string to access files
     * of this type.
     * @param absdir Points to directory where to look for this specific
     * type. Nonexistent directories may be saved but pruned.
     * @param priority if true, the directory is added before any other,
     * otherwise after
     */
    static void addAssetDir(const PkString &type, const PkString &dir, bool priority = true);

    /**
     * Tries to find a resource in the following order:
     * @li All PREFIX/\<relativename> paths (most recent first).
     * @li All absolute paths (most recent first).
     *
     * The filename should be a filename relative to the base dir
     * for resources. So it's a way to get the path to libkdecore.la
     * to findResource("lib", "libkdecore.la"). KStandardDirs will
     * then look into the subdir lib of all elements of all prefixes
     * ($KDEDIRS) for a file libkdecore.la and return the path to
     * the first one it finds (e.g. /opt/kde/lib/libkdecore.la).
     *
     * Example:
     * @code
     * PkString iconfilename = KoResourcePaths::findAsset("icon", "oxygen/22x22/apps/ktip.png");
     * @endcode
     *
     * @param type The type of the wanted resource
     * @param filename A relative filename of the resource.
     *
     * @return A full path to the filename specified in the second
     *         argument, or an empty string if not found.
     */

    static PkString findAsset(const PkString &type, const PkString &fileName);

    /**
     * Tries to find all directories whose names consist of the
     * specified type and a relative path. So
     * findDirs("xdgdata-apps", "Settings") would return
     * @li /home/joe/.local/share/applications/Settings/
     * @li /usr/share/applications/Settings/
     *
     * (from the most local to the most global)
     *
     * Note that it appends @c / to the end of the directories,
     * so you can use this right away as directory names.
     *
     * @param type The type of the base directory.
     * @param reldir Relative directory.
     *
     * @return A list of matching directories, or an empty
     *         list if the resource specified is not found.
     */
    static PkStringList findDirs(const PkString &type);

    /**
     * Tries to find all resources with the specified type.
     *
     * The function will look into all specified directories
     * and return all filenames in these directories.
     *
     * The "most local" files are returned before the "more global" files.
     *
     * @param type The type of resource to locate directories for.
     * @param filter Only accept filenames that fit to filter. The filter
     *        may consist of an optional directory and a wildcard
     *        wildcard expression. E.g. <tt>"images\*.jpg"</tt>.
     *        Use an empty string if you do not want a filter.
     * @param options if the flags passed include Recursive, subdirectories
     *        will also be search.
     *
     * @return List of all the files whose filename matches the
     *         specified filter.
     */
    static PkStringList findAllAssets(const PkString &type,
                                        const PkString &filter = PkString(),
                                        SearchOptions options = NoSearchOptions);

    /**
     * @param type The type of resource
     * @return The list of possible directories for the specified @p type.
     * The function updates the cache if possible.  If the resource
     * type specified is unknown, it will return an empty list.
     * Note, that the directories are assured to exist beside the save
     * location, which may not exist, but is returned anyway.
     */
    static PkStringList assetDirs(const PkString &type);

    /**
     * Finds a location to save files into for the given type
     * in the user's home directory.
     *
     * @param type The type of location to return.
     * @param suffix A subdirectory name.
     *             Makes it easier for you to create subdirectories.
     *   You can't pass filenames here, you _have_ to pass
     *       directory names only and add possible filename in
     *       that directory yourself. A directory name always has a
     *       trailing slash ('/').
     * @param create If set, saveLocation() will create the directories
     *        needed (including those given by @p suffix).
     *
     * @return A path where resources of the specified type should be
     *         saved, or an empty string if the resource type is unknown.
     */
    static PkString saveLocation(const PkString &type, const PkString &suffix = PkString(), bool create = true);

    /**
     * This function is just for convenience. It simply calls
     * KoResourcePaths::findResource((type, filename).
     *
     * @param type   The type of the wanted resource, see KStandardDirs
     * @param filename   A relative filename of the resource
     *
     * @return A full path to the filename specified in the second
     *         argument, or an empty string if not found
     **/
    static PkString locate(const PkString &type, const PkString &filename);

    /**
     * This function is much like locate. However it returns a
     * filename suitable for writing to. No check is made if the
     * specified @p filename actually exists. Missing directories
     * are created. If @p filename is only a directory, without a
     * specific file, @p filename must have a trailing slash.
     *
     * @param type   The type of the wanted resource, see KStandardDirs
     * @param filename   A relative filename of the resource
     *
     * @return A full path to the filename specified in the second
     *         argument, or an empty string if not found
     **/
    static PkString locateLocal(const PkString &type, const PkString &filename, bool createDir = false);

private:

    void addResourceTypeInternal(const PkString &type, const PkString &basetype,
                                 const PkString &relativeName, bool priority);

    void addResourceDirInternal(const PkString &type, const PkString &absdir, bool priority);

    PkString findResourceInternal(const PkString &type, const PkString &fileName);

    PkStringList findDirsInternal(const PkString &type);

    PkStringList findAllResourcesInternal(const PkString &type,
                                         const PkString &filter = PkString(),
                                         SearchOptions options = NoSearchOptions) const;

    PkStringList resourceDirsInternal(const PkString &type);

    PkString saveLocationInternal(const PkString &type, const PkString &suffix = PkString(), bool create = true);

    PkString locateInternal(const PkString &type, const PkString &filename);

    PkString locateLocalInternal(const PkString &type, const PkString &filename, bool createDir = false);

    PkStringList findExtraResourceDirs() const;

    class Private;
    PkScopedPointer<Private> d;
};

PK_DECLARE_OPERATORS_FOR_FLAGS(KoResourcePaths::SearchOptions)

#endif // KORESOURCEPATHS_H
