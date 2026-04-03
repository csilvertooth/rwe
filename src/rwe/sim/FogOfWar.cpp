#include "FogOfWar.h"
#include <cmath>

namespace rwe
{
    std::vector<Point> computeVisibleCells(
        const Grid<unsigned char>& heightmap,
        int cx, int cy,
        int sightRadiusCells)
    {
        std::vector<Point> result;

        if (sightRadiusCells <= 0)
        {
            return result;
        }

        auto gridWidth = static_cast<int>(heightmap.getWidth());
        auto gridHeight = static_cast<int>(heightmap.getHeight());

        // Eye height: terrain height at the unit's position plus a small offset
        float eyeHeight = 0.0f;
        if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight)
        {
            eyeHeight = static_cast<float>(heightmap.get(cx, cy)) + 20.0f;
        }

        int radiusSq = sightRadiusCells * sightRadiusCells;

        // Scan all cells within the sight radius
        for (int dy = -sightRadiusCells; dy <= sightRadiusCells; ++dy)
        {
            for (int dx = -sightRadiusCells; dx <= sightRadiusCells; ++dx)
            {
                if (dx * dx + dy * dy > radiusSq)
                {
                    continue;
                }

                int tx = cx + dx;
                int ty = cy + dy;

                if (tx < 0 || tx >= gridWidth || ty < 0 || ty >= gridHeight)
                {
                    continue;
                }

                // The unit's own cell is always visible
                if (dx == 0 && dy == 0)
                {
                    result.emplace_back(tx, ty);
                    continue;
                }

                // Bresenham-style LOS ray march from (cx,cy) to (tx,ty)
                float targetHeight = static_cast<float>(heightmap.get(tx, ty));
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

                bool blocked = false;
                int steps = std::max(std::abs(dx), std::abs(dy));
                for (int step = 1; step < steps; ++step)
                {
                    float t = static_cast<float>(step) / static_cast<float>(steps);
                    int ix = cx + static_cast<int>(std::round(dx * t));
                    int iy = cy + static_cast<int>(std::round(dy * t));

                    if (ix < 0 || ix >= gridWidth || iy < 0 || iy >= gridHeight)
                    {
                        continue;
                    }

                    float intermediateHeight = static_cast<float>(heightmap.get(ix, iy));
                    float losHeight = eyeHeight + t * (targetHeight - eyeHeight);

                    if (intermediateHeight > losHeight)
                    {
                        blocked = true;
                        break;
                    }
                }

                if (!blocked)
                {
                    result.emplace_back(tx, ty);
                }
            }
        }

        return result;
    }

    void addVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells)
    {
        for (const auto& p : cells)
        {
            auto& cell = fog.grid.get(p.x, p.y);
            cell.visibleCount++;
            cell.explored = true;
        }
    }

    void removeVisibility(PlayerFogOfWar& fog, const std::vector<Point>& cells)
    {
        for (const auto& p : cells)
        {
            auto& cell = fog.grid.get(p.x, p.y);
            if (cell.visibleCount > 0)
            {
                cell.visibleCount--;
            }
        }
    }

    bool isCellVisible(const PlayerFogOfWar& fog, int x, int y)
    {
        if (x < 0 || x >= static_cast<int>(fog.grid.getWidth()) || y < 0 || y >= static_cast<int>(fog.grid.getHeight()))
        {
            return false;
        }
        return fog.grid.get(x, y).visibleCount > 0;
    }

    bool isCellExplored(const PlayerFogOfWar& fog, int x, int y)
    {
        if (x < 0 || x >= static_cast<int>(fog.grid.getWidth()) || y < 0 || y >= static_cast<int>(fog.grid.getHeight()))
        {
            return false;
        }
        return fog.grid.get(x, y).explored;
    }
}
