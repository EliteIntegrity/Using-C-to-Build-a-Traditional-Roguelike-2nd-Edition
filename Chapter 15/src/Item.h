#pragma once
#include <string>   // std::string, to find a kind by its name
#include "Common.h"

// The kinds of thing an item can be
enum class ItemClass
{
    Potion,
    Scroll,
    Weapon,
    Armor,
    Gold,
};

// What using an item does
enum class ItemEffect
{
    None,         // isn't drunk or read
    Heal,         // gives back some health
    HealFully,    // gives back all of it
    Strengthen,   // adds 1 to the player's strength, for good
    Teleport,     // moves the player somewhere at random
    MapFloor,     // shows the player the whole floor
    Identify,     // names the kind of an item the player chooses
    Poison,       // poisons the player
    Regenerate,   // heals the player a little each turn, for a while
    Confuse,      // sends half the player's steps astray, for a while
    Frighten,     // makes the monsters in sight flee, for a while
};

// What every item of one kind has in common. Chapter 16 moves these into
// a text file. A potion or a scroll has an effect; a weapon has damage;
// armor has defense, and may be noisy
struct ItemDef
{
    const char* name;   // the kind's own name, as in "healing"
    ItemClass itemClass;
    ItemEffect effect;
    const char* tile;   // its picture's name in tiles.txt
    int frequency;      // how common it is, against the others' frequencies
    int minDamage = 0;
    int maxDamage = 0;
    int defense = 0;
    bool noisy = false;

    // Potions and scrolls stack: any number of one kind share a slot in
    // the pack, or a cell on the floor. Anything else is one of its own
    bool stacks() const
    {
        return itemClass == ItemClass::Potion ||
               itemClass == ItemClass::Scroll;
    }
};

// Every kind of item there is, so far. Chapter 21 adds bows, arrows,
// darts, and the scrolls of lightning and fireball
inline constexpr ItemDef ITEM_DEFS[] = {
    // name, class, effect, tile, frequency, then a weapon's damage from and
    // to, and armor's defense and noise
    { "healing", ItemClass::Potion, ItemEffect::Heal, "potion", 7 },
    { "greater healing", ItemClass::Potion, ItemEffect::HealFully, "potion",
      4 },
    { "strength", ItemClass::Potion, ItemEffect::Strengthen, "potion", 3 },
    { "poison", ItemClass::Potion, ItemEffect::Poison, "potion", 5 },
    { "regeneration", ItemClass::Potion, ItemEffect::Regenerate, "potion",
      4 },
    { "confusion", ItemClass::Potion, ItemEffect::Confuse, "potion", 5 },
    { "teleportation", ItemClass::Scroll, ItemEffect::Teleport, "scroll",
      5 },
    { "magic mapping", ItemClass::Scroll, ItemEffect::MapFloor, "scroll",
      4 },
    { "identify", ItemClass::Scroll, ItemEffect::Identify, "scroll", 6 },
    { "fear", ItemClass::Scroll, ItemEffect::Frighten, "scroll", 4 },
    { "dagger", ItemClass::Weapon, ItemEffect::None, "dagger", 3, 1, 4 },
    { "short sword", ItemClass::Weapon, ItemEffect::None, "sword", 2, 2, 6 },
    { "spear", ItemClass::Weapon, ItemEffect::None, "spear", 2, 2, 8 },
    { "mace", ItemClass::Weapon, ItemEffect::None, "mace", 2, 3, 7 },
    { "long sword", ItemClass::Weapon, ItemEffect::None, "sword", 1, 3, 9 },
    { "leather armor", ItemClass::Armor, ItemEffect::None, "leather-armor",
      3, 0, 0, 1 },
    { "chain mail", ItemClass::Armor, ItemEffect::None, "metal-armor",
      2, 0, 0, 3 },
    { "plate armor", ItemClass::Armor, ItemEffect::None, "metal-armor",
      1, 0, 0, 5, true },

    // Gold is placed in piles of its own, never chosen as an item, so its
    // frequency is 0. A pile's count is its number of gold pieces
    { "gold", ItemClass::Gold, ItemEffect::None, "gold", 0 },
};

// The kind with this name, or nullptr if there isn't one
inline const ItemDef* findItemDef(const std::string& name)
{
    for (const ItemDef& def : ITEM_DEFS)
    {
        if (name == def.name)
        {
            return &def;
        }
    }
    return nullptr;
}

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
