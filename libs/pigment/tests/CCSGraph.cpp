/*
 *  SPDX-FileCopyrightText: 2007-2008 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#include <DebugPigment.h>

#include "KoColorSpaceRegistry.h"
#include "KoColorConversionSystem.h"

#include <PkString.h>

struct FriendOfColorSpaceRegistry {
    static PkString toDot() {
        return KoColorSpaceRegistry::instance()->colorConversionSystem()->toDot();
    }

    static PkString bestPathToDot(const PkString &srcKey, const PkString &dstKey) {
        return KoColorSpaceRegistry::instance()->colorConversionSystem()->bestPathToDot(srcKey, dstKey);
    }
};

namespace {

std::vector<PkString> collectArgs(int argc, char **argv)
{
    std::vector<PkString> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

bool hasOption(const std::vector<PkString> &args, const char *name)
{
    for (const PkString &a : args) {
        if (a == name) {
            return true;
        }
    }
    return false;
}

PkString optionValue(const std::vector<PkString> &args, const char *name)
{
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) {
            return args[i + 1];
        }
    }
    return PkString();
}

void printHelp(std::ostream &out)
{
    out << "Usage: CCSGraph [options] outputfile\n"
        << "Options:\n"
        << "  --graphs                    return the list of available graphs\n"
        << "  --graph <type>              specify the type of graph (see --graphs to get\n"
        << "                              the full list, the default is full)\n"
        << "  --src-key <key>             specify the key of the source color space\n"
        << "  --dst-key <key>             specify the key of the destination color space\n"
        << "  --output <type>             specify the output (can be ps or dot, the default is ps)\n"
        << "  -h, --help                  show this help\n";
}

} // namespace

int main(int argc, char **argv)
{
    const std::vector<PkString> args = collectArgs(argc, argv);

    if (hasOption(args, "--help") || hasOption(args, "-h")) {
        printHelp(std::cout);
        return EXIT_SUCCESS;
    }

    if (hasOption(args, "--graphs")) {
        // Don't change those lines to use dbgPigment derivatives, they need to be outputted
        // to stdout not stderr.
        std::cout << "full : show all the connection on the graph" << std::endl;
        std::cout << "bestpath : show the best path for a given transformation" << std::endl;
        return EXIT_SUCCESS;
    }

    PkString graphType = optionValue(args, "--graph");
    if (graphType.isEmpty()) {
        graphType = PkString("full");
    }
    PkString outputType = optionValue(args, "--output");
    if (outputType.isEmpty()) {
        outputType = PkString("ps");
    }

    PkString outputFileName;
    for (const PkString &a : args) {
        if (!a.startsWith("--") && a != "-h" && a != "-o") {
            outputFileName = a;
        }
    }
    if (outputFileName.isEmpty()) {
        errorPigment << "No output file name specified";
        printHelp(std::cerr);
        return EXIT_FAILURE;
    }

    // Generate the graph
    PkString dot;
    if (graphType == "full") {
        dot = FriendOfColorSpaceRegistry::toDot();
    } else if (graphType == "bestpath") {
        const PkString srcKey = optionValue(args, "--src-key");
        const PkString dstKey = optionValue(args, "--dst-key");
        if (srcKey.isEmpty() || dstKey.isEmpty()) {
            errorPigment << "src-key and dst-key must be specified for the graph bestpath";
            return EXIT_FAILURE;
        } else {
            dot = FriendOfColorSpaceRegistry::bestPathToDot(srcKey, dstKey);
        }
    } else {
        errorPigment << "Unknown graph type : " << graphType;
        return EXIT_FAILURE;
    }

    if (outputType == "dot") {
        std::ofstream out(outputFileName.PkToUtf8());
        if (!out) {
            return EXIT_FAILURE;
        }
        out << dot.PkToUtf8();
    } else if (outputType == "ps" || outputType == "svg") {
        char tmpTemplate[] = "/tmp/ccsgraphXXXXXX";
        const int fd = mkstemp(tmpTemplate);
        if (fd == -1) {
            return EXIT_FAILURE;
        }
        {
            std::ofstream out(tmpTemplate);
            out << dot.PkToUtf8();
        }
        close(fd);

        const std::string cmd = std::string("dot -T") + outputType.PkToUtf8()
                              + " " + tmpTemplate
                              + " -o " + outputFileName.PkToUtf8();
        const int rc = std::system(cmd.c_str());
        std::remove(tmpTemplate);

        if (rc != 0) {
            errorPigment << "An error has occurred when executing : '" << cmd
                         << "' the most likely cause is that 'dot' command is missing, and that you should install graphviz (from https://www.graphviz.org)";
            return EXIT_FAILURE;
        }
    } else {
        errorPigment << "Unknown output type : " << outputType;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
