#pragma once
#include <string>   // std::string, for what killed the player
#include "Entity.h"
#include "Inventory.h"

// The player. Until Chapter 13 gives them weapons to wield, each of their
// blows does minDamage to maxDamage, before the target's defense
struct Player : Entity
{
    int minDamage = 2;
    int maxDamage = 6;

    // Added to the damage of every blow. Potions of strength raise it
    int strength = 0;

    // Whatever killed the player, such as "a jackal", for the death screen
    std::string killedBy;

    // What they carry
    Inventory inventory;
};
