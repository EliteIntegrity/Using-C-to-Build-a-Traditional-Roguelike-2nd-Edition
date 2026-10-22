#pragma once
#include <string>   // std::string, for a kind's names
#include "Common.h"

// The kinds of thing an item can be
enum class ItemClass
{
    Potion,
    Scroll,
    Weapon,
    Armor,
    Bow,       // fires the arrows in the quiver
    Missile,   // arrows, for a bow, and darts, to throw
    Gold,
    Amulet,    // counted, as gold is, not carried in the pack
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
    Lightning,    // a bolt along a line the player aims
    Fireball,     // a burst of fire where the player aims
};

// What every item of one kind has in common, read from items.txt. A potion
// or a scroll has an effect; a weapon has damage; armor has defense, and
// may be noisy. A bow has its arrows' damage and a range; a missile is
// found in a stack, and if it's thrown, has damage of its own
struct ItemDef
{
    std::string id;     // its name in the data files, as in short_sword
    std::string name;   // the kind's own name, as in "healing"
    ItemClass itemClass = ItemClass::Potion;
    ItemEffect effect = ItemEffect::None;
    std::string tile;   // its picture's name in tiles.txt
    int frequency = 0;  // how common it is, against the others' frequencies
    int minDamage = 0;
    int maxDamage = 0;
    int defense = 0;
    bool noisy = false;
    int range = 0;          // a bow's, in cells
    int minFound = 1;       // how many of a missile are found together
    int maxFound = 1;
    bool thrown = false;    // made to be thrown: a dagger, a dart

    // Potions, scrolls, arrows and darts stack: any number of one kind
    // share a slot in the pack, or a cell on the floor. Each weapon, bow
    // and suit of armor is one of its own
    bool stacks() const
    {
        return itemClass == ItemClass::Potion ||
               itemClass == ItemClass::Scroll ||
               itemClass == ItemClass::Missile;
    }
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
