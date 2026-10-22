#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for where it last saw the player
#include "Entity.h"

// What every monster of one kind has in common. Chapter 16 moves these
// into a text file, so new kinds need no recompiling
struct MonsterDef
{
    const char* name;
    char glyph;
    SDL_Color color;
    const char* tile;   // its picture's name in tiles.txt
    int sight;          // how far it can see, in cells
    int hp;             // its health when it's made
    int minDamage;      // each of its blows does minDamage to maxDamage,
    int maxDamage;      // before the target's defense
    int defense;
};

// Every kind of monster there is, so far. The inline makes it one table
// shared by every file that includes this header, not a copy in each
inline constexpr MonsterDef MONSTER_DEFS[] = {
    // name, glyph, color, tile,
    //     sight, hp, damage from and to, defense
    { "giant white rat", 'r', { 215, 215, 205, 255 }, "giant-white-rat",
      6, 4, 1, 3, 0 },
    { "jackal", 'j', { 205, 150, 80, 255 }, "jackal",
      7, 5, 1, 3, 0 },
    { "kobold", 'k', { 110, 185, 95, 255 }, "kobold",
      6, 7, 2, 4, 1 },
    { "goblin", 'g', { 160, 205, 60, 255 }, "goblin",
      7, 9, 2, 5, 1 },
};

// One monster in the dungeon: an Entity, the kind of monster it is, and
// where it last saw the player, if it has
struct Monster : Entity
{
    const MonsterDef* def = nullptr;
    std::optional<Point> lastSeen;
};
