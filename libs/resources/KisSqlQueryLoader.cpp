/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSqlQueryLoader.h"
#include "KisSqlScripts.h"

#include <sstream>
#include <string>

#include <kis_assert.h>

void KisSqlQueryLoader::init(const PkString &fileName, const PkString &entireScript,
                             bool singleStatementMode)
{
    m_singleStatementMode = singleStatementMode;
    m_fileName = fileName;

    std::istringstream stream(entireScript.PkToUtf8());
    std::string line;
    PkString normalized;
    while (std::getline(stream, line)) {
        const PkString statement = PkString(line.c_str()).trimmed();
        if (!statement.startsWith(PkString("--"))) {
            if (!normalized.isEmpty()) normalized += PkString(" ");
            normalized += statement;
        }
    }

    for (const PkString &part : normalized.split(u';')) {
        const PkString statement = part.trimmed();
        if (!statement.isEmpty()) m_statements.append(statement);
    }

    if (m_singleStatementMode) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(m_statements.size() == 1);
        if (!m_query.prepare(m_statements.first())) {
            throw SQLException(PkString("Failed to prepare an sql query from embedded script"),
                               m_fileName, 0, m_query.lastError());
        }
    }
}

void KisSqlQueryLoader::initEmbedded(const PkString &fileName, bool singleStatementMode)
{
    std::string alias = fileName.PkToUtf8();
    if (alias.rfind(":/", 0) == 0) alias.erase(0, 2);
    else if (!alias.empty() && alias.front() == ':') alias.erase(0, 1);

    const char *script = kisSqlScript(alias.c_str());
    if (!script) {
        throw FileException(PkString("Could not load embedded SQL script"), fileName,
                            PkString("Unknown SQL resource alias"));
    }
    init(fileName, PkString(script), singleStatementMode);
}

KisSqlQueryLoader::KisSqlQueryLoader(const PkString &fileName)
{
    initEmbedded(fileName, false);
}

KisSqlQueryLoader::KisSqlQueryLoader(const PkString &fileName, single_statement_mode_t)
{
    initEmbedded(fileName, true);
}

KisSqlQueryLoader::KisSqlQueryLoader(const PkString &scriptName, const PkString &script)
{
    init(scriptName, script, false);
}

KisSqlQueryLoader::KisSqlQueryLoader(const PkString &scriptName, const PkString &script,
                                     single_statement_mode_t)
{
    init(scriptName, script, true);
}

KisSqlQueryLoader::~KisSqlQueryLoader() = default;

PkSqlQuery &KisSqlQueryLoader::query()
{
    return m_query;
}

void KisSqlQueryLoader::exec()
{
    if (m_singleStatementMode) {
        if (!m_query.exec()) {
            throw SQLException(PkString("Failed to execute sql from embedded script"),
                               m_fileName, 0, m_query.lastError());
        }
    } else {
        for (int i = 0; i < m_statements.size(); ++i) {
            if (!m_query.exec(m_statements.at(i))) {
                throw SQLException(PkString("Failed to execute sql from embedded script"),
                                   m_fileName, i, m_query.lastError());
            }
        }
    }
}

void KisSqlQueryLoader::execBatch()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_singleStatementMode);
    if (!m_query.execBatch()) {
        throw SQLException(PkString("Failed to batch execute sql from embedded script"),
                           m_fileName, 0, m_query.lastError());
    }
}
