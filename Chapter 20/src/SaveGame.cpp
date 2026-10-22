// Saving a run, and loading it again. These are Game's own functions, in a
// file of their own, since Game.cpp is long enough already. A save is
// text, in the same records as the data files, so FieldReader checks every
// value read back, and names the line of any mistake. Then the records are
// checked against each other, since a save is only any use whole
#include "Game.h"
#include <map>       // std::map, for the monsters' states by name
#include <sstream>   // std::istringstream, to read the seed
#include "Files.h"
#include "Records.h"

namespace
{
    // The save, in the folder SDL gives the game for each user, and where
    // a damaged one is set aside
    const std::string SAVE_FILE = "save.txt";
    const std::string DAMAGED_FILE = "save.bad";

    // The version of the save's format. Whatever changes what's saved
    // changes the version, so an old save is never misread. Version 2
    // added the caves' floor (Chapter 18), version 3 doors, secret doors
    // and traps (Chapter 19), and version 4 the Amulets (Chapter 20)
    constexpr int SAVE_VERSION = 4;

    std::string savePath()
    {
        return userPath(SAVE_FILE);
    }

    // Each kind of terrain's letter in a save's rows
    struct TerrainLetter
    {
        Terrain terrain;
        char letter;
    };

    constexpr TerrainLetter TERRAIN_LETTERS[] = {
        { Terrain::Wall, '#' },
        { Terrain::Floor, '.' },
        { Terrain::StairsDown, '>' },
        { Terrain::CaveFloor, ':' },
        { Terrain::ClosedDoor, '+' },
        { Terrain::OpenDoor, '/' },
        { Terrain::SecretDoor, 'S' },
    };

    char letterFor(Terrain terrain)
    {
        for (const TerrainLetter& each : TERRAIN_LETTERS)
        {
            if (each.terrain == terrain)
            {
                return each.letter;
            }
        }
        return '?';
    }

    // The terrain a letter stands for. False if it stands for none
    bool terrainFor(char letter, Terrain& terrain)
    {
        for (const TerrainLetter& each : TERRAIN_LETTERS)
        {
            if (each.letter == letter)
            {
                terrain = each.terrain;
                return true;
            }
        }
        return false;
    }

    // The monsters' states, by their names in a save
    const std::map<std::string, MonsterState> MONSTER_STATES = {
        { "asleep", MonsterState::Asleep },
        { "wandering", MonsterState::Wandering },
        { "hunting", MonsterState::Hunting },
        { "fleeing", MonsterState::Fleeing },
    };

    std::string nameOf(MonsterState state)
    {
        for (const auto& [name, each] : MONSTER_STATES)
        {
            if (each == state)
            {
                return name;
            }
        }
        return "";
    }

    // Writing: each line of a record is "key = value"
    void addField(std::string& text, const std::string& key,
                  const std::string& value)
    {
        text += key + " = " + value + "\n";
    }

    void addField(std::string& text, const std::string& key, int value)
    {
        addField(text, key, std::to_string(value));
    }

    void addPoint(std::string& text, const std::string& key, Point point)
    {
        addField(text, key,
                 std::to_string(point.x) + " " + std::to_string(point.y));
    }

    void addColor(std::string& text, const std::string& key,
                  SDL_Color color)
    {
        addField(text, key, std::to_string(color.r) + " " +
                            std::to_string(color.g) + " " +
                            std::to_string(color.b));
    }

    // The statuses the player and the monsters share
    void addStatuses(std::string& text, const Entity& entity)
    {
        addField(text, "poisoned", entity.poisoned);
        addField(text, "regenerating", entity.regenerating);
        addField(text, "confused", entity.confused);
        addField(text, "afraid", entity.afraid);
    }

    // Reading: a place on the map, as x and y
    Point readPoint(FieldReader& fields, const std::string& key)
    {
        std::vector<int> xy = fields.numbers(key, 2, 0);
        return { xy[0], xy[1] };
    }

    void readStatuses(FieldReader& fields, Entity& entity)
    {
        entity.poisoned = fields.number("poisoned", 0);
        entity.regenerating = fields.number("regenerating", 0);
        entity.confused = fields.number("confused", 0);
        entity.afraid = fields.number("afraid", 0);
    }

