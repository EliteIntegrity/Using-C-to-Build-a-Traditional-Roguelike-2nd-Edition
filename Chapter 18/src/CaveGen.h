#pragma once
#include <SDL3/SDL.h>
#include <vector>   // std::vector, for the areas
#include "Map.h"
#include "Rng.h"

// Builds a floor of caves with a cellular automaton. The map starts as
// random wall and floor, and is then smoothed, again and again, by one rule
// that every cell follows at once, until it looks like caves
namespace CaveGen
{
    // How much of the map starts as wall, in percent, and how many times
    // it's smoothed (both *tune*)
    constexpr int WALL_PERCENT = 48;
    constexpr int SMOOTHING_PASSES = 4;

    // The fewest cells the biggest cave may have. A floor whose biggest
    // cave is smaller is made again (*tune*)
    constexpr int MIN_CAVE_CELLS = 600;

    // Monsters and items are placed in areas, as they are in rooms: the
    // squares of an AREA_W by AREA_H grid over the map that hold at least
    // MIN_AREA_CELLS of the cave
    constexpr int AREA_W = 16;
    constexpr int AREA_H = 12;
    constexpr int MIN_AREA_CELLS = 40;

    // Makes each cell inside the map's edge a wall WALL_PERCENT of the
    // time, and cave floor the rest. The edge is all wall
    void fillRandomly(Map& map, Rng& rng);

    // One smoothing pass, by the 4-5 rule: a cell becomes wall if 5 or
    // more of the 9 cells in the 3 x 3 block around it, itself included,
    // are walls, and floor if 4 or fewer are. Every cell is decided from
    // the map as it was before the pass, never from one half changed
    void smooth(Map& map);

    // Keeps the biggest cave: the floor cells that can all be walked to
    // from one another. Every other floor cell becomes wall. Returns how
    // many cells it kept
    int keepBiggestCave(Map& map);

    // The areas that hold enough of the cave, from the top left, row by
    // row, as RoomGen gives its rooms
    std::vector<SDL_Rect> areasOf(const Map& map);

    // A whole floor of caves: filled, smoothed, and its biggest cave kept.
    // It's made again if that cave is too small, or too squeezed to give
    // two areas, one for the player to start in and one for monsters.
    // Returns the areas; the player starts in the first
    std::vector<SDL_Rect> generate(Map& map, Rng& rng);
}
