#include "CaveGen.h"

void CaveGen::fillRandomly(Map& map, Rng& rng)
{
    map.fill(Terrain::Wall);
    for (int y = 1; y < MAP_H - 1; ++y)
    {
        for (int x = 1; x < MAP_W - 1; ++x)
        {
            if (rng.roll(1, 100) > WALL_PERCENT)
            {
                map.set(x, y, Terrain::CaveFloor);
            }
        }
    }
}

// Counting the cell itself is what makes this the 4-5 rule: a wall stays a
// wall with 4 walls around it (5 in the block), and a floor cell becomes a
// wall with 5 around it. The edge isn't changed, so it stays wall
void CaveGen::smooth(Map& map)
{
    Map before = map;
    for (int y = 1; y < MAP_H - 1; ++y)
    {
        for (int x = 1; x < MAP_W - 1; ++x)
        {
            int walls = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (before.at(x + dx, y + dy) == Terrain::Wall)
                    {
                        ++walls;
                    }
                }
            }
            map.set(x, y, walls >= 5 ? Terrain::Wall : Terrain::CaveFloor);
        }
    }
}

// A flood fill from each floor cell that isn't in a cave yet finds the
// cave it's in: the fill spreads to every neighbor a way can lead through,
// the diagonals too, since a step can go that way, and through doors, so
// a vault is part of the cave it opens onto. Every cell it reaches is in
// the same cave. Then every cell outside the biggest cave becomes wall
int CaveGen::keepBiggestCave(Map& map)
{
    // Each cell's cave, numbered from 0, or -1 for none yet, and each
    // cave's size
    std::vector<int> caveOf(MAP_W * MAP_H, -1);
    std::vector<int> sizes;
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            if (!map.isPassable(x, y) || caveOf[y * MAP_W + x] != -1)
            {
                continue;
            }
            int cave = static_cast<int>(sizes.size());
            int size = 0;
            caveOf[y * MAP_W + x] = cave;
            std::vector<Point> toVisit = { { x, y } };
            while (!toVisit.empty())
            {
                Point cell = toVisit.back();
                toVisit.pop_back();
                ++size;
                for (Point step : DIRECTIONS)
                {
                    Point next = { cell.x + step.x, cell.y + step.y };
                    if (map.isPassable(next.x, next.y) &&
                        caveOf[next.y * MAP_W + next.x] == -1)
                    {
                        caveOf[next.y * MAP_W + next.x] = cave;
                        toVisit.push_back(next);
                    }
                }
            }
            sizes.push_back(size);
        }
    }
    if (sizes.empty())
    {
        return 0;
    }

    int biggest = 0;
    for (int cave = 1; cave < static_cast<int>(sizes.size()); ++cave)
    {
        if (sizes[cave] > sizes[biggest])
        {
            biggest = cave;
        }
    }
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            if (map.isPassable(x, y) && caveOf[y * MAP_W + x] != biggest)
            {
                map.set(x, y, Terrain::Wall);
            }
        }
    }
    return sizes[biggest];
}

std::vector<SDL_Rect> CaveGen::areasOf(const Map& map)
{
    std::vector<SDL_Rect> areas;
    for (int top = 0; top < MAP_H; top += AREA_H)
    {
        for (int left = 0; left < MAP_W; left += AREA_W)
        {
            SDL_Rect area = { left, top, SDL_min(AREA_W, MAP_W - left),
                              SDL_min(AREA_H, MAP_H - top) };
            int cells = 0;
            for (int y = area.y; y < area.y + area.h; ++y)
            {
                for (int x = area.x; x < area.x + area.w; ++x)
                {
                    if (map.isWalkable(x, y))
                    {
                        ++cells;
                    }
                }
            }
            if (cells >= MIN_AREA_CELLS)
            {
                areas.push_back(area);
            }
        }
    }
    return areas;
}

std::vector<SDL_Rect> CaveGen::generate(Map& map, Rng& rng)
{
    std::vector<SDL_Rect> areas;
    while (areas.size() < 2)
    {
        fillRandomly(map, rng);
        for (int pass = 0; pass < SMOOTHING_PASSES; ++pass)
        {
            smooth(map);
        }
        if (keepBiggestCave(map) >= MIN_CAVE_CELLS)
        {
            areas = areasOf(map);
        }
    }
    return areas;
}
