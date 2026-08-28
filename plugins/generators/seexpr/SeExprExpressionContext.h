/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkMap.h>
#include <PkString.h>
#include <KSeExpr/Expression.h>
#include <cstring>

#include "SeExprVariable.h"

class SeExprExpressionContext : public KSeExpr::Expression
{
public:
    typedef PkMap<std::string, SeExprVariable *> VariableMap;

    VariableMap m_vars;

    SeExprExpressionContext(const PkString &expr);

    virtual KSeExpr::ExprVarRef *resolveVar(const std::string &name) const override;
};
