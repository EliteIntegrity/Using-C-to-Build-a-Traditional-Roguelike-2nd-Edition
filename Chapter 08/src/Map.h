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

    // Sight: what stops it, which cells the player can see right now, and
    // which they've ever seen
    bool blocksSight(int x, int y) const;
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    void setVisible(int x, int y);
    void clearVisible();

private:
    // What each cell is made of, whether the player can see it, and
    // whether they've seen it before
    struct Cell
    {
        Terrain terrain = Terrain::Wall;
        bool visible = false;
        bool explored = false;
    };

    std::vector<Cell> cells_;
};
