#include <QByteArray>
#include <QDataStream>
#include <QHash>
#include <QString>
#include <QVariant>

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

namespace {

std::string field(const std::string &line, const std::string &name)
{
    const std::string prefix = name + '=';
    const std::size_t begin = line.find(prefix);
    if (begin == std::string::npos) return {};
    const std::size_t valueBegin = begin + prefix.size();
    const std::size_t end = line.find(' ', valueBegin);
    return line.substr(valueBegin, end == std::string::npos ? end : end - valueBegin);
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;

    const QVariantHash expected{{QStringLiteral("a"), QVariant(3)},
                                {QStringLiteral("b"), QVariant(QStringLiteral("two"))},
                                {QStringLiteral("c"), QVariant(false)}};
    std::map<std::string, int> tags;
    int total = 0;
    int mismatch = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("PKHASH ", 0) != 0) continue;
        const std::string versionName = field(line, "version");
        const std::string orderName = field(line, "order");
        const QByteArray bytes = QByteArray::fromHex(QByteArray::fromStdString(field(line, "bytes")));
        QDataStream stream(bytes);
        stream.setVersion(versionName == "qt_4_6" ? QDataStream::Qt_4_6 : QDataStream::Qt_5_15);
        stream.setByteOrder(orderName == "little" ? QDataStream::LittleEndian : QDataStream::BigEndian);
        QVariant decoded;
        stream >> decoded;
        const bool ok = stream.status() == QDataStream::Ok
                     && decoded.userType() == QMetaType::QVariantHash
                     && decoded.toHash() == expected;
        ++total;
        if (!ok) {
            ++mismatch;
            ++tags[versionName + '-' + orderName];
        }
    }
    if (total != 4) {
        ++mismatch;
        ++tags["missing-pk-hash-cases"];
    }
    std::printf("DIFF total=%d mismatch=%d\n", total, mismatch);
    for (const auto &tag : tags) std::printf("DIFFTAG wire-hash %s %d\n", tag.first.c_str(), tag.second);
    return mismatch == 0 ? 0 : 1;
}