    // The kind a key names by its id, or nullptr, with the mistake
    // reported, if the data files have no such kind
    const ItemDef* readItemKind(FieldReader& fields, const GameData& data,
                                const std::string& key)
    {
        std::string id = fields.id(key);
        const ItemDef* def = data.findItem(id);
        if (def == nullptr && !id.empty())
        {
            fields.report(fields.lineOf(key), "no kind of item is called '" +
                          id + "'");
        }
        return def;
    }

    const MonsterDef* readMonsterKind(FieldReader& fields,
                                      const GameData& data,
                                      const std::string& key)
    {
        std::string id = fields.id(key);
        const MonsterDef* def = data.findMonster(id);
        if (def == nullptr && !id.empty())
        {
            fields.report(fields.lineOf(key),
                          "no kind of monster is called '" + id + "'");
        }
        return def;
    }

    // The run's seed. It can be bigger than an int, so it's read as an
    // unsigned int
    unsigned int readSeed(FieldReader& fields)
    {
        std::string text = fields.text("seed");
        std::istringstream in(text);
        unsigned int seed = 0;
        if (!text.empty() && !((in >> seed) && (in >> std::ws).eof()))
        {
            fields.report(fields.lineOf("seed"),
                          "'seed' should be a whole number");
        }
        return seed;
    }

    // How one kind of potion or scroll looks in the run, and whether the
    // player knows it
    void readLook(FieldReader& fields, const GameData& data,
                  Identification& looks)
    {
        const ItemDef* def = readItemKind(fields, data, "kind");
        Appearance look;
        look.name = fields.text("name");
        look.color = fields.color("color");
        bool known = fields.yesNo("known");
        bool hasLook = def != nullptr &&
                       (def->itemClass == ItemClass::Potion ||
                        def->itemClass == ItemClass::Scroll);
        if (def != nullptr && !hasLook)
        {
            fields.report(fields.lineOf("kind"),
                          "only potions and scrolls have looks");
        }
        if (hasLook)
        {
            looks.setAppearance(*def, look);
            if (known)
            {
                looks.learn(*def);
            }
        }
    }

    // The player, but for the pack, which has a record for each stack. The
    // player's defense comes from their armor, so it isn't saved
    void readPlayer(FieldReader& fields, const GameData& data,
                    Player& player)
    {
        player.pos = readPoint(fields, "pos");
        player.hp = fields.number("hp", 1);
        player.maxHp = fields.number("max_hp", 1);
        if (player.hp > player.maxHp)
        {
            fields.report(fields.lineOf("hp"), "'hp' is more than 'max_hp'");
        }
        player.strength = fields.number("strength", 0);
        player.gold = fields.number("gold", 0);
        player.amulets = fields.number("amulets", 0);
        if (player.amulets > AMULETS)
        {
            fields.report(fields.lineOf("amulets"), "there are only " +
                          std::to_string(AMULETS) + " Amulets");
        }
        readStatuses(fields, player);
        if (fields.has("weapon"))
        {
            const ItemDef* def = readItemKind(fields, data, "weapon");
            if (def != nullptr && def->itemClass != ItemClass::Weapon)
            {
                fields.report(fields.lineOf("weapon"),
                              "'" + def->id + "' isn't a weapon");
            }
            else if (def != nullptr)
            {
                player.weapon = Item{ def };
            }
        }
        if (fields.has("armor"))
        {
            const ItemDef* def = readItemKind(fields, data, "armor");
            if (def != nullptr && def->itemClass != ItemClass::Armor)
            {
                fields.report(fields.lineOf("armor"),
                              "'" + def->id + "' isn't armor");
            }
            else if (def != nullptr)
            {
                player.armor = Item{ def };
                player.defense = def->defense;
            }
        }
    }

    // One stack in the player's pack. The records come in the pack's
    // order, so every stack gets its letter back
    void readCarried(FieldReader& fields, const GameData& data,
                     Player& player)
    {
        const ItemDef* def = readItemKind(fields, data, "kind");
        int count = fields.number("count", 1);
        bool counted = def != nullptr &&
                       (def->itemClass == ItemClass::Gold ||
                        def->itemClass == ItemClass::Amulet);
        if (counted)
        {
            fields.report(fields.lineOf("kind"), "gold and the Amulets "
                          "aren't carried in the pack");
        }
        else if (def != nullptr && !player.inventory.add(Item{ def, count }))
        {
            fields.report(fields.lineOf("kind"), "the pack is already full");
        }
    }

