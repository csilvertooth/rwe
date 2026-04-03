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

    /** Compute the set of heightmap cells visible from (cx, cy) with given sight radius in cells. */
    std::vector<Point> computeVisibleCells(
        const Grid<unsigned char>& heightmap,
        int cx, int cy,
        int sightRadiusCells);

    void addVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells);
    void removeVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells);

    bool isCellVisible(const PlayerFogOfWar& fog, int x, int y);
    bool isCellExplored(const PlayerFogOfWar& fog, int x, int y);
}
