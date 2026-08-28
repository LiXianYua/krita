/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CSV_READ_LINE_H_
#define CSV_READ_LINE_H_

#include <PkString.h>
#include <PkStream.h>
#include <PkAuxTypes.h>

/* The .png file names are within 20 characters, only the layer
 * names can be longer. If the length exceeds this, it will be
 * truncated.
 */
#define CSV_FIELD_MAX 50

class CSVReadLine
{

public:
    CSVReadLine();
    ~CSVReadLine();

    int nextLine(PkStream* io);
    bool nextField(PkString *field);
    void rewind();

private:

    char       m_separator;
    int        m_row;
    PkByteArray m_linebuf;
    int        m_pos;
};

#endif
