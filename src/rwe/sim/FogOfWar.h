#pragma once

#include <rwe/grid/Grid.h>
#include <rwe/grid/Point.h>
#include <rwe/sim/PlayerId.h>
#include <vector>

namespace rwe
{
    struct VisibilityCell
    {
        /** Number of units currently illuminating this cell. */
        uint8_t visibleCount{0};
        /** True once any unit has ever seen this cell. */
        bool explored{false};
    };

    struct PlayerFogOfWar
    {
        Grid<VisibilityCell> grid;

        PlayerFogOfWar(unsigned int width, unsigned int height)
            : grid(width, height, VisibilityCell())
        {
        }
    };

    /** Compute the set of heightmap cells visible from (cx, cy) with given sight radius in cells.
     *  If trueLOS is false (default), uses a simple circular radius without terrain blocking.
     *  If trueLOS is true, uses Bresenham ray-marching against terrain heights. */
    std::vector<Point> computeVisibleCells(
        const Grid<unsigned char>& heightmap,
        int cx, int cy,
        int sightRadiusCells,
        bool trueLOS = false);

    void addVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells);
    void removeVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells);

    bool isCellVisible(const PlayerFogOfWar& fog, int x, int y);
    bool isCellExplored(const PlayerFogOfWar& fog, int x, int y);

    /** Per-player radar detection grid. Simpler than fog — just tracks detection count. */
    struct PlayerRadarMap
    {
        Grid<uint8_t> grid;

        PlayerRadarMap(unsigned int width, unsigned int height)
            : grid(width, height, 0)
        {
        }
    };

    /** Compute cells within a simple circular radius (no LOS blocking — radar goes through terrain). */
    std::vector<Point> computeRadarCells(
        unsigned int gridWidth, unsigned int gridHeight,
        int cx, int cy,
        int radiusCells);

    bool isCellRadarDetected(const PlayerRadarMap& radar, int x, int y);
}
