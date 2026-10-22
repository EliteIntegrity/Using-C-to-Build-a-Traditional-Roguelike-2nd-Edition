#include "Data.h"
#include <algorithm>   // std::max, for the widest of a vault's rows
#include <map>       // std::map, for the traits and the ids so far
#include <sstream>   // std::istringstream, to read the traits' words
#include "Common.h"
#include "Files.h"
#include "Identification.h"
#include "Records.h"

namespace
{
    // A data file's records, or none, with the mistake reported, if the
    // file can't be read
    std::vector<Record> readDataFile(const std::string& folder,
                                     const std::string& file,
                                     std::vector<std::string>& errors)
    {
        std::string text;
        if (!readTextFile(folder + file, text))
        {
            errors.push_back("Couldn't read " + folder + file);
            return {};
        }
        return readRecords(text, file, errors);
    }

    // The monsters' traits that are bits, by their names in the files
    const std::map<std::string, unsigned int> TRAIT_BITS = {
        { "never_sleeps", Trait::NEVER_SLEEPS },
        { "erratic", Trait::ERRATIC },
        { "stationary", Trait::STATIONARY },
        { "shrieks", Trait::SHRIEKS },
        { "slow", Trait::SLOW },
        { "never_flees", Trait::NEVER_FLEES },
        { "regenerates", Trait::REGENERATES },
        { "drains", Trait::DRAINS },
        { "opens_doors", Trait::OPENS_DOORS },
        { "vault_only", Trait::VAULT_ONLY },
        { "keeps_distance", Trait::KEEPS_DISTANCE },
    };

    // The kinds of attack from afar, by their names in the files
    const std::map<std::string, RangedKind> RANGED_KINDS = {
        { "arrows", RangedKind::Arrows },
        { "lightning", RangedKind::Lightning },
        { "fire", RangedKind::Fire },
    };

    // The attack from afar a "ranged" value names, as in "arrows 2 6 7 1":
    // its kind, its damage from and to, its range, and the turns between
    // attacks
    void readRanged(FieldReader& fields, MonsterDef& def)
    {
        if (!fields.has("ranged"))
        {
            return;
        }
        std::istringstream words(fields.text("ranged"));
        std::string kind;
        RangedAttack ranged;
        bool read = static_cast<bool>(words >> kind >> ranged.minDamage >>
                                      ranged.maxDamage >> ranged.range >>
                                      ranged.every) &&
                    (words >> std::ws).eof();
        auto found = RANGED_KINDS.find(kind);
        bool right = read && found != RANGED_KINDS.end() &&
                     ranged.minDamage >= 0 &&
                     ranged.minDamage <= ranged.maxDamage &&
                     ranged.range >= 1 && ranged.every >= 1;
        if (!right)
        {
            fields.report(fields.lineOf("ranged"), "'ranged' is arrows, "
                          "lightning or fire, then damage from and to, a "
                          "range, and the turns between attacks");
            return;
        }
        ranged.kind = found->second;
        def.ranged = ranged;
    }

    // The traits a "traits" value names, as in "slow never_flees". Two
    // have an amount after a colon: pack:3, for a pack of three, and
    // venomous:4, for a bite that poisons for four turns
    void readTraits(FieldReader& fields, MonsterDef& def)
    {
        if (!fields.has("traits"))
        {
            return;
        }
        int line = fields.lineOf("traits");
        std::istringstream words(fields.text("traits"));
        std::string word;
        while (words >> word)
        {
            // The trait's name, and its amount, if it has one
            size_t colon = word.find(':');
            std::string name = word.substr(0, colon);
            int amount = 0;
            bool counted = colon != std::string::npos &&
                           readWholeNumber(word.substr(colon + 1), amount) &&
                           amount >= 1;

            if (TRAIT_BITS.contains(word))
            {
                def.traits |= TRAIT_BITS.at(word);
            }
            else if (name == "pack" && counted)
            {
                def.pack = amount;
            }
            else if (name == "venomous" && counted)
            {
                def.venom = amount;
            }
            else if (name == "pack" || name == "venomous")
            {
                fields.report(line, "'" + name + "' needs an amount of 1 "
                              "or more, as in " + name + ":3");
            }
            else
            {
                fields.report(line, "unknown trait '" + word + "'");
            }
        }
    }

