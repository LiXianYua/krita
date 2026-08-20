/*
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisResourceSearchBoxFilter.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace
{

std::string trimAscii(std::string value)
{
    const auto notSpace = [](unsigned char character) {
        return !std::isspace(character);
    };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());
    return value;
}

std::string lowerUnicodeUtf8(const PkString &value)
{
    return value.toLower().PkToUtf8();
}

bool anyContains(const std::vector<std::string> &values,
                 const std::string &part)
{
    for (const std::string &value : values) {
        if (value.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

class KisResourceSearchBoxFilter::Private
{
public:
    PkString filter;
    std::set<std::string> tagExactIncluded;
    std::set<std::string> tagExactExcluded;
    std::set<std::string> resourceExactIncluded;
    std::set<std::string> resourceExactExcluded;
    std::vector<std::string> resourcePartialIncluded;
    std::vector<std::string> resourcePartialExcluded;
    std::vector<std::string> tagPartialIncluded;
    std::vector<std::string> tagPartialExcluded;
};

KisResourceSearchBoxFilter::KisResourceSearchBoxFilter()
    : d(new Private)
{
}

KisResourceSearchBoxFilter::~KisResourceSearchBoxFilter()
{
    delete d;
}

void KisResourceSearchBoxFilter::setFilter(const PkString &filter)
{
    d->filter = filter;
    initializeFilterData();
}

bool KisResourceSearchBoxFilter::matchesResource(
    const PkString &resourceName,
    const PkStringList &tagList) const
{
    const std::string name = lowerUnicodeUtf8(resourceName);
    std::vector<std::string> tags;
    tags.reserve(static_cast<std::size_t>(tagList.size()));
    for (const PkString &tag : tagList) {
        tags.push_back(lowerUnicodeUtf8(tag));
    }

    if (!d->resourceExactIncluded.empty() &&
        d->resourceExactIncluded.count(name) == 0) {
        return false;
    }
    if (d->resourceExactExcluded.count(name) != 0) {
        return false;
    }

    for (const std::string &part : d->resourcePartialIncluded) {
        if (name.find(part) == std::string::npos && !anyContains(tags, part)) {
            return false;
        }
    }
    for (const std::string &part : d->resourcePartialExcluded) {
        if (name.find(part) != std::string::npos || anyContains(tags, part)) {
            return false;
        }
    }

    for (const std::string &part : d->tagPartialIncluded) {
        if (!anyContains(tags, part)) {
            return false;
        }
    }
    for (const std::string &part : d->tagPartialExcluded) {
        if (anyContains(tags, part)) {
            return false;
        }
    }

    for (const std::string &tag : d->tagExactIncluded) {
        if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            return false;
        }
    }
    for (const std::string &tag : d->tagExactExcluded) {
        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
            return false;
        }
    }
    return true;
}

bool KisResourceSearchBoxFilter::isEmpty() const
{
    return d->filter.isEmpty();
}

void KisResourceSearchBoxFilter::clearFilterData()
{
    d->tagExactIncluded.clear();
    d->tagExactExcluded.clear();
    d->resourceExactIncluded.clear();
    d->resourceExactExcluded.clear();
    d->resourcePartialIncluded.clear();
    d->resourcePartialExcluded.clear();
    d->tagPartialIncluded.clear();
    d->tagPartialExcluded.clear();
}

void KisResourceSearchBoxFilter::initializeFilterData()
{
    clearFilterData();

    const std::string source = d->filter.PkToUtf8();
    std::size_t offset = 0;
    while (offset <= source.size()) {
        const std::size_t comma = source.find(',', offset);
        std::string token = trimAscii(source.substr(
            offset,
            comma == std::string::npos ? std::string::npos : comma - offset));
        offset = comma == std::string::npos ? source.size() + 1 : comma + 1;
        if (token.empty()) {
            continue;
        }

        token = lowerUnicodeUtf8(PkString::PkFromUtf8(
            token.data(), static_cast<int>(token.size())));
        bool included = true;
        if (!token.empty() && token.front() == '!') {
            included = false;
            token.erase(token.begin());
        }

        bool tag = false;
        if (!token.empty() && token.front() == '#') {
            tag = true;
            token.erase(token.begin());
        }

        bool exact = token.size() >= 2 && token.front() == '"' &&
            token.back() == '"';
        if (exact) {
            token = token.substr(1, token.size() - 2);
        }
        if (token.empty()) {
            continue;
        }

        if (tag && exact) {
            (included ? d->tagExactIncluded : d->tagExactExcluded).insert(token);
        } else if (tag) {
            (included ? d->tagPartialIncluded : d->tagPartialExcluded)
                .push_back(token);
        } else if (exact) {
            (included ? d->resourceExactIncluded : d->resourceExactExcluded)
                .insert(token);
        } else {
            (included ? d->resourcePartialIncluded : d->resourcePartialExcluded)
                .push_back(token);
        }
    }
}
