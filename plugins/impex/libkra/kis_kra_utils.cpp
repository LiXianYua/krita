/* This file is part of the KDE project
 * Copyright 2011 (C) Silvio Heinrich <plassy@web.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_kra_utils.h"

PkString KRA::flagsToString(const PkBitArray& flags, int size, char trueToken, char falseToken, bool defaultTrue)
{
    size = (size < 0) ? flags.count() : size;
    
    PkString string(size, defaultTrue ? trueToken : falseToken);
    
    for(int i=0; i < qMin(size, flags.count()); ++i)
        string[i] = flags[i] ? trueToken : falseToken;
    
    return string;
}

PkBitArray KRA::stringToFlags(const PkString& string, int size, char token, bool defaultTrue)
{
    size = (size < 0) ? string.length() : size;
    
    PkBitArray flags(size, defaultTrue);
    
    for(int i=0; i < qMin(size, string.length()); ++i)
        flags[i] = (string[i] == token) ? !defaultTrue : defaultTrue;
    
    return flags;
}
