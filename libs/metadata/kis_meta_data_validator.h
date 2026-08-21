/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_META_DATA_VALIDATION_RESULT_H_
#define _KIS_META_DATA_VALIDATION_RESULT_H_

#include <PkMap.h>
#include <PkString.h>

#include <kritametadata_export.h>

namespace KisMetaData
{
class Store;
/**
 * This class contains information on the validation results of a \ref KisMetaData::Store .
 */
class KRITAMETADATA_EXPORT Validator
{
public:
    class KRITAMETADATA_EXPORT Reason
    {
        friend class Validator;
    public:
        enum Type {
            UNKNOWN_REASON,
            UNKNOWN_ENTRY,
            INVALID_TYPE,
            INVALID_VALUE
        };
    public:
        Reason(Type type = UNKNOWN_REASON);
        Reason(const Reason&);
        Reason& operator=(const Reason&);
    public:
        ~Reason();
        Type type() const;
    private:
        struct Private;
        Private* const d;
    };
public:
    /**
     * Validate a store. This constructor will call the \ref revalidate function.
     */
    Validator(const Store*);
    ~Validator();
    int countInvalidEntries() const;
    int countValidEntries() const;
    const PkMap<PkString, Reason>& invalidEntries() const;
    /**
     * Call this function to revalidate the store.
     */
    void revalidate();
private:
    struct Private;
    Private* const d;
};
}

#endif
