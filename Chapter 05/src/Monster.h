#pragma once
#include <SDL3/SDL.h>
#include "Common.h"

// What every monster of one kind has in common. Chapter 16 moves these
// into a text file, so new kinds need no recompiling
struct MonsterDef
{
    const char* name;
    char glyph;
    SDL_Color color;
    int sight;   // how far it can see, in cells
};

// Every kind of monster there is, so far. The inline makes it one table
// shared by every file that includes this header, not a copy in each
inline constexpr MonsterDef MONSTER_DEFS[] = {
    { "giant white rat", 'r', { 215, 215, 205, 255 }, 6 },
    { "jackal", 'j', { 205, 150, 80, 255 }, 7 },
    { "kobold", 'k', { 110, 185, 95, 255 }, 6 },
    { "goblin", 'g', { 160, 205, 60, 255 }, 7 },
};

// One monster in the dungeon: where it is, and what kind it is
struct Monster
{
    Point pos;
    const MonsterDef* def = nullptr;
};
