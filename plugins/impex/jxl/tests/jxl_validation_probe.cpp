#include "../jxl_validation.h"
#include "../kis_jpegxl_output_processor.h"

#include <PkStream.h>

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
class ShortWriteStream final : public PkStream
{
public:
    ShortWriteStream() { open(PkStream::WriteOnly); }

protected:
    pk_int64 readData(char *, pk_int64) override { return -1; }
    pk_int64 writeData(const char *, pk_int64 maxSize) override
    {
        return maxSize > 0 ? maxSize - 1 : maxSize;
    }
};

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    std::size_t bytes = 0;
    require(jxlCheckedImageBufferSize(17, 13, 4, 2, bytes) && bytes == 1768,
            "valid JXL dimensions must produce the exact interleaved byte count");
    require(!jxlCheckedImageBufferSize(0, 13, 4, 2, bytes),
            "zero-width JXL images must be rejected");
    require(!jxlCheckedImageBufferSize(std::numeric_limits<std::size_t>::max(), 2, 4, 2, bytes),
            "JXL decoded image allocation multiplication must reject overflow");
    require(!jxlInputMayGrow(64, 63),
            "a JXL reader must not wait for bytes beyond a known truncated input");
    ShortWriteStream stream;
    JXLExpTool::JxlOutputProcessor processor(&stream);
    std::size_t requested = 8;
    auto *buffer = static_cast<unsigned char *>(
        JXLExpTool::JxlOutputProcessor::getBuffer(&processor, &requested));
    require(buffer && requested == 8, "JXL output callback must provide its real buffer");
    JXLExpTool::JxlOutputProcessor::releaseBuffer(&processor, requested);
    require(!processor.ok(), "JXL output callback must retain a short-write failure");
    return 0;
}
