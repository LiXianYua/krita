#pragma once

#include <PkVector.h>

#include <initializer_list>

/**
 * A Qt-free value carrier for the encoded integer chords used by Krita's
 * shortcut matcher.  The encoding is the Qt 5.15 Key | Modifier bit layout,
 * whose values are provided by PkNamespace.
 */
class PkKeySequence
{
public:
    PkKeySequence() = default;
    PkKeySequence(std::initializer_list<int> chords)
        : m_chords(chords)
    {
    }

    bool isEmpty() const noexcept { return m_chords.isEmpty(); }
    int size() const noexcept { return m_chords.size(); }
    int operator[](int index) const { return m_chords.at(index); }

private:
    PkVector<int> m_chords;
};
