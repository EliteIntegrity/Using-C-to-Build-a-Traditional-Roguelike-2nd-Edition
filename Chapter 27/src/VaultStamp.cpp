#include "VaultStamp.h"

namespace
{
    // True if a cell of the vault's map is outside it: past its rows, or a
    // space
    bool outside(const VaultDef& vault, int x, int y)
    {
        if (y < 0 || y >= vault.height() || x < 0 || x >= vault.width())
        {
            return true;
        }
        return vault.rows[y][x] == ' ';
    }
}

// The way in is the door on the vault's edge, and the cell outside it is
// the one of its four straight neighbors that's outside the vault. The
// data's checks make sure there's exactly one
Point VaultStamp::entrance(const VaultDef& vault)
{
    for (int y = 0; y < vault.height(); ++y)
    {
        for (int x = 0; x < vault.width(); ++x)
        {
            char symbol = vault.rows[y][x];
            if (symbol != '+' && symbol != 'S')
            {
                continue;
            }
            for (int i = 0; i < 8; i += 2)
            {
                // DIRECTIONS' even steps are the straight ones
                Point next = { x + DIRECTIONS[i].x, y + DIRECTIONS[i].y };
                if (outside(vault, next.x, next.y))
                {
                    return next;
                }
            }
        }
    }
    return { 0, 0 };
}

VaultStamp::Contents VaultStamp::stamp(Map& map, const VaultDef& vault,
                                       Point corner)
{
    Contents contents;
    contents.guardian = vault.guardian;
    for (int y = 0; y < vault.height(); ++y)
    {
        for (int x = 0; x < vault.width(); ++x)
        {
            char symbol = vault.rows[y][x];
            Point cell = { corner.x + x, corner.y + y };
            if (symbol == ' ')
            {
                continue;
            }
            if (symbol == '#')
            {
                map.set(cell.x, cell.y, Terrain::Wall);
                continue;
            }
            if (symbol == '+')
            {
                map.set(cell.x, cell.y, Terrain::ClosedDoor);
                continue;
            }
            if (symbol == 'S')
            {
                map.set(cell.x, cell.y, Terrain::SecretDoor);
                continue;
            }

            // Everything else stands on a floor of worked stone, in a cave
            // too
            map.set(cell.x, cell.y, Terrain::Floor);
            if (symbol == '^')
            {
                contents.traps.push_back(cell);
            }
            else if (symbol == '$')
            {
                contents.treasure.push_back(cell);
            }
            else if (symbol == 'm')
            {
                contents.monsters.push_back(cell);
            }
            else if (symbol == 'G')
            {
                contents.guardians.push_back(cell);
            }
            else if (symbol == 'A')
            {
                contents.amulet = cell;
            }
        }
    }
    return contents;
}
