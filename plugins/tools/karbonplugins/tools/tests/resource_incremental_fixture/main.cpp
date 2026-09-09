#include "KarbonToolsResources.h"

#include <cstdint>
#include <cstdio>

int main()
{
    const KarbonToolsResource resource = karbonCalligraphyIconPng();
    std::uint64_t digest = 14695981039346656037ULL;
    for (std::size_t i = 0; i < resource.size; ++i) {
        digest ^= resource.data[i];
        digest *= 1099511628211ULL;
    }
    std::printf("%zu:%llu\n", resource.size,
                static_cast<unsigned long long>(digest));
    return 0;
}