    // A kind's damage, from and to, which mustn't run backward
    void readDamage(FieldReader& fields, ItemDef& def)
    {
        std::vector<int> damage = fields.numbers("damage", 2, 0);
        def.minDamage = damage[0];
        def.maxDamage = damage[1];
        if (def.minDamage > def.maxDamage)
        {
            fields.report(fields.lineOf("damage"), "'damage' runs backward");
        }
    }

    // Reports an id that an earlier record in the file already has, since
    // each kind's id must be its own. idLines holds the ids so far, and the
    // lines they're on, and a new id joins them
    void checkNewId(FieldReader& fields, const std::string& id,
                    std::map<std::string, int>& idLines)
    {
        if (id.empty())
        {
            return;   // a missing id has been reported already
        }
        if (idLines.contains(id))
        {
            fields.report(fields.lineOf("id"), "the id '" + id +
                          "' is already used, on line " +
                          std::to_string(idLines[id]));
            return;
        }
        idLines[id] = fields.lineOf("id");
    }

    // The classes of item, by the types of record that hold them
    const std::map<std::string, ItemClass> ITEM_CLASSES = {
        { "potion", ItemClass::Potion },
        { "scroll", ItemClass::Scroll },
        { "weapon", ItemClass::Weapon },
        { "armor", ItemClass::Armor },
        { "bow", ItemClass::Bow },
        { "missile", ItemClass::Missile },
        { "gold", ItemClass::Gold },
        { "amulet", ItemClass::Amulet },
    };

    // What potions and scrolls can do, by their names in the files
    const std::map<std::string, ItemEffect> ITEM_EFFECTS = {
        { "heal", ItemEffect::Heal },
        { "heal_fully", ItemEffect::HealFully },
        { "strengthen", ItemEffect::Strengthen },
        { "teleport", ItemEffect::Teleport },
        { "map_floor", ItemEffect::MapFloor },
        { "identify", ItemEffect::Identify },
        { "poison", ItemEffect::Poison },
        { "regenerate", ItemEffect::Regenerate },
        { "confuse", ItemEffect::Confuse },
        { "frighten", ItemEffect::Frighten },
        { "lightning", ItemEffect::Lightning },
        { "fireball", ItemEffect::Fireball },
    };

    // The kinds of item the game's own code asks for by id, which
    // items.txt must have: the player's first weapon, gold, and the Amulet
    const char* const NEEDED_ITEMS[] = { "short_sword", "gold", "amulet" };

    // The symbols of a vault's map, a space included, and its sizes, the
    // biggest being about as big as rooms ever have room for
    const std::string VAULT_SYMBOLS = "#.+S^$mGA ";
    constexpr int MIN_VAULT_SIZE = 3;
    constexpr int MAX_VAULT_W = 15;
    constexpr int MAX_VAULT_H = 9;

    // True if a cell of a vault's map is outside the vault: past its rows,
    // or a space
    bool outsideVault(const std::vector<std::string>& rows, int x, int y)
    {
        if (y < 0 || y >= static_cast<int>(rows.size()) || x < 0 ||
            x >= static_cast<int>(rows[y].size()))
        {
            return true;
        }
        return rows[y][x] == ' ';
    }

