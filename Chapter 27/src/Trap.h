#pragma once
#include <SDL3/SDL.h>
#include "Common.h"

// The kinds of trap, and what each one does
enum class TrapKind
{
    Dart,           // 2 to 6 damage, less the player's defense
    PoisonNeedle,   // poisons the player
    Alarm,          // a noise loud enough to wake much of the floor
    Teleport,       // takes the player somewhere else on the floor
    Trapdoor,       // drops the player to the next floor, with a fall
};

// What every trap of a kind has: its name in a save, its name in messages,
// and the color of its ^, and of its tile
struct TrapDef
{
    TrapKind kind;
    const char* id;
    const char* name;
    SDL_Color color;
};

inline constexpr TrapDef TRAP_DEFS[] = {
    { TrapKind::Dart, "dart", "dart trap", { 200, 200, 215, 255 } },
    { TrapKind::PoisonNeedle, "poison_needle", "poison needle",
      { 130, 205, 70, 255 } },
    { TrapKind::Alarm, "alarm", "alarm trap", { 240, 205, 70, 255 } },
    { TrapKind::Teleport, "teleport", "teleport trap",
      { 175, 120, 240, 255 } },
    { TrapKind::Trapdoor, "trapdoor", "trapdoor", { 180, 125, 75, 255 } },
};

// The kind's row of TRAP_DEFS
inline const TrapDef& trapDefOf(TrapKind kind)
{
    for (const TrapDef& def : TRAP_DEFS)
    {
        if (def.kind == kind)
        {
            return def;
        }
    }
    return TRAP_DEFS[0];
}

// A trap on a floor: where it is, its kind, and whether the player knows
// it's there. A hidden trap is found by searching, or by stepping on it
struct Trap
{
    Point pos;
    TrapKind kind = TrapKind::Dart;
    bool known = false;
};
