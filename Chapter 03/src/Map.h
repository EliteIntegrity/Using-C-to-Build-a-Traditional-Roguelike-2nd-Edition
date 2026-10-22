#pragma once
#include <SDL3/SDL.h>
#include <vector>   // std::vector, for the cells
#include "Common.h"

// What a cell of the map is made of
enum class Terrain
{
    Wall,
    Floor,
};

// One floor of the dungeon: a grid of MAP_W by MAP_H cells, kept row by row
// in a single vector
class Map
{
public:
    Map();

    bool inBounds(int x, int y) const;
    Terrain at(int x, int y) const;
    void set(int x, int y, Terrain terrain);
    void fill(Terrain terrain);
    void carve(const SDL_Rect& area);

    // True for a wall with floor beside it, diagonals included. Those are
    // the only walls worth drawing. The rest is solid rock nobody can see
    bool bordersFloor(int x, int y) const;

private:
    std::vector<Terrain> cells_;
};
