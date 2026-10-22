// The morgue file: a run's story, written when it ends, by death or by
// victory. It's plain text, in the player's own folder, under morgue,
// named by the moment the run ended, so every run keeps its own. These
// are Game's own functions, in a file of their own, as the save's are
#include "Game.h"
#include <algorithm>   // std::replace, for the folder's slashes
#include <string>      // std::string, for the file's text
#include <vector>      // std::vector, for the rows of the map
#include "Files.h"
#include "Words.h"

namespace
{
    // How many of the last messages the morgue file keeps
    constexpr int LAST_MESSAGES = 10;

    // The character a cell's terrain is in the glyph view
    char terrainGlyph(Terrain terrain)
    {
        switch (terrain)
        {
        case Terrain::Floor:
        case Terrain::CaveFloor:
            return '.';
        case Terrain::StairsDown:
            return '>';
        case Terrain::ClosedDoor:
            return '+';
        case Terrain::OpenDoor:
            return '\'';
        default:
            return '#';
        }
    }
}

// Writes the morgue file, and keeps its name, for the end screens. SDL
// gives the date and time, on the player's own clock. A run that can't be
// written down is logged, and the game goes on: it's over anyway
void Game::writeMorgue()
{
    SDL_Time now = 0;
    SDL_DateTime when;
    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &when, true))
    {
        SDL_Log("No morgue file: %s", SDL_GetError());
        return;
    }
    char name[64];
    SDL_snprintf(name, sizeof(name), "morgue/%04d-%02d-%02d-%02d%02d%02d.txt",
                 when.year, when.month, when.day, when.hour, when.minute,
                 when.second);
    if (!SDL_CreateDirectory(userPath("morgue").c_str()) ||
        !writeTextFile(userPath(name), morgueText()))
    {
        SDL_Log("No morgue file: %s", SDL_GetError());
        return;
    }
    morgueName_ = name;
}

// O, on an end screen: opens the morgue folder, with whatever the system
// opens folders with. SDL_OpenURL takes a file:// address, which has
// forward slashes, a slash before a Windows drive's letter, and %20 for a
// space, as in a Mac's Application Support folder
void Game::openMorgueFolder() const
{
    std::string path = userPath("morgue");
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.front() != '/')
    {
        path = "/" + path;
    }
    std::string url = "file://";
    for (char c : path)
    {
        if (c == ' ')
        {
            url += "%20";
        }
        else
        {
            url += c;
        }
    }
    if (!SDL_OpenURL(url.c_str()))
    {
        SDL_Log("Couldn't open the morgue folder: %s", SDL_GetError());
    }
}

// The run's story: who, how it ended, and its score and seed; the player
// at the end, and their pack, with everything in it named; their kills, by
// kind; the last messages; and the floor, as far as they'd seen it
std::string Game::morgueText() const
{
    std::string text = "Amulets of SDL: the story of a run\n\n";
    std::string who = withArticle(player_.classDef->name);
    if (state_ == GameState::Won)
    {
        text += capitalized(who) + ", who found all three Amulets of SDL, "
                "and won, on turn " + std::to_string(turn_) + ".\n";
    }
    else
    {
        text += capitalized(who) + ", killed by " + player_.killedBy +
                " on floor " + std::to_string(level_.depth) + ", on turn " +
                std::to_string(turn_) + ".\n";
    }
    text += "A score of " + std::to_string(score()) + ", with " +
            std::to_string(player_.gold) + " gold and " +
            std::to_string(player_.amulets) + " of " +
            std::to_string(AMULETS) + " Amulets. The seed was " +
            std::to_string(seed_) + ".\n\n";

    int low = player_.minDamage() + player_.strength;
    int high = player_.maxDamage() + player_.strength;
    text += "Health " + std::to_string(player_.hp) + " of " +
            std::to_string(player_.maxHp) + ", strength +" +
            std::to_string(player_.strength) + ", defense " +
            std::to_string(player_.defense) + ", damage " +
            std::to_string(low) + "-" + std::to_string(high) + ".\n";
    text += wieldingText() + ". " + wearingText() + ". " + bowText() +
            ".\n\n";

    // The pack, everything in it named, now that it doesn't matter
    text += "The pack:\n";
    const std::vector<Item>& items = player_.inventory.items();
    for (size_t i = 0; i < items.size(); ++i)
    {
        text += "  " + std::string(1, Inventory::letterFor(
                                       static_cast<int>(i))) +
                ") " + itemName(items[i], true) + "\n";
    }
    if (items.empty())
    {
        text += "  nothing\n";
    }

    // The kills, each kind's name and how many, in a column
    int total = 0;
    size_t widest = 0;
    for (const auto& [id, count] : kills_)
    {
        total += count;
        widest = SDL_max(widest, data_.findMonster(id)->name.size());
    }
    text += "\nKills, " + std::to_string(total) + " in all:\n";
    for (const auto& [id, count] : kills_)
    {
        std::string name = data_.findMonster(id)->name;
        name.resize(widest, ' ');
        text += "  " + name + "  " + std::to_string(count) + "\n";
    }

    // The last messages, as the log shows them
    text += "\nThe last messages:\n";
    const std::vector<MessageLog::Entry>& entries = log_.entries();
    size_t first = entries.size() > LAST_MESSAGES
                   ? entries.size() - LAST_MESSAGES : 0;
    for (size_t i = first; i < entries.size(); ++i)
    {
        text += "  " + entries[i].withCount() + "\n";
    }

    // The floor, as the player had seen it, drawn as the glyph view draws
    // it, with the traps they knew of, the items they'd seen, and the
    // monsters in sight, cut down to the part with anything in it
    std::vector<std::string> rows(MAP_H, std::string(MAP_W, ' '));
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            Terrain terrain = level_.map.at(x, y);
            bool wall = terrainGlyph(terrain) == '#';
            bool shown = level_.map.isExplored(x, y) &&
                         (!wall || level_.map.bordersFloor(x, y));
            if (shown)
            {
                rows[y][x] = terrainGlyph(terrain);
            }
        }
    }
    for (const Trap& trap : level_.traps)
    {
        if (trap.known)
        {
            rows[trap.pos.y][trap.pos.x] = '^';
        }
    }
    for (const FloorItem& floorItem : level_.items)
    {
        if (floorItem.seen)
        {
            Point cell = floorItem.pos;
            rows[cell.y][cell.x] = itemLook(floorItem.item).glyph;
        }
    }
    for (const Monster& monster : level_.monsters)
    {
        bool seen = level_.map.isVisible(monster.pos.x, monster.pos.y);
        if (seen && monster.isAlive())
        {
            rows[monster.pos.y][monster.pos.x] = monster.def->glyph;
        }
    }
    rows[player_.pos.y][player_.pos.x] = '@';

    // The rows and columns with anything in them: the player at least
    int top = MAP_H;
    int bottom = 0;
    int left = MAP_W;
    int right = 0;
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            if (rows[y][x] != ' ')
            {
                top = SDL_min(top, y);
                bottom = SDL_max(bottom, y);
                left = SDL_min(left, x);
                right = SDL_max(right, x);
            }
        }
    }
    text += "\nThe floor, as far as it had been seen:\n";
    for (int y = top; y <= bottom; ++y)
    {
        std::string line = rows[y].substr(left, right - left + 1);
        line.erase(line.find_last_not_of(' ') + 1);
        text += "  " + line + "\n";
    }
    return text;
}