    // Reports the first cell inside a vault that can't be reached from its
    // way in, by steps in any of the eight directions over anything but
    // wall. An Amulet walled off there could never be taken
    void checkVaultReach(FieldReader& fields,
                         const std::vector<std::string>& rows, Point wayIn,
                         int mapLine)
    {
        int w = static_cast<int>(rows.front().size());
        int h = static_cast<int>(rows.size());
        std::vector<bool> reached(w * h, false);
        reached[wayIn.y * w + wayIn.x] = true;
        std::vector<Point> toVisit = { wayIn };
        while (!toVisit.empty())
        {
            Point cell = toVisit.back();
            toVisit.pop_back();
            for (Point step : DIRECTIONS)
            {
                Point next = { cell.x + step.x, cell.y + step.y };
                bool open = !outsideVault(rows, next.x, next.y) &&
                            rows[next.y][next.x] != '#';
                if (open && !reached[next.y * w + next.x])
                {
                    reached[next.y * w + next.x] = true;
                    toVisit.push_back(next);
                }
            }
        }
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                bool open = rows[y][x] != ' ' && rows[y][x] != '#';
                if (open && !reached[y * w + x])
                {
                    fields.report(mapLine + 1 + y, "the cell at column " +
                                  std::to_string(x + 1) + " can't be "
                                  "reached from the way in");
                    return;
                }
            }
        }
    }

    // Checks a vault's map, and makes its rows as wide as the widest, with
    // spaces. It must be a size rooms have room for, use only the legend's
    // symbols, have G only if there's a guardian, and be closed all around:
    // a cell inside it that touches the outside, diagonals too, must be
    // wall, but for one door, the way in, which must meet the outside along
    // a row or a column. Every cell inside but wall must be reachable from
    // the way in. The rows begin on the line after the map's key
    void checkVaultMap(FieldReader& fields, VaultDef& def)
    {
        std::vector<std::string>& rows = def.rows;
        if (rows.empty())
        {
            return;   // reported already
        }
        int mapLine = fields.lineOf("map");
        size_t widest = 0;
        for (const std::string& row : rows)
        {
            widest = std::max(widest, row.size());
        }
        for (std::string& row : rows)
        {
            row.resize(widest, ' ');
        }
        int w = def.width();
        int h = def.height();
        if (w < MIN_VAULT_SIZE || h < MIN_VAULT_SIZE || w > MAX_VAULT_W ||
            h > MAX_VAULT_H)
        {
            fields.report(mapLine, "a vault is from 3 x 3 to 15 x 9 cells");
            return;
        }

        int ways = 0;
        Point wayIn = { 0, 0 };
        int amulets = 0;
        bool guards = false;
        bool open = false;   // reported once, at the first open cell
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                char c = rows[y][x];
                int line = mapLine + 1 + y;
                if (VAULT_SYMBOLS.find(c) == std::string::npos)
                {
                    fields.report(line, "'" + std::string(1, c) +
                                  "' isn't in a vault's legend");
                    continue;
                }
                guards = guards || c == 'G';
                if (c == 'A')
                {
                    ++amulets;
                }
                if (c == ' ' || c == '#')
                {
                    continue;
                }
                // Next to the outside, diagonals too; and straight next to
                // it, as a way in must be. DIRECTIONS' even steps are the
                // straight ones
                bool edge = false;
                bool straight = false;
                for (int i = 0; i < 8; ++i)
                {
                    Point next = { x + DIRECTIONS[i].x, y + DIRECTIONS[i].y };
                    bool out = outsideVault(rows, next.x, next.y);
                    edge = edge || out;
                    straight = straight || (out && i % 2 == 0);
                }
                bool door = c == '+' || c == 'S';
                if (edge && door && straight)
                {
                    ++ways;
                    wayIn = { x, y };
                }
                else if (edge && door)
                {
                    fields.report(line, "the door at column " +
                                  std::to_string(x + 1) +
                                  " meets the outside only at a corner");
                }
                else if (edge && !open)
                {
                    fields.report(line, "the vault is open to the outside, "
                                  "at column " + std::to_string(x + 1));
                    open = true;
                }
            }
        }
        if (ways != 1)
        {
            fields.report(mapLine, "a vault has one way in: a + or an S "
                          "on its edge");
        }
        else
        {
            checkVaultReach(fields, rows, wayIn, mapLine);
        }
        if (guards && !fields.has("guardian"))
        {
            fields.report(mapLine, "the map has G, but there's no "
                          "'guardian'");
        }
        if (!guards && def.guardian != nullptr)
        {
            fields.report(fields.lineOf("guardian"), "a 'guardian' needs "
                          "a G on the map");
        }
        if (def.amulet && amulets != 1)
        {
            fields.report(mapLine, "an Amulet's vault has one A");
        }
        if (!def.amulet && amulets > 0)
        {
            fields.report(mapLine, "only an Amulet's vault, with 'amulet = "
                          "yes', has an A");
        }
    }
}

bool GameData::load(const std::string& folder)
{
    monsters_.clear();
    items_.clear();
    vaults_.clear();
    errors_.clear();
    readMonsters(folder);
    readItems(folder);
    readVaults(folder);
    return errors_.empty();
}

const std::vector<MonsterDef>& GameData::monsters() const
{
    return monsters_;
}

