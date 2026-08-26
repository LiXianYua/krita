/*
 * SPDX-FileCopyrightText: 2026 S-09-b Task 1 palette handover fixture (Pk side)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Pk-side fixture for PaletteGeneratorConfig byte-compatibility.
 *
 * Reads the real-Qt 213-byte blob (hex, argv[1]), decodes it with the
 * production PaletteGeneratorConfig::fromByteArray, verifies every field
 * against the known test values, then re-serializes with toByteArray and
 * prints the identical BLOB/COLOR/ENABLED/GRADIENT/INBETWEEN/DIAGONAL lines
 * as the Qt fixture. The runner diffs the two outputs byte-for-byte:
 *   - the Pk re-serialized hex must equal the real-Qt blob hex, and
 *   - the Pk read-back semantic values must equal the Qt semantic text.
 */

#include "palettegeneratorconfig.h"

#include <PkColor.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

PkByteArray hexToBytes(const char *hex)
{
    std::string out;
    const int n = static_cast<int>(std::strlen(hex));
    for (int i = 0; i + 1 < n; i += 2) {
        const int hi = hexVal(hex[i]);
        const int lo = hexVal(hex[i + 1]);
        if (hi < 0 || lo < 0) return PkByteArray();
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return PkByteArray(out.data(), static_cast<int>(out.size()));
}

void printHex(const PkByteArray &bytes)
{
    for (int i = 0; i < bytes.size(); ++i) {
        std::printf("%02x", static_cast<unsigned int>(
            static_cast<unsigned char>(bytes.constData()[i])));
    }
}

int checkInt(const char *name, int got, int expected)
{
    if (got != expected) {
        std::fprintf(stderr, "MISMATCH %s got=%d expected=%d\n", name, got, expected);
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <hex-blob>\n", argv[0]);
        return 2;
    }

    const PkByteArray blob = hexToBytes(argv[1]);
    if (blob.size() != 213) {
        std::fprintf(stderr, "PK BLOB SIZE %d != 213\n", blob.size());
        return 1;
    }

    PaletteGeneratorConfig cfg;
    cfg.fromByteArray(blob);

    // Internal read-back verification against the known test values. The
    // runner's diff of the semantic text below is the primary evidence; this
    // is a belt-and-braces check with precise field names for diagnostics.
    int failures = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            char name[48];
            std::snprintf(name, sizeof(name), "color[%d][%d].r", i, j);
            failures += checkInt(name, cfg.colors[i][j].red(), i * 40);
            std::snprintf(name, sizeof(name), "color[%d][%d].g", i, j);
            failures += checkInt(name, cfg.colors[i][j].green(), j * 40);
            std::snprintf(name, sizeof(name), "color[%d][%d].b", i, j);
            failures += checkInt(name, cfg.colors[i][j].blue(), (i + j) * 20);
            std::snprintf(name, sizeof(name), "color[%d][%d].a", i, j);
            failures += checkInt(name, cfg.colors[i][j].alpha(), 255 - i * 10 - j);
            std::snprintf(name, sizeof(name), "colorsEnabled[%d][%d]", i, j);
            failures += checkInt(name, cfg.colorsEnabled[i][j] ? 1 : 0,
                                 ((i + j) % 2) != 0 ? 1 : 0);
        }
    }
    for (int i = 0; i < 3; ++i) {
        char name[24];
        std::snprintf(name, sizeof(name), "gradientSteps[%d]", i);
        failures += checkInt(name, cfg.gradientSteps[i], 2 * i + 3);
    }
    failures += checkInt("inbetweenRampSteps", cfg.inbetweenRampSteps, 9);
    failures += checkInt("diagonalGradients", cfg.diagonalGradients ? 1 : 0, 1);

    if (failures) {
        std::fprintf(stderr, "PALETTE_READ_SEMANTICS FAIL failures=%d\n", failures);
        return 1;
    }
    std::printf("PALETTE_READ_SEMANTICS PASS\n");

    // Re-serialize with the production writer and emit the same lines as the
    // Qt fixture so the runner can diff byte-for-byte.
    const PkByteArray reencoded = cfg.toByteArray();
    std::printf("BLOB ");
    printHex(reencoded);
    std::printf("\n");

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            std::printf("COLOR %d %d %d %d %d %d\n", i, j,
                        cfg.colors[i][j].red(), cfg.colors[i][j].green(),
                        cfg.colors[i][j].blue(), cfg.colors[i][j].alpha());
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            std::printf("ENABLED %d %d %d\n", i, j, cfg.colorsEnabled[i][j] ? 1 : 0);
        }
    }
    for (int i = 0; i < 3; ++i) {
        std::printf("GRADIENT %d %d\n", i, cfg.gradientSteps[i]);
    }
    std::printf("INBETWEEN %d\n", cfg.inbetweenRampSteps);
    std::printf("DIAGONAL %d\n", cfg.diagonalGradients ? 1 : 0);
    return 0;
}
