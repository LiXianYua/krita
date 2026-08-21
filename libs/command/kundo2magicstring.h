/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2014 Alexander Potashev <aspotashev@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KUNDO2MAGICSTRING_H
#define KUNDO2MAGICSTRING_H

#include <PkString.h>
#include <PkDebug.h>

#include <boost/operators.hpp>

#include "kritacommand_export.h"

/**
 * \class KUndo2MagicString is a special wrapper for a string that is
 * going to passed to a KUndo2Command and be later shown in the undo
 * history and undo action in menu. The kernel returns the source text
 * as-is; translation is handled by the Flutter side (D-6).
 *
 * Magic split is used in some languages to split the message in the
 * undo history docker (which is either verb or <a
 * href="https://en.wikipedia.org/wiki/Nominative_case">noun in
 * nominative</a>) and the message in undo/redo actions (which is
 * usually a <a href="https://en.wikipedia.org/wiki/Accusative_case">noun
 * in accusative</a>). When the translator needs it he, splits two
 * translations with '\n' symbol and the magic string will recognize
 * it.
 *
 * \note KUndo2MagicString will never support concatenation operators,
 *       because in many languages you cannot combine words without
 *       knowing the proper case.
 */
class KRITACOMMAND_EXPORT KUndo2MagicString : public boost::equality_comparable<KUndo2MagicString>
{
public:
    /**
     * Construct an empty string. Note that you cannot create a
     * non-empty string without the special functions declared below.
     */
    KUndo2MagicString();

    /**
     * Fetch the main string. That is the one that goes to
     * undo history and resembles the action name in verb/nominative
     */
    PkString toString() const;

    /**
     * Fetch the secondary string which will go to the undo/redo
     * action.  This is usually a noun in accusative. If no
     * secondary string was provided, toString() and
     * toSecondaryString() return the same values.
     */
    PkString toSecondaryString() const;

    /**
     * \return true if the contained string is empty
     */
    bool isEmpty() const;

    bool operator==(const KUndo2MagicString &rhs) const;

private:
    /**
     * Construction of a magic string is allowed only with the means
     * of the special kundo2_text_* functions below.
     */
    explicit KUndo2MagicString(const PkString &text);


    friend KUndo2MagicString kundo2_text_raw(const PkString &text);
    template <typename A1>
    friend KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1);
    template <typename A1, typename A2>
    friend KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2);
    template <typename A1, typename A2, typename A3>
    friend KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2, const A3 &a3);
    template <typename A1, typename A2, typename A3, typename A4>
    friend KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4);


    friend KUndo2MagicString kundo2_text(const char *text);
    template <typename A1>
    friend KUndo2MagicString kundo2_text(const char *text, const A1 &a1);
    template <typename A1, typename A2>
    friend KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2);
    template <typename A1, typename A2, typename A3>
    friend KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2, const A3 &a3);
    template <typename A1, typename A2, typename A3, typename A4>
    friend KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4);


    friend KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text);
    template <typename A1>
    friend KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1);
    template <typename A1, typename A2>
    friend KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2);
    template <typename A1, typename A2, typename A3>
    friend KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3);
    template <typename A1, typename A2, typename A3, typename A4>
    friend KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4);


    template <typename A1>
    friend KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1);
    template <typename A1, typename A2>
    friend KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2);
    template <typename A1, typename A2, typename A3>
    friend KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3);
    template <typename A1, typename A2, typename A3, typename A4>
    friend KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4);


    template <typename A1>
    friend KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1);
    template <typename A1, typename A2>
    friend KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2);
    template <typename A1, typename A2, typename A3>
    friend KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3);
    template <typename A1, typename A2, typename A3, typename A4>
    friend KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4);

private:
    PkString m_text;
};

inline PkDebug operator<<(PkDebug dbg, const KUndo2MagicString &v)
{
    if (v.toString() != v.toSecondaryString()) {
        dbg.nospace() << v.toString() << "(" << v.toSecondaryString() << ")";
    } else {
        dbg.nospace() << v.toString();
    }

    return dbg.space();
}


/**
 * This is a special wrapper to a string which tells explicitly
 * that we don't need a translation for a given string. It is used
 * either in testing or internal commands, which don't go to the
 * stack directly.
 */
inline KUndo2MagicString kundo2_text_raw(const PkString &text)
{
    return KUndo2MagicString(text);
}

template <typename A1>
inline KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1)
{
    return KUndo2MagicString(PkString(text).arg(a1));
}

template <typename A1, typename A2>
inline KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2));
}

template <typename A1, typename A2, typename A3>
inline KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3));
}

template <typename A1, typename A2, typename A3, typename A4>
inline KUndo2MagicString kundo2_text_raw(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3).arg(a4));
}

/**
 * Same as kundo2_text_raw, but intended for strings going to
 * the undo history. After D-6 it is a pass-through: the source text
 * is returned as-is, translation is handled by the Flutter side.
 */

inline KUndo2MagicString kundo2_text(const char *text)
{
    return KUndo2MagicString(PkString(text));
}

template <typename A1>
inline KUndo2MagicString kundo2_text(const char *text, const A1 &a1)
{
    return KUndo2MagicString(PkString(text).arg(a1));
}

template <typename A1, typename A2>
inline KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2));
}

template <typename A1, typename A2, typename A3>
inline KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3));
}

template <typename A1, typename A2, typename A3, typename A4>
inline KUndo2MagicString kundo2_text(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3).arg(a4));
}

/**
 * Context variant of kundo2_text. After D-6 the context argument is
 * ignored (translation is handled by the Flutter side).
 */
inline KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text)
{
    (void)ctxt;
    return KUndo2MagicString(PkString(text));
}

template <typename A1>
inline KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1)
{
    (void)ctxt;
    return KUndo2MagicString(PkString(text).arg(a1));
}

template <typename A1, typename A2>
inline KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2)
{
    (void)ctxt;
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2));
}

template <typename A1, typename A2, typename A3>
inline KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{
    (void)ctxt;
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3));
}

template <typename A1, typename A2, typename A3, typename A4>
inline KUndo2MagicString kundo2_text_ctx(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    (void)ctxt;
    return KUndo2MagicString(PkString(text).arg(a1).arg(a2).arg(a3).arg(a4));
}

/**
 * Plural-aware variant. After D-6 the kernel returns the plural source
 * text (plur); the real plural selection is left to the Flutter
 * translation layer.
 */

template <typename A1>
inline KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1)
{
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1));
}

template <typename A1, typename A2>
inline KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2)
{
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2));
}

template <typename A1, typename A2, typename A3>
inline KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3)
{
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2).arg(a3));
}

template <typename A1, typename A2, typename A3, typename A4>
inline KUndo2MagicString kundo2_text_plural(const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2).arg(a3).arg(a4));
}


/**
 * Plural-aware variant with context. After D-6 both the context and the
 * singular source text are ignored; the kernel returns the plural source
 * text.
 */
template <typename A1>
inline KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1)
{
    (void)ctxt;
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1));
}

template <typename A1, typename A2>
inline KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2)
{
    (void)ctxt;
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2));
}

template <typename A1, typename A2, typename A3>
inline KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3)
{
    (void)ctxt;
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2).arg(a3));
}

template <typename A1, typename A2, typename A3, typename A4>
inline KUndo2MagicString kundo2_text_ctx_plural(const char *ctxt, const char *sing, const char *plur, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    (void)ctxt;
    (void)sing;
    return KUndo2MagicString(PkString(plur).arg(a1).arg(a2).arg(a3).arg(a4));
}

#endif /* KUNDO2MAGICSTRING_H */
