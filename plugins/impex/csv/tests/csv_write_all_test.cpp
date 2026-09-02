#include "../csv_write_all.h"

#include <cstring>
#include <iostream>
#include <string>

class ChunkStream final : public PkStream
{
public:
    ChunkStream() { open(WriteOnly); }
    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }
    const std::string &data() const { return m_data; }

protected:
    pk_int64 readData(char *, pk_int64) override { return -1; }
    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const pk_int64 count = maxSize > 2 ? 2 : maxSize;
        m_data.append(data, static_cast<std::size_t>(count));
        return count;
    }

private:
    std::string m_data;
};

int main()
{
    ChunkStream stream;
    const std::string expected = "short writes are legal";
    if (!CsvPrivate::writeAll(&stream, expected) || stream.data() != expected) {
        std::cerr << "writeAll must retry legal short writes\n";
        return 1;
    }
    return 0;
}