    // One row of the map: which row it is, what each of its cells is made
    // of, and which of them the player has seen. rowsRead keeps track of
    // the rows so far, so none is given twice
    void readRow(FieldReader& fields, Map& map, std::vector<bool>& rowsRead)
    {
        int y = fields.number("y", 0);
        std::string terrain = fields.text("terrain");
        std::string explored = fields.text("explored");
        if (y >= MAP_H)
        {
            fields.report(fields.lineOf("y"), "'y' goes up to " +
                          std::to_string(MAP_H - 1));
            return;
        }
        if (rowsRead[y])
        {
            fields.report(fields.lineOf("y"), "row " + std::to_string(y) +
                          " is given twice");
            return;
        }
        rowsRead[y] = true;
        if (terrain.size() != MAP_W || explored.size() != MAP_W)
        {
            fields.report(fields.lineOf("terrain"), "a row is " +
                          std::to_string(MAP_W) + " cells across");
            return;
        }
        for (int x = 0; x < MAP_W; ++x)
        {
            Terrain cell = Terrain::Wall;
            if (!terrainFor(terrain[x], cell))
            {
                fields.report(fields.lineOf("terrain"),
                              "no terrain is called '" +
                              std::string(1, terrain[x]) + "'");
                return;
            }
            map.set(x, y, cell);
            if (explored[x] == '1')
            {
                map.setExplored(x, y);
            }
            else if (explored[x] != '0')
            {
                fields.report(fields.lineOf("explored"),
                              "'explored' is 0s and 1s");
                return;
            }
        }
    }

    // A monster, as it was. Its kind gives it its health at most, and its
    // defense, which aren't saved
    void readMonster(FieldReader& fields, const GameData& data,
                     std::vector<Monster>& monsters)
    {
        Monster monster;
        monster.def = readMonsterKind(fields, data, "kind");
        monster.pos = readPoint(fields, "pos");
        monster.hp = fields.number("hp", 1);
        std::string state = fields.text("state");
        auto found = MONSTER_STATES.find(state);
        if (found != MONSTER_STATES.end())
        {
            monster.state = found->second;
        }
        else if (!state.empty())
        {
            fields.report(fields.lineOf("state"), "no state is called '" +
                          state + "'");
        }
        monster.packId = fields.number("pack", 0);
        monster.searchTurns = fields.number("search_turns", 0);
        readStatuses(fields, monster);
        if (fields.has("target"))
        {
            monster.target = readPoint(fields, "target");
        }
        if (fields.has("last_seen"))
        {
            monster.lastSeen = readPoint(fields, "last_seen");
        }
        if (monster.def != nullptr)
        {
            monster.maxHp = monster.def->hp;
            monster.defense = monster.def->defense;
            if (monster.hp > monster.maxHp)
            {
                fields.report(fields.lineOf("hp"), "a " + monster.def->id +
                              " has " + std::to_string(monster.maxHp) +
                              " health at most");
            }
        }
        monsters.push_back(monster);
    }

    // A trap, as it was: its kind, by its name, where it is, and whether
    // the player knows of it
    void readTrap(FieldReader& fields, std::vector<Trap>& traps)
    {
        Trap trap;
        std::string kind = fields.text("kind");
        bool named = false;
        for (const TrapDef& def : TRAP_DEFS)
        {
            if (kind == def.id)
            {
                trap.kind = def.kind;
                named = true;
            }
        }
        if (!named && !kind.empty())
        {
            fields.report(fields.lineOf("kind"),
                          "no kind of trap is called '" + kind + "'");
        }
        trap.pos = readPoint(fields, "pos");
        trap.known = fields.yesNo("known");
        traps.push_back(trap);
    }

    // An item lying on the floor
    void readFloorItem(FieldReader& fields, const GameData& data,
                       std::vector<FloorItem>& items)
    {
        FloorItem floorItem;
        floorItem.item.def = readItemKind(fields, data, "kind");
        floorItem.item.count = fields.number("count", 1);
        floorItem.pos = readPoint(fields, "pos");
        floorItem.seen = fields.yesNo("seen");
        items.push_back(floorItem);
    }

