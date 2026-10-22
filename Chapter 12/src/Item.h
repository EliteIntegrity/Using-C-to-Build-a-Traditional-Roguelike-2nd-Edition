#pragma once
#include "Common.h"

// The kinds of thing an item can be. Chapter 13 adds weapons and armor
enum class ItemClass
{
    Potion,
    Scroll,
};

// What using an item does
enum class ItemEffect
{
    Heal,         // gives back some health
    HealFully,    // gives back all of it
    Strengthen,   // adds 1 to the player's strength, for good
    Teleport,     // moves the player somewhere at random
    MapFloor,     // shows the player the whole floor
    Identify,     // names the kind of an item the player chooses
};

// What every item of one kind has in common. Chapter 16 moves these into
// a text file
struct ItemDef
{
    const char* name;   // the kind's own name, as in "healing"
    ItemClass itemClass;
    ItemEffect effect;
};

// Every kind of item there is, so far. Chapter 14 adds the potions whose
// effects last (poison, regeneration and confusion) and the scroll of
// fear, and Chapter 21 the scrolls of lightning and fireball
inline constexpr ItemDef ITEM_DEFS[] = {
    { "healing", ItemClass::Potion, ItemEffect::Heal },
    { "greater healing", ItemClass::Potion, ItemEffect::HealFully },
    { "strength", ItemClass::Potion, ItemEffect::Strengthen },
    { "teleportation", ItemClass::Scroll, ItemEffect::Teleport },
    { "magic mapping", ItemClass::Scroll, ItemEffect::MapFloor },
    { "identify", ItemClass::Scroll, ItemEffect::Identify },
};

// Some items of one kind, as one stack: potions and scrolls of the same
// kind are carried together
struct Item
{
    const ItemDef* def = nullptr;
    int count = 1;
};

// An item lying in the dungeon, where it is, and whether the player has
// seen it there. A cell holds one at most
struct FloorItem
{
    Point pos;
    Item item;
    bool seen = false;
};
