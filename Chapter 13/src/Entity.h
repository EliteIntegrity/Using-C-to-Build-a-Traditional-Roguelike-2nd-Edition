#pragma once
#include "Common.h"

// What the player and the monsters have in common: a place on the map,
// health, and a defense that takes something off every blow they suffer
struct Entity
{
    Point pos;
    int hp = 1;
    int maxHp = 1;
    int defense = 0;

    bool isAlive() const
    {
        return hp > 0;
    }
};
