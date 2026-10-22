#include "Map.h"

Map::Map()
    : cells_(MAP_W * MAP_H, Terrain::Wall)
{
}

bool Map::inBounds(int x, int y) const
{
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

// Outside the map counts as wall, so code that looks at a cell's neighbors
// never has to check the edges first
Terrain Map::at(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return Terrain::Wall;
    }
    return cells_[y * MAP_W + x];
}

void Map::set(int x, int y, Terrain terrain)
{
    if (inBounds(x, y))
    {
        cells_[y * MAP_W + x] = terrain;
    }
}

void Map::fill(Terrain terrain)
{
    cells_.assign(cells_.size(), terrain);
}

// Turns every cell inside area into floor
void Map::carve(const SDL_Rect& area)
{
    for (int y = area.y; y < area.y + area.h; ++y)
    {
        for (int x = area.x; x < area.x + area.w; ++x)
        {
            set(x, y, Terrain::Floor);
        }
    }
}

bool Map::bordersFloor(int x, int y) const
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (at(x + dx, y + dy) == Terrain::Floor)
            {
                return true;
            }
        }
    }
    return false;
}
