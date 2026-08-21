/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "KisFileUtils.h"

#include "KisGlobalFileSystem.h"

#include <PkString.h>

#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

namespace fs = std::filesystem;

PkString fromUtf8(const std::string &text)
{
    return PkString::PkFromUtf8(text.data(), static_cast<int>(text.size()));
}

struct FileNameParts {
    std::string baseName;
    std::string completeSuffix;
};

FileNameParts splitFileName(const std::string &fileName)
{
    const std::size_t firstDot = fileName.find('.');
    if (firstDot == std::string::npos) {
        return {fileName, std::string()};
    }
    return {fileName.substr(0, firstDot), fileName.substr(firstDot + 1)};
}

bool reuseCounterPrefix(const std::string &fileName,
                        const std::string &separator,
                        FileNameParts *parts)
{
    // This is the literal-string equivalent of the old anchored regular
    // expression.  Searching from right to left preserves its greedy prefix
    // behavior for an empty separator ("foo12" reuses the final digit).
    for (std::size_t separatorPosition = fileName.size(); separatorPosition > 1;) {
        --separatorPosition;
        const std::string prefix = fileName.substr(0, separatorPosition);
        if (prefix.find('.') != std::string::npos) {
            continue;
        }
        if (fileName.compare(separatorPosition, separator.size(), separator) != 0) {
            continue;
        }

        const std::size_t digitsBegin = separatorPosition + separator.size();
        if (digitsBegin >= fileName.size() ||
            std::isdigit(static_cast<unsigned char>(fileName[digitsBegin])) == 0) {
            continue;
        }

        std::size_t digitsEnd = digitsBegin;
        while (digitsEnd < fileName.size() &&
               std::isdigit(static_cast<unsigned char>(fileName[digitsEnd])) != 0) {
            ++digitsEnd;
        }

        if (digitsEnd != fileName.size() &&
            (fileName[digitsEnd] != '.' || digitsEnd + 1 == fileName.size())) {
            continue;
        }

        parts->baseName = prefix;
        parts->completeSuffix = digitsEnd == fileName.size()
            ? std::string() : fileName.substr(digitsEnd + 1);
        return true;
    }
    return false;
}

} // namespace

namespace KritaUtils {

PkString resolveAbsoluteFilePath(const PkString &baseDir, const PkString &fileName)
{
    const fs::path requestedPath = KisGlobalFileSystem::toPath(fileName);
    if (requestedPath.is_absolute()) {
        return fileName;
    }

    const fs::path basePath = KisGlobalFileSystem::toPath(baseDir);
    std::error_code error;
    const bool baseIsDirectory = fs::is_directory(basePath, error);
    const fs::path parent = baseIsDirectory ? basePath : basePath.parent_path();
    const fs::path combined = parent / requestedPath;
    error.clear();
    const fs::path absolute = fs::absolute(combined, error);

    return KisGlobalFileSystem::fromPath(
        (error ? combined : absolute).lexically_normal());
}

PkString deduplicateFileName(const PkString &fileName,
                            const PkString &separator,
                            std::function<bool(PkString)> fileAllowedCallback)
{
    const std::string proposedName =
        KisGlobalFileSystem::toPath(fileName).filename().u8string();
    const std::string separatorText = separator.PkToUtf8();
    std::string proposedFileName = proposedName;
    FileNameParts parts = splitFileName(proposedName);

    /**
     * Search for the separator around the leftmost dot in the filename
     * and try to reuse its counter.
     *
     * The design choice is that there cannot be any dots to the left
     * from the separator. Separator itself can have dots, but it cannot
     * be a part of the file extension.
     */
    reuseCounterPrefix(proposedName, separatorText, &parts);

    unsigned long long counter = 0;
    while (!fileAllowedCallback(fromUtf8(proposedFileName))) {
        proposedFileName = parts.baseName + separatorText + std::to_string(counter++);

        if (!parts.completeSuffix.empty()) {
            proposedFileName += ".";
            proposedFileName += parts.completeSuffix;
        }
    }

    return fromUtf8(proposedFileName);
}
}