const std::vector<ItemDef>& GameData::items() const
{
    return items_;
}

const MonsterDef* GameData::findMonster(const std::string& id) const
{
    for (const MonsterDef& def : monsters_)
    {
        if (def.id == id)
        {
            return &def;
        }
    }
    return nullptr;
}

const std::vector<VaultDef>& GameData::vaults() const
{
    return vaults_;
}

const ItemDef* GameData::findItem(const std::string& id) const
{
    for (const ItemDef& def : items_)
    {
        if (def.id == id)
        {
            return &def;
        }
    }
    return nullptr;
}

const std::vector<std::string>& GameData::errors() const
{
    return errors_;
}

// Every [monster] record in monsters.txt, checked, and kept if it has no
// mistakes
void GameData::readMonsters(const std::string& folder)
{
    const std::string file = "monsters.txt";
    size_t errorsBefore = errors_.size();

    // The ids so far, and their lines, to catch one given twice
    std::map<std::string, int> idLines;

    for (const Record& record : readDataFile(folder, file, errors_))
    {
        FieldReader fields(record, file, errors_);
        if (record.type != "monster")
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }

        MonsterDef def;
        def.id = fields.id("id");
        def.name = fields.text("name");
        def.glyph = fields.glyph("glyph");
        def.color = fields.color("color");
        def.tile = fields.text("tile");
        std::vector<int> floors = fields.numbers("floors", 2, 1);
        def.minFloor = floors[0];
        def.maxFloor = floors[1];
        def.hp = fields.number("hp", 1);
        std::vector<int> damage = fields.numbers("damage", 2, 0);
        def.minDamage = damage[0];
        def.maxDamage = damage[1];
        def.defense = fields.number("defense", 0);
        def.sight = fields.number("sight", 1);
        readTraits(fields, def);
        readRanged(fields, def);

        // Ranges run from low to high
        if (def.minFloor > def.maxFloor)
        {
            fields.report(fields.lineOf("floors"), "'floors' runs backward");
        }
        if (def.minDamage > def.maxDamage)
        {
            fields.report(fields.lineOf("damage"), "'damage' runs backward");
        }
        checkNewId(fields, def.id, idLines);

        if (fields.finish())
        {
            monsters_.push_back(def);
        }
    }

    // Every floor needs a kind of monster found at random to fill it. A
    // kind with a mistake is left out, which could leave a floor empty for
    // no real reason, so this waits until the records are all good
    if (errors_.size() > errorsBefore)
    {
        return;
    }
    for (int floor = 1; floor <= LAST_FLOOR; ++floor)
    {
        bool found = false;
        for (const MonsterDef& def : monsters_)
        {
            bool atRandom = !def.has(Trait::VAULT_ONLY);
            found = found || (atRandom && def.minFloor <= floor &&
                              floor <= def.maxFloor);
        }
        if (!found)
        {
            errors_.push_back(file + ": no monster is found on floor " +
                              std::to_string(floor));
        }
    }
}

