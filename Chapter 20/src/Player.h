#pragma once
#include <optional>   // std::optional, for an empty hand
#include <string>     // std::string, for what killed the player
#include "Entity.h"
#include "Inventory.h"

// The player
struct Player : Entity
{
    // The damage of a blow with bare fists
    static constexpr int FIST_MIN_DAMAGE = 1;
    static constexpr int FIST_MAX_DAMAGE = 2;

    // Added to the damage of every blow. Potions of strength raise it
    int strength = 0;

    // The gold the player has picked up, and the Amulets. Neither is
    // carried in the pack
    int gold = 0;
    int amulets = 0;

    // Whatever killed the player, such as "a jackal", for the death screen
    std::string killedBy;

    // What they carry, the weapon in their hand, and the armor they wear,
    // if any. Armor sets the player's defense
    Inventory inventory;
    std::optional<Item> weapon;
    std::optional<Item> armor;

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
};
