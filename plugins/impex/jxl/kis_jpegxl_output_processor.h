#ifndef KIS_JPEGXL_OUTPUT_PROCESSOR_H
#define KIS_JPEGXL_OUTPUT_PROCESSOR_H

#include <PkAuxTypes.h>
#include <PkStream.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include <jxl/encode.h>
#include <jxl/version.h>

namespace JXLExpTool
{
class JxlOutputProcessor
{
public:
    explicit JxlOutputProcessor(PkStream *io)
        : m_outDevice(io)
    {
    }

#if JPEGXL_NUMERIC_VERSION >= JPEGXL_COMPUTE_NUMERIC_VERSION(0, 10, 1)
    JxlEncoderOutputProcessor getOutputProcessor()
    {
        return JxlEncoderOutputProcessor{this, getBuffer, releaseBuffer, seek, setFinalizedPosition};
    }
#endif

    bool ok() const { return !m_failed; }

    static void *getBuffer(void *opaque, std::size_t *size)
    {
        auto *self = static_cast<JxlOutputProcessor *>(opaque);
        if (!self || !size || self->m_failed) {
            if (size) *size = 0;
            return nullptr;
        }
        *size = std::min<std::size_t>(*size, 1u << 16);
        if (*size == 0 || *size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            self->m_failed = true;
            *size = 0;
            return nullptr;
        }
        if (static_cast<std::size_t>(self->m_output.size()) < *size) {
            self->m_output.resize(static_cast<int>(*size));
        }
        return self->m_output.data();
    }

    static void releaseBuffer(void *opaque, std::size_t writtenBytes)
    {
        auto *self = static_cast<JxlOutputProcessor *>(opaque);
        if (!self || self->m_failed) return;
        if (!self->m_outDevice || !self->m_outDevice->isWritable() ||
            writtenBytes > static_cast<std::size_t>(self->m_output.size()) ||
            writtenBytes > static_cast<std::size_t>(std::numeric_limits<PkStream::pk_int64>::max()) ||
            self->m_outDevice->write(self->m_output.data(),
                                     static_cast<PkStream::pk_int64>(writtenBytes)) !=
                static_cast<PkStream::pk_int64>(writtenBytes)) {
            self->m_failed = true;
        }
        self->m_output.resize(0);
    }

    static void seek(void *opaque, std::uint64_t position)
    {
        auto *self = static_cast<JxlOutputProcessor *>(opaque);
        if (!self || self->m_failed) return;
        if (!self->m_outDevice || !self->m_outDevice->isOpen() ||
            position < self->m_finalizedPosition ||
            position > static_cast<std::uint64_t>(std::numeric_limits<PkStream::pk_int64>::max()) ||
            !self->m_outDevice->seek(static_cast<PkStream::pk_int64>(position))) {
            self->m_failed = true;
        }
    }

    static void setFinalizedPosition(void *opaque, std::uint64_t position)
    {
        auto *self = static_cast<JxlOutputProcessor *>(opaque);
        if (!self || self->m_failed) return;
        if (!self->m_outDevice || position < self->m_finalizedPosition ||
            position > static_cast<std::uint64_t>(std::numeric_limits<PkStream::pk_int64>::max()) ||
            self->m_outDevice->size() < 0 ||
            position > static_cast<std::uint64_t>(self->m_outDevice->size())) {
            self->m_failed = true;
            return;
        }
        self->m_finalizedPosition = position;
    }

private:
    PkStream *m_outDevice = nullptr;
    PkByteArray m_output;
    std::uint64_t m_finalizedPosition = 0;
    bool m_failed = false;
};
} // namespace JXLExpTool

#endif
