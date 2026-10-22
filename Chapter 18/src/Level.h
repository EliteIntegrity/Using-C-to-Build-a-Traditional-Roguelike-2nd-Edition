#pragma once
#include <vector>   // std::vector, for the monsters and the items
#include "Common.h"
#include "Item.h"
#include "Map.h"
#include "Monster.h"

// One floor of the dungeon: how deep it is, its map, the monsters and the
// items on it, and where its stairs down are. Only the floor the player is
// on is kept, since the way down is one way
struct Level
{
    int depth = 1;   // 1 for the first floor
    Map map;
    std::vector<Monster> monsters;
    std::vector<FloorItem> items;
    Point stairs;
};