    // Reports something the save puts off the map, or in a wall, as in
    // "save.txt: the jackal at 70, 5 is off the map". True if it's
    // somewhere it can be
    bool checkPlace(const Map& map, Point pos, const std::string& what,
                    std::vector<std::string>& errors)
    {
        std::string where = SAVE_FILE + ": " + what + " at " +
                            std::to_string(pos.x) + ", " +
                            std::to_string(pos.y);
        if (!map.inBounds(pos.x, pos.y))
        {
            errors.push_back(where + " is off the map");
            return false;
        }
        if (!map.isWalkable(pos.x, pos.y))
        {
            errors.push_back(where + " is in a wall");
            return false;
        }
        return true;
    }
}

bool Game::saveExists() const
{
    return SDL_GetPathInfo(savePath().c_str(), nullptr);
}

void Game::removeSave()
{
    if (!SDL_RemovePath(savePath().c_str()))
    {
        SDL_Log("The save couldn't be deleted: %s", SDL_GetError());
    }
}

// Renames a damaged save to save.bad, in place of any save.bad before it.
// If it can't, the reason is logged, and the save stays where it is
bool Game::setSaveAside()
{
    if (!SDL_RenamePath(savePath().c_str(), userPath(DAMAGED_FILE).c_str()))
    {
        SDL_Log("The save couldn't be set aside: %s", SDL_GetError());
        return false;
    }
    return true;
}

// Writes the run as it stands, between turns: everything that can't be
// worked out again from the rest
bool Game::writeSave()
{
    std::string text = "# A saved run of Amulets of SDL. The run's end "
                       "deletes it.\n";
    text += "\n[save]\n";
    addField(text, "version", SAVE_VERSION);

    text += "\n[run]\n";
    addField(text, "seed", std::to_string(seed_));
    addField(text, "turn", turn_);
    addField(text, "floor", level_.depth);
    addField(text, "dice", rng_.state());

    // How this run's potions and scrolls look, and which the player knows
    for (const ItemDef& def : data_.items())
    {
        bool hasLook = def.itemClass == ItemClass::Potion ||
                       def.itemClass == ItemClass::Scroll;
        if (!hasLook)
        {
            continue;
        }
        const Appearance& look = identification_.appearanceOf(def);
        text += "\n[look]\n";
        addField(text, "kind", def.id);
        addField(text, "name", look.name);
        addColor(text, "color", look.color);
        addField(text, "known", identification_.isKnown(def) ? "yes" : "no");
    }

    text += "\n[player]\n";
    addPoint(text, "pos", player_.pos);
    addField(text, "hp", player_.hp);
    addField(text, "max_hp", player_.maxHp);
    addField(text, "strength", player_.strength);
    addField(text, "gold", player_.gold);
    addField(text, "amulets", player_.amulets);
    addStatuses(text, player_);
    if (player_.weapon)
    {
        addField(text, "weapon", player_.weapon->def->id);
    }
    if (player_.armor)
    {
        addField(text, "armor", player_.armor->def->id);
    }
    for (const Item& item : player_.inventory.items())
    {
        text += "\n[carried]\n";
        addField(text, "kind", item.def->id);
        addField(text, "count", item.count);
    }

    // The floor, a row at a time: what each cell is made of, and a 1 for
    // each cell the player has seen
    for (int y = 0; y < MAP_H; ++y)
    {
        std::string terrain;
        std::string explored;
        for (int x = 0; x < MAP_W; ++x)
        {
            terrain += letterFor(level_.map.at(x, y));
            explored += level_.map.isExplored(x, y) ? '1' : '0';
        }
        text += "\n[row]\n";
        addField(text, "y", y);
        addField(text, "terrain", terrain);
        addField(text, "explored", explored);
    }

    for (const Monster& monster : level_.monsters)
    {
        text += "\n[monster]\n";
        addField(text, "kind", monster.def->id);
        addPoint(text, "pos", monster.pos);
        addField(text, "hp", monster.hp);
        addField(text, "state", nameOf(monster.state));
        addField(text, "pack", monster.packId);
        addField(text, "search_turns", monster.searchTurns);
        addStatuses(text, monster);
        if (monster.target)
        {
            addPoint(text, "target", *monster.target);
        }
        if (monster.lastSeen)
        {
            addPoint(text, "last_seen", *monster.lastSeen);
        }
    }

    for (const FloorItem& floorItem : level_.items)
    {
        text += "\n[item]\n";
        addField(text, "kind", floorItem.item.def->id);
        addField(text, "count", floorItem.item.count);
        addPoint(text, "pos", floorItem.pos);
        addField(text, "seen", floorItem.seen ? "yes" : "no");
    }

    for (const Trap& trap : level_.traps)
    {
        text += "\n[trap]\n";
        addField(text, "kind", trapDefOf(trap.kind).id);
        addPoint(text, "pos", trap.pos);
        addField(text, "known", trap.known ? "yes" : "no");
    }

    return writeTextFile(savePath(), text);
}

