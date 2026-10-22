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
}

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
    unsigned int traits = 0;

    // True if the kind has the trait. & keeps only the bits both have
    bool has(unsigned int trait) const
    {
        return (traits & trait) != 0;
    }
};

// Every kind of monster there is, so far. The inline makes it one table
// shared by every file that includes this header, not a copy in each. A
// kind with no traits can leave them out, and gets 0
inline constexpr MonsterDef MONSTER_DEFS[] = {
    // name, glyph, color, tile,
    //     sight, hp, damage from and to, defense, traits
    { "giant white rat", 'r', { 215, 215, 205, 255 }, "giant-white-rat",
      6, 4, 1, 3, 0 },
    { "jackal", 'j', { 205, 150, 80, 255 }, "jackal",
      7, 5, 1, 3, 0 },
    { "kobold", 'k', { 110, 185, 95, 255 }, "kobold",
      6, 7, 2, 4, 1 },
    { "goblin", 'g', { 160, 205, 60, 255 }, "goblin",
      7, 9, 2, 5, 1 },
    { "fruit bat", 'b', { 205, 120, 90, 255 }, "fruit-bat",
      6, 3, 1, 2, 0, Trait::NEVER_SLEEPS | Trait::ERRATIC },
    { "shrieker", ',', { 210, 130, 220, 255 }, "shrieker",
      5, 4, 0, 0, 0, Trait::STATIONARY | Trait::SHRIEKS },
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
// it's doing, the cells it's heading for and last saw the player on, and
// how long it's been searching for them there
struct Monster : Entity
{
    const MonsterDef* def = nullptr;
    MonsterState state = MonsterState::Wandering;
    std::optional<Point> target;
    std::optional<Point> lastSeen;
    int searchTurns = 0;
};
