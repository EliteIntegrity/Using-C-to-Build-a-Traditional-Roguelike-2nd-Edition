#pragma once
#include "Common.h"

// What the player and the monsters have in common: a place on the map,
// health, a defense that takes something off every blow they suffer, and
// statuses
struct Entity
{
    Point pos;
    int hp = 1;
    int maxHp = 1;
    int defense = 0;

    // Statuses: how many more turns each one lasts. 0 means it isn't on
    int poisoned = 0;       // loses 1 health a turn
    int regenerating = 0;   // gains 1 health a turn
    int confused = 0;       // half of its moves go astray
    int afraid = 0;         // a monster flees, whatever its health

    bool isAlive() const
    {
        return hp > 0;
    }
};
