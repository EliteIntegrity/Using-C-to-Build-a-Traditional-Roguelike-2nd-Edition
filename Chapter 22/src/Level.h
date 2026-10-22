#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for a vault, which a floor may have
#include <vector>     // std::vector, for the monsters and the items
#include "Common.h"
#include "Item.h"
#include "Map.h"
#include "Monster.h"
#include "Trap.h"

// One floor of the dungeon: how deep it is, its map, the monsters, the
// items and the traps on it, and where its stairs down are. Only the floor
// the player is on is kept, since the way down is one way
struct Level
{
    int depth = 1;   // 1 for the first floor
    Map map;
    std::vector<Monster> monsters;
    std::vector<FloorItem> items;
    std::vector<Trap> traps;
    Point stairs;

    // Where the floor's vault is, if it has one, so nothing else is put
    // in it while the floor is made. It isn't saved: by then, it's done
    // its job
    std::optional<SDL_Rect> vault;
};
