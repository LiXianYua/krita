/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSQLQUERYLOADER_H
#define KISSQLQUERYLOADER_H

#include <kritaresources_export.h>

#include <exception>
#include <utility>
#include <PkSqlError.h>
#include <PkSqlQuery.h>
#include <PkStringList.h>

/** Loads an embedded SQL script or an explicitly supplied script and executes it. */
class KRITARESOURCES_EXPORT KisSqlQueryLoader
{
public:
    struct single_statement_mode_t {};
    static constexpr single_statement_mode_t single_statement_mode{};

    struct FileException : std::exception
    {
        FileException(PkString message_, PkString filePath_, PkString fileErrorString_)
            : message(std::move(message_))
            , filePath(std::move(filePath_))
            , fileErrorString(std::move(fileErrorString_))
        {}

        PkString message;
        PkString filePath;
        PkString fileErrorString;
    };

    struct SQLException : std::exception
    {
        SQLException(PkString message_, PkString filePath_, int statementIndex_, PkSqlError sqlError_)
            : message(std::move(message_))
            , filePath(std::move(filePath_))
            , statementIndex(statementIndex_)
            , sqlError(std::move(sqlError_))
        {}

        PkString message;
        PkString filePath;
        int statementIndex {0};
        PkSqlError sqlError;
    };

    explicit KisSqlQueryLoader(const PkString &fileName);
    KisSqlQueryLoader(const PkString &fileName, single_statement_mode_t);
    KisSqlQueryLoader(const PkString &scriptName, const PkString &script);
    KisSqlQueryLoader(const PkString &scriptName, const PkString &script, single_statement_mode_t);
    ~KisSqlQueryLoader();

    /** Resolve a canonical alias or a legacy :/alias and report a missing entry explicitly. */
    static PkString loadEmbeddedScript(const PkString &fileName);

    PkSqlQuery &query();
    void exec();
    void execBatch();

private:
    void init(const PkString &fileName, const PkString &entireScript, bool singleStatementMode);
    void initEmbedded(const PkString &fileName, bool singleStatementMode);

    PkSqlQuery m_query;
    PkStringList m_statements;
    bool m_singleStatementMode {false};
    PkString m_fileName;
};

#endif /* KISSQLQUERYLOADER_H */
