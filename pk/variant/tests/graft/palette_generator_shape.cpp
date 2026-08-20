#include "PkColor.h"
#include "PkDataStream.h"

#include <cstdio>

struct PaletteGeneratorConfigShape
{
    PkColor colors[4][4];
    bool colorsEnabled[4][4]{};
    int gradientSteps[3]{};
    int inbetweenRampSteps = 0;
    bool diagonalGradients = false;

    PkByteArray toByteArray() const
    {
        PkByteArray bytes;
        PkDataStream stream(&bytes, PkStream::WriteOnly);
        stream.setVersion(PkDataStream::Qt_4_6);
        stream.setByteOrder(PkDataStream::BigEndian);
        stream << std::int32_t(0);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) stream << colors[i][j];
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) stream << colorsEnabled[i][j];
        for (int value : gradientSteps) stream << std::int32_t(value);
        stream << std::int32_t(inbetweenRampSteps) << diagonalGradients;
        return bytes;
    }

    bool fromByteArray(const PkByteArray &bytes)
    {
        PkDataStream stream(bytes);
        stream.setVersion(PkDataStream::Qt_4_6);
        stream.setByteOrder(PkDataStream::BigEndian);
        std::int32_t version = -1;
        stream >> version;
        if (version != 0) return false;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) stream >> colors[i][j];
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) stream >> colorsEnabled[i][j];
        for (int &value : gradientSteps) {
            std::int32_t decoded = 0;
            stream >> decoded;
            value = decoded;
        }
        std::int32_t rampSteps = 0;
        stream >> rampSteps >> diagonalGradients;
        inbetweenRampSteps = rampSteps;
        return stream.status() == PkDataStream::Ok;
    }
};

int main()
{
    PaletteGeneratorConfigShape source;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            source.colors[i][j] = PkColor(i * 40, j * 40, (i + j) * 20, 255 - i * 10 - j);
            source.colorsEnabled[i][j] = ((i + j) % 2) != 0;
        }
    }
    source.gradientSteps[0] = 3;
    source.gradientSteps[1] = 5;
    source.gradientSteps[2] = 7;
    source.inbetweenRampSteps = 9;
    source.diagonalGradients = true;

    const PkByteArray bytes = source.toByteArray();
    if (bytes.size() != 213) return 1;
    PaletteGeneratorConfigShape decoded;
    if (!decoded.fromByteArray(bytes)) return 2;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (decoded.colors[i][j].wireState() != source.colors[i][j].wireState()) return 3;
            if (decoded.colorsEnabled[i][j] != source.colorsEnabled[i][j]) return 4;
        }
    }
    for (int i = 0; i < 3; ++i)
        if (decoded.gradientSteps[i] != source.gradientSteps[i]) return 5;
    if (decoded.inbetweenRampSteps != source.inbetweenRampSteps
        || decoded.diagonalGradients != source.diagonalGradients) return 6;
    std::puts("GRAFT PASS PaletteGeneratorConfig QDataStream Qt_4_6 shape");
}
