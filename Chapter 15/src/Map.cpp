#include "Map.h"

Map::Map()
    : cells_(MAP_W * MAP_H)
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
    return cells_[y * MAP_W + x].terrain;
}

bool Map::isWalkable(int x, int y) const
{
    Terrain terrain = at(x, y);
    return terrain == Terrain::Floor || terrain == Terrain::StairsDown;
}

void Map::set(int x, int y, Terrain terrain)
{
    if (inBounds(x, y))
    {
        cells_[y * MAP_W + x].terrain = terrain;
    }
}

// Makes every cell terrain, unseen and unexplored, ready for a new dungeon
void Map::fill(Terrain terrain)
{
    Cell cell;
    cell.terrain = terrain;
    cells_.assign(cells_.size(), cell);
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
            if (isWalkable(x + dx, y + dy))
            {
                return true;
            }
        }
    }
    return false;
}

bool Map::blocksSight(int x, int y) const
{
    return at(x, y) == Terrain::Wall;
}

bool Map::isVisible(int x, int y) const
{
    return inBounds(x, y) && cells_[y * MAP_W + x].visible;
}

bool Map::isExplored(int x, int y) const
{
    return inBounds(x, y) && cells_[y * MAP_W + x].explored;
}

// A cell the player can see is one they've explored, too, and it stays
// explored when it goes out of sight
void Map::setVisible(int x, int y)
{
    if (inBounds(x, y))
    {
        cells_[y * MAP_W + x].visible = true;
        cells_[y * MAP_W + x].explored = true;
    }
}

// A cell the player knows of without having seen it, as magic mapping
// shows them
void Map::setExplored(int x, int y)
{
    if (inBounds(x, y))
    {
        cells_[y * MAP_W + x].explored = true;
    }
}

void Map::clearVisible()
{
    for (Cell& cell : cells_)
    {
        cell.visible = false;
    }
}