// Reads the save into a run of its own: a floor, a player, the looks of
// the potions and scrolls, and the dice. Only once all of it has been read
// and checked does that run take the place of the one in play. Anything
// else leaves the mistakes in errors: a save from another version of the
// game, or a damaged one
LoadResult Game::loadSave(std::vector<std::string>& errors)
{
    std::string text;
    if (!readTextFile(savePath(), text))
    {
        errors.push_back("Couldn't read " + savePath());
        return LoadResult::Damaged;
    }
    std::vector<std::string> lineErrors;
    std::vector<Record> records = readRecords(text, SAVE_FILE, lineErrors);

    // The version comes first, before anything else is read: a save from
    // another version of the game isn't damaged, just not this game's
    if (records.empty() || records.front().type != "save")
    {
        errors.push_back(SAVE_FILE + ": a save starts with [save]");
        return LoadResult::Damaged;
    }
    FieldReader versionField(records.front(), SAVE_FILE, errors);
    int version = versionField.number("version", 1);
    if (!versionField.finish())
    {
        return LoadResult::Damaged;
    }
    if (version != SAVE_VERSION)
    {
        errors.push_back(SAVE_FILE + ": it's version " +
                         std::to_string(version) + ", and this game reads "
                         "version " + std::to_string(SAVE_VERSION));
        return LoadResult::OtherVersion;
    }
    errors.insert(errors.end(), lineErrors.begin(), lineErrors.end());

    unsigned int seed = 0;
    int turn = 0;
    Rng dice(0);
    Level level;
    Player player;
    Identification looks;
    int runs = 0;
    int players = 0;
    std::vector<bool> rowsRead(MAP_H, false);
    for (size_t i = 1; i < records.size(); ++i)
    {
        const Record& record = records[i];
        FieldReader fields(record, SAVE_FILE, errors);
        if (record.type == "run")
        {
            ++runs;
            seed = readSeed(fields);
            turn = fields.number("turn", 0);
            level.depth = fields.number("floor", 1);
            if (level.depth > LAST_FLOOR)
            {
                fields.report(fields.lineOf("floor"), "'floor' goes up to " +
                              std::to_string(LAST_FLOOR));
            }
            std::string state = fields.text("dice");
            if (!state.empty() && !dice.setState(state))
            {
                fields.report(fields.lineOf("dice"),
                              "'dice' isn't the dice's state");
            }
        }
        else if (record.type == "look")
        {
            readLook(fields, data_, looks);
        }
        else if (record.type == "player")
        {
            ++players;
            readPlayer(fields, data_, player);
        }
        else if (record.type == "carried")
        {
            readCarried(fields, data_, player);
        }
        else if (record.type == "row")
        {
            readRow(fields, level.map, rowsRead);
        }
        else if (record.type == "monster")
        {
            readMonster(fields, data_, level.monsters);
        }
        else if (record.type == "item")
        {
            readFloorItem(fields, data_, level.items);
        }
        else if (record.type == "trap")
        {
            readTrap(fields, level.traps);
        }
        else
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }
        fields.finish();
    }
    if (!errors.empty())
    {
        return LoadResult::Damaged;
    }

    // The records must add up to a whole run: one [run] and one [player],
    // every row of the map, and a look for every potion and scroll
    if (runs != 1)
    {
        errors.push_back(SAVE_FILE + ": a save has one [run]");
    }
    if (players != 1)
    {
        errors.push_back(SAVE_FILE + ": a save has one [player]");
    }
    for (int y = 0; y < MAP_H; ++y)
    {
        if (!rowsRead[y])
        {
            errors.push_back(SAVE_FILE + ": row " + std::to_string(y) +
                             " of the map is missing");
        }
    }
    for (const ItemDef& def : data_.items())
    {
        bool needsLook = def.itemClass == ItemClass::Potion ||
                         def.itemClass == ItemClass::Scroll;
        if (needsLook && !looks.hasAppearance(def))
        {
            errors.push_back(SAVE_FILE + ": the " + def.id +
                             " has no [look]");
        }
    }
    if (!errors.empty())
    {
        return LoadResult::Damaged;
    }

    // Everything on the map must be on it, on ground it can stand on, with
    // one monster to a cell, none on the player, and one item to a cell
    checkPlace(level.map, player.pos, "the player", errors);
    for (size_t i = 0; i < level.monsters.size(); ++i)
    {
        const Monster& monster = level.monsters[i];
        std::string what = "the " + monster.def->id;
        if (!checkPlace(level.map, monster.pos, what, errors))
        {
            continue;
        }
        bool shared = monster.pos == player.pos;
        for (size_t j = 0; j < i; ++j)
        {
            shared = shared || level.monsters[j].pos == monster.pos;
        }
        if (shared)
        {
            errors.push_back(SAVE_FILE + ": " + what + " at " +
                             std::to_string(monster.pos.x) + ", " +
                             std::to_string(monster.pos.y) +
                             " shares its cell");
        }
    }
    for (size_t i = 0; i < level.items.size(); ++i)
    {
        const FloorItem& floorItem = level.items[i];
        std::string what = "the " + floorItem.item.def->id;
        if (!checkPlace(level.map, floorItem.pos, what, errors))
        {
            continue;
        }
        bool shared = false;
        for (size_t j = 0; j < i; ++j)
        {
            shared = shared || level.items[j].pos == floorItem.pos;
        }
        if (shared)
        {
            errors.push_back(SAVE_FILE + ": " + what + " at " +
                             std::to_string(floorItem.pos.x) + ", " +
                             std::to_string(floorItem.pos.y) +
                             " shares its cell");
        }
    }
    for (size_t i = 0; i < level.traps.size(); ++i)
    {
        const Trap& trap = level.traps[i];
        std::string what = std::string("the ") + trapDefOf(trap.kind).id;
        if (!checkPlace(level.map, trap.pos, what, errors))
        {
            continue;
        }
        bool shared = false;
        for (size_t j = 0; j < i; ++j)
        {
            shared = shared || level.traps[j].pos == trap.pos;
        }
        if (shared)
        {
            errors.push_back(SAVE_FILE + ": " + what + " at " +
                             std::to_string(trap.pos.x) + ", " +
                             std::to_string(trap.pos.y) +
                             " shares its cell");
        }
        if (trap.kind == TrapKind::Trapdoor && level.depth == LAST_FLOOR)
        {
            errors.push_back(SAVE_FILE + ": a trapdoor can't be on the "
                             "last floor");
        }
    }

    // The stairs aren't saved: they're wherever the map has them. Every
    // floor but the last has one stairs down, and the last has none
    int stairs = 0;
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            if (level.map.at(x, y) == Terrain::StairsDown)
            {
                level.stairs = { x, y };
                ++stairs;
            }
        }
    }
    bool lastFloor = level.depth == LAST_FLOOR;
    if (stairs != (lastFloor ? 0 : 1))
    {
        errors.push_back(SAVE_FILE + ": the map has " +
                         std::to_string(stairs) + " stairs down, where floor " +
                         std::to_string(level.depth) + " has " +
                         (lastFloor ? "none" : "one"));
    }
    if (!errors.empty())
    {
        return LoadResult::Damaged;
    }

    // Everything was read, so the saved run takes the place of the new one
    seed_ = seed;
    turn_ = turn;
    rng_ = dice;
    level_ = level;
    player_ = player;
    identification_ = looks;
    updateSight();
    toPlayer_.compute(level_.map, { player_.pos });
    return LoadResult::Loaded;
}
