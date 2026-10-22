#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for what a monster is heading for
#include <string>     // std::string, for a kind's names
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

    // It can open a closed door. Without it, a closed door stops it
    constexpr unsigned int OPENS_DOORS = 1 << 8;

    // It's never found at random, only in the vaults that ask for it
    constexpr unsigned int VAULT_ONLY = 1 << 9;
}

// What every monster of one kind has in common, read from monsters.txt
struct MonsterDef
{
    std::string id;     // its name in the data files, as in cave_orc
    std::string name;   // its name in messages
    char glyph = '?';
    SDL_Color color = {};
    std::string tile;   // its picture's name in tiles.txt
    int minFloor = 1;   // the floors it's found on, from minFloor to
    int maxFloor = 1;   // maxFloor
    int sight = 1;      // how far it can see, in cells
    int hp = 1;         // its health when it's made
    int minDamage = 0;  // each of its blows does minDamage to maxDamage,
    int maxDamage = 0;  // before the target's defense
    int defense = 0;
    unsigned int traits = 0;
    int venom = 0;      // a hit poisons for this many turns
    int pack = 1;       // how many of it are found together

    // True if the kind has the trait. & keeps only the bits both have
    bool has(unsigned int trait) const
    {
        return (traits & trait) != 0;
    }
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
