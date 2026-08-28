/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "csv_read_line.h"

#include <kis_debug.h>

#include <PkStream.h>

CSVReadLine::CSVReadLine()
    : m_separator(0)
    , m_row(0)
    , m_linebuf()
    , m_pos(-1)
{
}

CSVReadLine::~CSVReadLine()
{
}

// returns: 0 finished, + continue, - error
int CSVReadLine::nextLine(PkStream *io)
{
    int retval= 0;
    m_pos= -1;

    m_linebuf = io->readLine();

    if (m_linebuf.isEmpty()) {
        retval = io->atEnd() ? 0 : -1;
    } else {
        if (!m_separator) {
            m_separator = ((m_linebuf.size() > 5) && (m_linebuf.data()[5] == ';')) ? ';' : ',';
        }
        m_pos = 0;
        retval = 1;
    }
    return retval;
}

bool CSVReadLine::nextField(PkString *field)
{
    char     strBuf[CSV_FIELD_MAX];
    char    *ptr;
    char     c;
    int      i,p,max;

    p= m_pos;

    if (!field || p < 0) return false;

    ptr= strBuf;
    max= m_linebuf.size();

    do {    if (p >= max) {
                ptr[0]= 0;
                m_pos= -1;
                *field = PkString(strBuf);
                return true;
            }
            c= m_linebuf.data()[p++];
    } while((c == ' ') || (c == '\t'));

    i= 0;

    if (c == '\"') {
        //quoted
        while(p < max) {
            c= m_linebuf.data()[p++];

            if (c == '\"') {

                if (p >= max) break;

                if (m_linebuf.data()[p] != c) break;

                 //double quote escape sequence
                ++p;
            }
            if (i < (CSV_FIELD_MAX - 1))
                ptr[i++]= c;
        }

        while (p < max) {
            c= m_linebuf.data()[p++];
            if (c == m_separator) break;
        }
    } else {
        //without quotes
        while (c != m_separator) {
            if (i < (CSV_FIELD_MAX - 1))
                ptr[i++]= c;

            if (p >= max) break;

            c= m_linebuf.data()[p++];
        }

        while(i > 0) {
            c= ptr[--i];
            if ((c != ' ')  && (c != '\t') &&
                (c != '\r') && (c != '\n')) {
                ++i;
                break;
            }
        }
    }
    ptr[i]= 0;
    m_pos= (p < max) ? p : -1;
    *field = PkString(strBuf);
    return true;
}

void CSVReadLine::rewind()
{
    m_pos= 0;
}
