#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for what a monster is heading for
#include "Entity.h"

// What makes a kind of monster behave differently. Each trait is one bit,
// so a kind can have several, joined with |, as in NEVER_SLEEPS | ERRATIC
namespace Trait
{
    constexpr unsigned int NEVER_SLEEPS = 1 << 0;

    // Half its moves are random
    constexpr unsigned int ERRATIC = 1 << 1;

    // It never moves, and a shrieker calls for help while it sees the
    // player
    constexpr unsigned int STATIONARY = 1 << 2;
    constexpr unsigned int SHRIEKS = 1 << 3;

    // It acts every other turn; it fights to the death
    constexpr unsigned int SLOW = 1 << 4;
    constexpr unsigned int NEVER_FLEES = 1 << 5;

    // It heals every turn; its blows heal it by half the damage they do
    constexpr unsigned int REGENERATES = 1 << 6;
    constexpr unsigned int DRAINS = 1 << 7;
}

// What every monster of one kind has in common. Chapter 16 moves these
// into a text file, so new kinds need no recompiling
struct MonsterDef
{
    const char* name;
    char glyph;
    SDL_Color color;
    const char* tile;   // its picture's name in tiles.txt
    int minFloor;       // the floors it's found on, from minFloor to
    int maxFloor;       // maxFloor
    int sight;          // how far it can see, in cells
    int hp;             // its health when it's made
    int minDamage;      // each of its blows does minDamage to maxDamage,
    int maxDamage;      // before the target's defense
    int defense;
    unsigned int traits = 0;
    int venom = 0;      // a hit poisons for this many turns
    int pack = 1;       // how many of it are found together

    // True if the kind has the trait. & keeps only the bits both have
    bool has(unsigned int trait) const
    {
        return (traits & trait) != 0;
    }
};

// Every kind of monster there is, so far. The inline makes it one table
// shared by every file that includes this header, not a copy in each.
// What a kind doesn't need from the end of a row, it can leave out
inline constexpr MonsterDef MONSTER_DEFS[] = {
    // name, glyph, color, tile, floors from and to,
    //     sight, hp, damage from and to, defense, traits, venom, pack
    { "giant white rat", 'r', { 215, 215, 205, 255 }, "giant-white-rat",
      1, 3, 6, 4, 1, 3, 0 },
    { "fruit bat", 'b', { 205, 120, 90, 255 }, "fruit-bat",
      1, 3, 6, 3, 1, 2, 0, Trait::NEVER_SLEEPS | Trait::ERRATIC },
    { "jackal", 'j', { 205, 150, 80, 255 }, "jackal",
      1, 3, 7, 5, 1, 3, 0, 0, 0, 3 },
    { "kobold", 'k', { 110, 185, 95, 255 }, "kobold",
      1, 4, 6, 7, 2, 4, 1 },
    { "shrieker", ',', { 210, 130, 220, 255 }, "shrieker",
      1, 5, 5, 4, 0, 0, 0, Trait::STATIONARY | Trait::SHRIEKS },
    { "cave spider", 'S', { 150, 130, 175, 255 }, "cave-spider",
      2, 4, 5, 5, 1, 3, 0, 0, 4 },
    { "goblin", 'g', { 160, 205, 60, 255 }, "goblin",
      2, 5, 7, 9, 2, 5, 1 },
    { "cave orc", 'o', { 205, 125, 60, 255 }, "cave-orc",
      4, 6, 7, 16, 4, 8, 3, 0, 0, 2 },
    { "wolf", 'C', { 175, 175, 185, 255 }, "wolf",
      4, 6, 8, 14, 3, 7, 1, 0, 0, 3 },
    { "zombified kobold", 'z', { 125, 155, 110, 255 }, "zombified-kobold",
      4, 6, 5, 20, 3, 6, 2, Trait::SLOW | Trait::NEVER_FLEES },
    { "giant spider", 'S', { 185, 70, 70, 255 }, "giant-spider",
      5, 7, 6, 16, 3, 7, 2, 0, 6 },
    { "skeleton orc", 's', { 230, 230, 215, 255 }, "skeleton-orc",
      5, 6, 6, 18, 4, 8, 4, Trait::NEVER_FLEES | Trait::NEVER_SLEEPS },
    { "ogre", 'O', { 205, 165, 110, 255 }, "ogre",
      7, 9, 7, 26, 5, 10, 2 },
    { "forest troll", 'T', { 105, 165, 95, 255 }, "forest-troll",
      7, 9, 7, 22, 4, 8, 2, Trait::REGENERATES },
    { "shadow hound", 'Z', { 130, 95, 180, 255 }, "shadow-hound",
      7, 9, 9, 14, 3, 7, 2, 0, 0, 3 },
    { "vampire", 'V', { 205, 45, 65, 255 }, "vampire",
      8, 9, 8, 24, 4, 8, 3, Trait::DRAINS },
};

// What a monster is doing, which decides what it does on its turn
enum class MonsterState
{
    Asleep,      // doesn't move, until something wakes it
    Wandering,   // walks toward a cell it chose at random
    Hunting,     // goes after the player, or where it last saw them
    Fleeing,     // runs from the player
};

// One monster in the dungeon: an Entity, the kind of monster it is, what
// it's doing, the cells it's heading for and last saw the player on, how
// long it's been searching for them there, and the pack it belongs to, if
// any (packs are numbered from 1; 0 is none)
struct Monster : Entity
{
    const MonsterDef* def = nullptr;
    MonsterState state = MonsterState::Wandering;
    std::optional<Point> target;
    std::optional<Point> lastSeen;
    int searchTurns = 0;
    int packId = 0;
};
