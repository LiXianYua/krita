/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTCHANGETRACKER_H
#define KOFONTCHANGETRACKER_H

#include <PkScopedPointer.h>
#include <PkString.h>
#include <PkStringList.h>

/**
 * @brief The KoFontChangeTracker class
 * This class keeps track of the paths FontConfig is looking at,
 * and resets the font registry if they change.
 */
class KoFontChangeTracker
{
public:
    explicit KoFontChangeTracker(PkStringList paths);
    ~KoFontChangeTracker();

    /// This should be called after fontregistry initialization is done to start the signal compressor.
    void resetChangeTracker();
    bool directoriesChanged() const;
private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOFONTCHANGETRACKER_H
