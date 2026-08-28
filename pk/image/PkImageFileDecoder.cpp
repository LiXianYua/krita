#include "PkImageFileDecoder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_set>
#include <utility>

std::vector<PkImageFileDecoderHandler> pkNativeImageCodecHandlers();
PkImageFileDecoderHandler pkBmpImageCodecHandler();
PkImageFileDecoderHandler pkPnmImageCodecHandler();
PkImageFileDecoderHandler pkXbmImageCodecHandler();
PkImageFileDecoderHandler pkXpmImageCodecHandler();
PkImageFileDecoderHandler pkIcoImageCodecHandler();

namespace
{

constexpr std::uintmax_t kMaximumEncodedFileBytes = 512u * 1024u * 1024u;

struct HandlerRegistry
{
    std::shared_mutex mutex;
    std::vector<PkImageFileDecoderHandler> handlers;
};

HandlerRegistry &registry()
{
    static HandlerRegistry instance;
    return instance;
}

bool registerHandlerInternal(PkImageFileDecoderHandler handler)
{
    HandlerRegistry &state = registry();
    std::unique_lock<std::shared_mutex> lock(state.mutex);
    const auto duplicate = std::find_if(
        state.handlers.begin(), state.handlers.end(),
        [&handler](const PkImageFileDecoderHandler &existing) {
            return existing.name == handler.name;
        });
    if (duplicate != state.handlers.end()) {
        return false;
    }
    state.handlers.push_back(std::move(handler));
    return true;
}

void ensureBuiltInHandlers()
{
    static std::once_flag once;
    std::call_once(once, [] {
        try {
            std::vector<PkImageFileDecoderHandler> handlers = pkNativeImageCodecHandlers();
            handlers.push_back(pkBmpImageCodecHandler());
            handlers.push_back(pkPnmImageCodecHandler());
            handlers.push_back(pkXbmImageCodecHandler());
            handlers.push_back(pkXpmImageCodecHandler());
            handlers.push_back(pkIcoImageCodecHandler());
            for (PkImageFileDecoderHandler &handler : handlers) {
                registerHandlerInternal(std::move(handler));
            }
        } catch (...) {
            // Registry initialization is a best-effort boundary under allocation failure.
        }
    });
}

std::vector<PkImageFileDecoderHandler> orderedSnapshot()
{
    HandlerRegistry &state = registry();
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    std::vector<PkImageFileDecoderHandler> result = state.handlers;
    lock.unlock();
    std::stable_sort(result.begin(), result.end(),
                     [](const PkImageFileDecoderHandler &left,
                        const PkImageFileDecoderHandler &right) {
                         return left.priority > right.priority;
                     });
    return result;
}

std::string normalizedExtension(std::string extension)
{
    while (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension;
}

} // namespace

bool PkImageFileDecoder::registerHandler(PkImageFileDecoderHandler handler)
{
    ensureBuiltInHandlers();
    return registerHandlerInternal(std::move(handler));
}

PkImage PkImageFileDecoder::decode(const uint8_t *data, std::size_t size,
                                   const std::string &pathHint)
{
    ensureBuiltInHandlers();
    if (!data || size == 0) {
        return PkImage();
    }

    const std::vector<PkImageFileDecoderHandler> handlers = orderedSnapshot();
    for (const PkImageFileDecoderHandler &handler : handlers) {
        try {
            if (!handler.canDecode || !handler.decode ||
                !handler.canDecode(data, size, pathHint)) {
                continue;
            }
            PkImage image = handler.decode(data, size, pathHint);
            if (!image.isNull()) {
                return image;
            }
        } catch (...) {
            // A third-party or high-level adapter failure is local to that handler.
        }
    }
    return PkImage();
}

PkImage PkImageFileDecoder::load(const std::string &path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return PkImage();
    }

    const std::streamoff end = input.tellg();
    if (end <= 0) {
        return PkImage();
    }
    const std::uintmax_t length = static_cast<std::uintmax_t>(end);
    if (length > kMaximumEncodedFileBytes ||
        length > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        length > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return PkImage();
    }

    try {
        std::vector<uint8_t> bytes(static_cast<std::size_t>(length));
        input.seekg(0, std::ios::beg);
        if (!input ||
            !input.read(reinterpret_cast<char *>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()))) {
            return PkImage();
        }
        return decode(bytes.data(), bytes.size(), path);
    } catch (const std::bad_alloc &) {
        return PkImage();
    } catch (const std::length_error &) {
        return PkImage();
    }
}

std::vector<std::string> PkImageFileDecoder::supportedExtensions()
{
    ensureBuiltInHandlers();
    const std::vector<PkImageFileDecoderHandler> handlers = orderedSnapshot();
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const PkImageFileDecoderHandler &handler : handlers) {
        for (const std::string &extension : handler.extensions) {
            std::string normalized = normalizedExtension(extension);
            if (!normalized.empty() && seen.insert(normalized).second) {
                result.push_back(std::move(normalized));
            }
        }
    }
    return result;
}
