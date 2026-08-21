/*
    This file is part of the KDE libraries

    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allen Winter <winter@kde.org>
    SPDX-FileCopyrightText: 2006 Gregory S. Hayes <syncomm@kde.org>
    SPDX-FileCopyrightText: 2006 Jaison Lee <lee.jaison@gmail.com>
    SPDX-FileCopyrightText: 2011 Romain Perier <bambi@ubuntu.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "KisBackup.h"

#include "KisGlobalFileSystem.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path withExtension(fs::path path, const std::string &extension)
{
    path += fs::u8path(extension);
    return path;
}

bool copyReplacing(const fs::path &source, const fs::path &destination)
{
    std::error_code error;
    fs::remove(destination, error);
    error.clear();
    return fs::copy_file(source, destination, fs::copy_options::none, error) && !error;
}

bool parseBackupNumber(const std::string &name,
                       const std::string &prefix,
                       const std::string &suffix,
                       unsigned int *number)
{
    if (name.size() < prefix.size() + suffix.size() + 1 ||
        name.compare(0, prefix.size(), prefix) != 0 ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }

    const std::size_t numberLength = name.size() - prefix.size() - suffix.size();
    const std::string numberText = name.substr(prefix.size(), numberLength);
    unsigned int parsed = 0;
    const auto result = std::from_chars(numberText.data(),
                                        numberText.data() + numberText.size(),
                                        parsed);
    if (result.ec != std::errc() || result.ptr != numberText.data() + numberText.size()) {
        return false;
    }
    *number = parsed;
    return true;
}

} // namespace

bool KisBackup::backupFile(const PkString &qFilename, const PkString &backupDir)
{
    return (simpleBackupFile(qFilename, backupDir, PkString("~")));
}

bool KisBackup::simpleBackupFile(const PkString &qFilename, const PkString &backupDir, const PkString &backupExtension)
{
    const fs::path source = KisGlobalFileSystem::toPath(qFilename);
    const std::string extension = backupExtension.PkToUtf8();
    fs::path backupFileName = withExtension(source, extension);

    if (!backupDir.isEmpty()) {
        backupFileName = withExtension(
            KisGlobalFileSystem::toPath(backupDir) / source.filename(), extension);
    }

    return copyReplacing(source, backupFileName);
}

bool KisBackup::numberedBackupFile(const PkString &qFilename, const PkString &backupDir, const PkString &backupExtension, const uint maxBackups)
{
    const fs::path source = KisGlobalFileSystem::toPath(qFilename);
    fs::path directory = backupDir.isEmpty()
        ? source.parent_path() : KisGlobalFileSystem::toPath(backupDir);
    if (directory.empty()) {
        directory = fs::path(".");
    }
    const std::string sourceName = source.filename().u8string();
    const std::string extension = backupExtension.PkToUtf8();
    const std::string prefix = sourceName + ".";

    const auto backupPath = [&](unsigned int number) {
        return directory / fs::u8path(prefix + std::to_string(number) + extension);
    };

    std::vector<fs::directory_entry> entries;
    std::error_code error;
    for (fs::directory_iterator iterator(directory, fs::directory_options::skip_permission_denied, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        entries.push_back(*iterator);
    }
    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &left,
                                                  const fs::directory_entry &right) {
        return left.path().filename().u8string() < right.path().filename().u8string();
    });

    uint maxBackupFound = 0;
    for (const fs::directory_entry &entry : entries) {
        error.clear();
        if (fs::is_symlink(entry.symlink_status(error)) || error ||
            !entry.is_regular_file(error) || error) {
            continue;
        }

        uint number = 0;
        if (!parseBackupNumber(entry.path().filename().u8string(), prefix, extension, &number)) {
            continue;
        }
        if (number >= maxBackups) {
            error.clear();
            fs::remove(entry.path(), error);
        } else {
            maxBackupFound = std::max(maxBackupFound, number);
        }
    }

    // Next, rename max-1 to max, max-2 to max-1, etc.
    fs::path to = backupPath(maxBackupFound + 1);

    for (uint i = maxBackupFound; i > 0; --i) {
        const fs::path from = backupPath(i);
        error.clear();
        fs::rename(from, to, error);
        to = from;
    }

    // Finally create most recent backup by copying the file to backup number 1.
    error.clear();
    return fs::copy_file(source, backupPath(1), fs::copy_options::none, error) && !error;
}
