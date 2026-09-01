#ifndef KIS_PATH_RASTERIZER_P_H
#define KIS_PATH_RASTERIZER_P_H

#include <cstdint>
#include <vector>

#include <PkRect.h>

class PkPainterPath;
class PkPen;

namespace KisPathRasterizer {

struct CoverageMask {
    PkRect bounds;
    int stride = 0;
    std::vector<uint8_t> alpha;

    bool isEmpty() const noexcept
    {
        return stride <= 0 || alpha.empty() || bounds.isEmpty();
    }

    const uint8_t *scanLine(int absoluteY) const noexcept
    {
        if (absoluteY < bounds.y() || absoluteY > bounds.bottom()) {
            return nullptr;
        }
        return alpha.data() + (absoluteY - bounds.y()) * stride;
    }

    uint8_t coverageAt(int absoluteX, int absoluteY) const noexcept
    {
        if (absoluteX < bounds.x() || absoluteX > bounds.right()
            || absoluteY < bounds.y() || absoluteY > bounds.bottom()) {
            return 0;
        }
        return alpha[(absoluteY - bounds.y()) * stride + absoluteX - bounds.x()];
    }
};

CoverageMask rasterizeFill(const PkPainterPath &path,
                           const PkRect &clip,
                           bool antialiased);

CoverageMask rasterizeStroke(const PkPainterPath &path,
                             const PkPen &pen,
                             const PkRect &clip,
                             bool antialiased);

} // namespace KisPathRasterizer

#endif // KIS_PATH_RASTERIZER_P_H