// Every record in items.txt, checked, and kept if it has no mistakes. The
// record's type is the item's class, and decides what else it needs: an
// effect for a potion or a scroll, damage for a weapon, defense for armor,
// which may also be noisy, damage and a range for a bow, and for a missile
// how many are found together, and damage if it's thrown
void GameData::readItems(const std::string& folder)
{
    const std::string file = "items.txt";
    size_t errorsBefore = errors_.size();
    std::map<std::string, int> idLines;

    for (const Record& record : readDataFile(folder, file, errors_))
    {
        FieldReader fields(record, file, errors_);
        auto itemClass = ITEM_CLASSES.find(record.type);
        if (itemClass == ITEM_CLASSES.end())
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }

        ItemDef def;
        def.itemClass = itemClass->second;
        def.id = fields.id("id");
        def.name = fields.text("name");
        def.tile = fields.text("tile");
        def.frequency = fields.number("frequency", 0);
        if (def.itemClass == ItemClass::Potion ||
            def.itemClass == ItemClass::Scroll)
        {
            std::string effect = fields.text("effect");
            auto found = ITEM_EFFECTS.find(effect);
            if (found != ITEM_EFFECTS.end())
            {
                def.effect = found->second;
            }
            else if (!effect.empty())
            {
                fields.report(fields.lineOf("effect"),
                              "unknown effect '" + effect + "'");
            }
        }
        else if (def.itemClass == ItemClass::Weapon)
        {
            readDamage(fields, def);
            def.thrown = fields.yesNo("thrown");
        }
        else if (def.itemClass == ItemClass::Bow)
        {
            // A bow's damage is what its arrows do
            readDamage(fields, def);
            def.range = fields.number("range", 1);
        }
        else if (def.itemClass == ItemClass::Missile)
        {
            // An arrow does its bow's damage; a missile that's thrown
            // has damage of its own
            std::vector<int> found = fields.numbers("found", 2, 1);
            def.minFound = found[0];
            def.maxFound = found[1];
            if (def.minFound > def.maxFound)
            {
                fields.report(fields.lineOf("found"), "'found' runs backward");
            }
            def.thrown = fields.yesNo("thrown");
            if (def.thrown)
            {
                readDamage(fields, def);
            }
        }
        else if (def.itemClass == ItemClass::Armor)
        {
            def.defense = fields.number("defense", 0);
            def.noisy = fields.yesNo("noisy");
        }
        checkNewId(fields, def.id, idLines);

        if (fields.finish())
        {
            items_.push_back(def);
        }
    }

    // The checks of the file as a whole wait until the records are all
    // good, as the monsters' do
    if (errors_.size() > errorsBefore)
    {
        return;
    }
    for (const char* id : NEEDED_ITEMS)
    {
        if (findItem(id) == nullptr)
        {
            errors_.push_back(file + ": the game needs an item with the id '" +
                              id + "'");
        }
    }

    // Items are chosen by frequency, so some must be above 0. And each
    // kind of potion needs a color of its own
    int total = 0;
    int potions = 0;
    for (const ItemDef& def : items_)
    {
        total += def.frequency;
        if (def.itemClass == ItemClass::Potion)
        {
            ++potions;
        }
    }
    if (total == 0)
    {
        errors_.push_back(file + ": every frequency is 0, so no item can "
                          "be chosen");
    }
    if (potions > potionColorCount())
    {
        errors_.push_back(file + ": there are " + std::to_string(potions) +
                          " kinds of potion, but only " +
                          std::to_string(potionColorCount()) +
                          " colors for them");
    }
}

// Every [vault] record in vaults.txt, checked, and kept if it has no
// mistakes. A guardian is named by its id, so the monsters are read first.
// Each floor with an Amulet needs a vault for it
void GameData::readVaults(const std::string& folder)
{
    const std::string file = "vaults.txt";
    size_t errorsBefore = errors_.size();
    std::map<std::string, int> idLines;
    for (const Record& record : readDataFile(folder, file, errors_))
    {
        FieldReader fields(record, file, errors_);
        if (record.type != "vault")
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }

        VaultDef def;
        def.id = fields.id("id");
        std::vector<int> floors = fields.numbers("floors", 2, 1);
        def.minFloor = floors[0];
        def.maxFloor = floors[1];
        if (def.minFloor > def.maxFloor)
        {
            fields.report(fields.lineOf("floors"), "'floors' runs backward");
        }
        def.amulet = fields.yesNo("amulet");
        if (fields.has("guardian"))
        {
            std::string id = fields.id("guardian");
            def.guardian = findMonster(id);
            if (def.guardian == nullptr && !id.empty())
            {
                fields.report(fields.lineOf("guardian"),
                              "no monster is called '" + id + "'");
            }
        }
        def.rows = fields.rows("map");
        checkVaultMap(fields, def);
        checkNewId(fields, def.id, idLines);

        if (fields.finish())
        {
            vaults_.push_back(def);
        }
    }

    // Each Amulet's floor needs an Amulet's vault, once the records are
    // all good
    if (errors_.size() > errorsBefore)
    {
        return;
    }
    for (int floor : AMULET_FLOORS)
    {
        bool found = false;
        for (const VaultDef& vault : vaults_)
        {
            found = found || (vault.amulet && vault.minFloor <= floor &&
                              floor <= vault.maxFloor);
        }
        if (!found)
        {
            errors_.push_back(file + ": floor " + std::to_string(floor) +
                              " has no Amulet's vault");
        }
    }
}
