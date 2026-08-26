/*
 * SPDX-FileCopyrightText: 2026 S-09-b Task 1 palette handover fixture (real-Qt side)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Real-Qt fixture for PaletteGeneratorConfig byte-compatibility.
 *
 * This TU uses the genuine Qt QColor/QDataStream (never the Pk headers) and
 * mirrors the exact serialization order of the production
 * PaletteGeneratorConfig::toByteArray:
 *
 *   version int32(0) -> 16 colors -> 16 bools -> 3 gradientSteps int32
 *   -> inbetweenRampSteps int32 -> diagonalGradients bool
 *
 * under QDataStream::Qt_4_6 + BigEndian, filling non-default test values:
 *
 *   color[i][j]      = QColor(i*40, j*40, (i+j)*20, 255 - i*10 - j)
 *   colorsEnabled    = ((i+j) % 2) != 0
 *   gradientSteps    = {3, 5, 7}
 *   inbetweenRampSteps = 9
 *   diagonalGradients  = true
 *
 * Output lines (both fixtures emit the identical format so the runner can diff):
 *   BLOB <hex>                -- the 213-byte blob, hex-encoded
 *   COLOR <i> <j> <r> <g> <b> <a>
 *   ENABLED <i> <j> <0|1>
 *   GRADIENT <i> <v>
 *   INBETWEEN <v>
 *   DIAGONAL <0|1>
 */

#include <QByteArray>
#include <QColor>
#include <QDataStream>
#include <QIODevice>

#include <cstdio>

namespace {

struct QtPaletteGeneratorConfig
{
    QColor colors[4][4];
    bool colorsEnabled[4][4] = {};
    int gradientSteps[3] = {};
    int inbetweenRampSteps = 0;
    bool diagonalGradients = false;
};

void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
}

QByteArray toByteArray(const QtPaletteGeneratorConfig &cfg)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << qint32(0); // version
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            stream << cfg.colors[i][j];
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            stream << cfg.colorsEnabled[i][j];
        }
    }
    for (int i = 0; i < 3; ++i) {
        stream << qint32(cfg.gradientSteps[i]);
    }
    stream << qint32(cfg.inbetweenRampSteps);
    stream << cfg.diagonalGradients;
    return bytes;
}

} // namespace

int main()
{
    QtPaletteGeneratorConfig cfg;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            cfg.colors[i][j] = QColor(i * 40, j * 40, (i + j) * 20, 255 - i * 10 - j);
            cfg.colorsEnabled[i][j] = ((i + j) % 2) != 0;
        }
    }
    cfg.gradientSteps[0] = 3;
    cfg.gradientSteps[1] = 5;
    cfg.gradientSteps[2] = 7;
    cfg.inbetweenRampSteps = 9;
    cfg.diagonalGradients = true;

    const QByteArray bytes = toByteArray(cfg);
    if (bytes.size() != 213) {
        std::fprintf(stderr, "QT BLOB SIZE %d != 213\n", static_cast<int>(bytes.size()));
        return 1;
    }

    std::printf("BLOB ");
    printHex(bytes);
    std::printf("\n");

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const QColor &c = cfg.colors[i][j];
            std::printf("COLOR %d %d %d %d %d %d\n", i, j,
                        c.red(), c.green(), c.blue(), c.alpha());
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
