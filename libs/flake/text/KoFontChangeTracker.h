/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTCHANGETRACKER_H
#define KOFONTCHANGETRACKER_H

#include <QObject>
// [migrate] missing include for Pk/Qt type
#include <PkScopedPointer.h>
#include <PkString.h>
#include <PkStringList.h>

/**
 * @brief The KoFontChangeTracker class
 * This class keeps track of the paths FontConfig is looking at,
 * and resets the font registry if they change.
 */
class KoFontChangeTracker : public QObject
{
    Q_OBJECT
public:
    explicit KoFontChangeTracker(PkStringList paths, QObject *parent = nullptr);
    ~KoFontChangeTracker();

    /// This should be called after fontregistry initialization is done to start the signal compressor.
    void resetChangeTracker();
Q_SIGNALS:
    void sigUpdateConfig();
private Q_SLOTS:
    void directoriesChanged();
private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOFONTCHANGETRACKER_H
