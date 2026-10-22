#pragma once
#include <optional>   // std::optional, for the Amulet, if it has one
#include <vector>     // std::vector, for the cells of a vault's contents
#include "Common.h"
#include "Data.h"
#include "Map.h"

// Builds a vault from vaults.txt into the map: its walls, floors and doors.
// What's in it, the game fills in, from the cells the stamp hands back
namespace VaultStamp
{
    // The cells of a vault's contents, by what they hold, and what its
    // guardians are
    struct Contents
    {
        std::vector<Point> traps;
        std::vector<Point> treasure;
        std::vector<Point> monsters;
        std::vector<Point> guardians;
        const MonsterDef* guardian = nullptr;
        std::optional<Point> amulet;
    };

    // The cell just outside the vault's way in, from its top left corner.
    // It can be outside the vault's map, as (-1, 3) is
    Point entrance(const VaultDef& vault);

    // Builds the vault into the map, its top left corner on corner. A
    // space leaves the map as it was. Returns the cells of its contents
    Contents stamp(Map& map, const VaultDef& vault, Point corner);
}
