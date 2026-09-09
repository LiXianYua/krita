/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOABSTRACTCANVASRESOURCEINTERFACE_H
#define KOABSTRACTCANVASRESOURCEINTERFACE_H

#include <PkObject.h>
#include <PkSharedPointer.h>
#include "kritaflake_export.h"
// [migrate] missing include for Pk/Qt type
#include <PkString.h>

class PkVariant;

/**
 * \class KoAbstractCanvasResourceInterface
 *
 * Defines an abstract resource that is stored outside the resource manager
 */
class KRITAFLAKE_EXPORT KoAbstractCanvasResourceInterface : public PkObject
{
public:
    KoAbstractCanvasResourceInterface(int key, const PkString debugTag = PkString());

    /**
     * Return the current value of the resource
     */
    virtual PkVariant value() const = 0;

    /**
     * @brief set the value of the current resource
     */
    virtual void setValue(const PkVariant value) = 0;

    /**
     * The key corresponding to the resource
     */
    int key() const;

public:
    /**
     * The signal is emitted when the resource is changed outside
     * the setValue() call by some external entity
     */
    void sigResourceChangedExternal(int key, const PkVariant &value);

private:
    int m_key = -1;
    PkString m_debugTag;
};

typedef PkSharedPointer<KoAbstractCanvasResourceInterface> KoAbstractCanvasResourceInterfaceSP;

#endif // KOABSTRACTCANVASRESOURCEINTERFACE_H
