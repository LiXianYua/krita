#include "PkTestCompare.h"

std::string pkTestCompareFailureMessage(const char *actualExpr, const char *expectedExpr,
                                        const std::string &actualStr, const std::string &expectedStr)
{
    std::string msg = "Compared values are not the same\n";
    msg += "   Actual   (";
    msg += actualExpr;
    msg += ") : ";
    msg += actualStr;
    msg += "\n   Expected (";
    msg += expectedExpr;
    msg += ") : ";
    msg += expectedStr;
    return msg;
}
