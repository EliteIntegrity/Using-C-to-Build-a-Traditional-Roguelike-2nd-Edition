#pragma once
#include <SDL3/SDL.h>
#include <random>   // std::mt19937, for the dungeon's dice
#include <vector>   // std::vector, for the rooms
#include "Common.h"
#include "Map.h"

// Builds a floor of rooms by binary space partitioning: the map is cut in
// two, each half is cut in two again, and so on, and every piece that isn't
// cut any further gets a room
namespace RoomGen
{
    std::vector<SDL_Rect> generate(Map& map, std::mt19937& rng);
    Point centerOf(const SDL_Rect& room);
}
