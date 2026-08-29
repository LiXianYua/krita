/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILE_DATA_SWAPPER_P_H_
#define KIS_TILE_DATA_SWAPPER_P_H_

#include <cstdint>

#include "kis_image_config.h"
#include "tiles3/kis_tile_data.h"


/*
       Limits Diagram
  +------------------------+  <-- out of memory
  |                        |
  |                        |
  |                        |
  |## emergencyThreshold ##|  <-- new tiles are not created
  |                        |      until we free some memory
  |                        |
  |== hardLimitThreshold ==|  <-- the swapper thread starts
  |........................|      swapping out working (actually
  |........................|      needed) tiles until the level
  |........................|      reaches hardLimit level.
  |........................|
  |=====  hardLimit  ======|  <-- the swapper stops swapping
  |                        |      out needed tiles
  |                        |
  :                        :
  |                        |
  |                        |
  |== softLimitThreshold ==|  <-- the swapper starts swapping
  |........................|      out memento tiles (those, which
  |........................|      store undo information)
  |........................|
  |=====  softLimit  ======|  <-- the swapper stops swapping
  |                        |      out memento tiles
  |                        |
  :                        :
  |                        |
  +------------------------+  <-- 0 MiB

 */


class KisStoreLimits
{
public:
    KisStoreLimits() {
        KisImageConfig config(true);

        m_emergencyThreshold = MiB_TO_METRIC(config.tilesHardLimit());

        m_hardLimitThreshold = m_emergencyThreshold - (m_emergencyThreshold / 8);
        m_hardLimit = m_hardLimitThreshold - (m_hardLimitThreshold / 8);

        m_softLimitThreshold = qBound(0, MiB_TO_METRIC(config.tilesSoftLimit()), m_hardLimitThreshold);
        m_softLimit = m_softLimitThreshold - m_softLimitThreshold / 8;
    }

    /**
     * These methods return the "metric" of the size
     */

    inline std::int32_t emergencyThreshold() {
        return m_emergencyThreshold;
    }

    inline std::int32_t hardLimitThreshold() {
        return m_hardLimitThreshold;
    }

    inline std::int32_t hardLimit() {
        return m_hardLimit;
    }

    inline std::int32_t softLimitThreshold() {
        return m_softLimitThreshold;
    }

    inline std::int32_t softLimit() {
        return m_softLimit;
    }

private:
    std::int32_t m_emergencyThreshold;
    std::int32_t m_hardLimitThreshold;
    std::int32_t m_hardLimit;
    std::int32_t m_softLimitThreshold;
    std::int32_t m_softLimit;
};




#endif /* KIS_TILE_DATA_SWAPPER_P_H_ */
