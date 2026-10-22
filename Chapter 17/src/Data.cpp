#include "Data.h"
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
    };

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
        { "gold", ItemClass::Gold },
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
    };

    // The kinds of item the game's own code asks for by id, which
    // items.txt must have: the player's first weapon, and gold
    const char* const NEEDED_ITEMS[] = { "short_sword", "gold" };
}

bool GameData::load(const std::string& folder)
{
    monsters_.clear();
    items_.clear();
    errors_.clear();
    readMonsters(folder);
    readItems(folder);
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

    // Every floor needs a kind of monster to fill it. A kind with a
    // mistake is left out, which could leave a floor empty for no real
    // reason, so this waits until the records are all good
    if (errors_.size() > errorsBefore)
    {
        return;
    }
    for (int floor = 1; floor <= LAST_FLOOR; ++floor)
    {
        bool found = false;
        for (const MonsterDef& def : monsters_)
        {
            found = found || (def.minFloor <= floor && floor <= def.maxFloor);
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
// effect for a potion or a scroll, damage for a weapon, and defense for
// armor, which may also be noisy
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
            std::vector<int> damage = fields.numbers("damage", 2, 0);
            def.minDamage = damage[0];
            def.maxDamage = damage[1];
            if (def.minDamage > def.maxDamage)
            {
                fields.report(fields.lineOf("damage"),
                              "'damage' runs backward");
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
