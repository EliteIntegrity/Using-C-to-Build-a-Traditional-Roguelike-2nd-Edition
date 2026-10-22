#pragma once
#include <iterator>   // std::size, to count the events' names

// The things in the game that make a sound. sounds.txt says which files
// each one plays
enum class SoundEvent
{
    Hit,        // a blow, or a missile, lands on a monster
    Kill,       // a monster dies
    Hurt,       // the player is hit
    Shoot,      // an arrow is fired, or something thrown
    Miss,       // a missile misses
    Lightning,  // a bolt of lightning
    Fire,       // a fireball's burst, or a dragon's breath
    DoorOpen,
    DoorClose,
    Stairs,     // the player goes down the stairs
    PickUp,
    Gold,
    Amulet,
    Drop,
    Wield,      // a weapon wielded, armor put on, or a bow readied
    Quiver,     // arrows put in the quiver
    Drink,
    Read,
    Heal,       // a potion that heals
    Poisoned,
    Confused,
    Teleport,
    Fear,       // the scroll of fear
    Trap,       // a trap springs
    Alarm,      // an alarm trap rings
    Shriek,     // a shrieker shrieks
    Found,      // a secret door, or a trap, found
    Bump,       // the player walks into a wall
    Breaks,     // an arrow breaks
    Refused,    // a command that can't be done now
    LowHealth,  // the player's health falls below a quarter
    Death,      // the player dies
    Choose,     // a class chosen, and the run begun
};

// Each event's name in sounds.txt, in the order above
constexpr const char* SOUND_EVENT_NAMES[] = {
    "hit", "kill", "hurt", "shoot", "miss", "lightning", "fire",
    "door_open", "door_close", "stairs", "pick_up", "gold", "amulet",
    "drop", "wield", "quiver", "drink", "read", "heal", "poisoned",
    "confused", "teleport", "fear", "trap", "alarm", "shriek", "found",
    "bump", "breaks", "refused", "low_health", "death", "choose",
};

// How many events there are: as many as there are names. The
// static_assert stops the build if the events above ever number more or
// fewer than their names
constexpr int SOUND_EVENTS = static_cast<int>(std::size(SOUND_EVENT_NAMES));
static_assert(static_cast<int>(SoundEvent::Choose) + 1 == SOUND_EVENTS);
