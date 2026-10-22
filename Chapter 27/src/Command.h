#pragma once
#include "Common.h"

// What a command that takes many turns is doing
enum class CommandKind
{
    Rest,     // waiting until the player is healed
    Travel,   // walking to a cell the player chose, by the shortest way
    Explore,  // walking to the nearest edge of what the player has seen
};

// A command that takes many turns, a step at a time, and stops for
// anything the player should see to: what it's doing, where it's going,
// if anywhere, and the turns it's taken so far
struct Command
{
    CommandKind kind = CommandKind::Rest;
    Point goal;
    int turns = 0;
};
