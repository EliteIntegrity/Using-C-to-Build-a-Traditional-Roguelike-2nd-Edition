#pragma once
#include <optional>   // std::optional, for an empty hand
#include <string>     // std::string, for what killed the player
#include <vector>     // std::vector, for a class's kit
#include "Entity.h"
#include "Inventory.h"

// The gifts a class can have, things it does better than the others, as
// bits in ClassDef::gifts
namespace Gift
{
    // A free search every turn, so secret doors and traps are found in
    // passing
    constexpr unsigned int KEEN_EYED = 1 << 0;

    // A monster asleep that can see the player wakes half as often
    constexpr unsigned int STEALTHY = 1 << 1;

    // A scroll's magic does half as much damage again
    constexpr unsigned int SCROLL_POWER = 1 << 2;

    // A scroll's kind is known as soon as it's picked up
    constexpr unsigned int SCROLL_LORE = 1 << 3;

    // Lightning cast by heart, with no scroll: aimed as a scroll of
    // lightning is, and ready again a few turns later
    constexpr unsigned int LIGHTNING_BY_HEART = 1 << 4;
}

// A class the player can choose, read from classes.txt: its name, its
// picture, a line about it for the class menu, its health, strength and
// defense, its kit, what it starts with, and its gifts
struct ClassDef
{
    std::string id;     // its name in a save, as in fighter
    std::string name;   // its name on the screen, as in Fighter
    std::string tile;   // its picture's name in tiles.txt
    std::string about;
    int hp = 1;
    int strength = 0;
    int defense = 0;    // its own, before armor adds any
    std::vector<Item> kit;
    unsigned int gifts = 0;

    // True if the class has the gift. & keeps only the bits both have
    bool has(unsigned int gift) const
    {
        return (gifts & gift) != 0;
    }
};

// The player
struct Player : Entity
{
    // The class they chose at the start of the run
    const ClassDef* classDef = nullptr;

    // The damage of a blow with bare fists
    static constexpr int FIST_MIN_DAMAGE = 1;
    static constexpr int FIST_MAX_DAMAGE = 2;

    // Added to the damage of every blow. Potions of strength raise it
    int strength = 0;

    // The gold the player has picked up, and the Amulets. Neither is
    // carried in the pack
    int gold = 0;
    int amulets = 0;

    // The turns until lightning cast by heart is ready again: 0 when it's
    // ready
    int spellTurns = 0;

    // Whatever killed the player, such as "a jackal", for the death screen
    std::string killedBy;

    // What they carry, the weapon in their hand, the armor they wear, the
    // bow they have ready, and the arrows in their quiver, if any. Armor
    // adds to their defense
    Inventory inventory;
    std::optional<Item> weapon;
    std::optional<Item> armor;
    std::optional<Item> bow;
    std::optional<Item> quiver;

    // Each blow does minDamage() to maxDamage(), before strength and the
    // target's defense: the weapon's damage, or the fists'
    int minDamage() const
    {
        return weapon ? weapon->def->minDamage : FIST_MIN_DAMAGE;
    }

    int maxDamage() const
    {
        return weapon ? weapon->def->maxDamage : FIST_MAX_DAMAGE;
    }

    // Works out their defense again: their class's own, and their armor's,
    // if they wear any. It changes whenever they put armor on
    void updateDefense()
    {
        defense = classDef->defense;
        if (armor)
        {
            defense += armor->def->defense;
        }
    }
};
