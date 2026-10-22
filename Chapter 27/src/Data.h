#pragma once
#include <string>   // std::string, for paths and error messages
#include <vector>   // std::vector, for the kinds and the errors
#include "Item.h"
#include "Monster.h"
#include "Player.h"
#include "SoundEvent.h"

// A vault: a room made by hand, read from vaults.txt. Its map has a row
// of text for each row of cells, all as wide as the widest:
//   # wall    . floor    + door    S secret door    ^ trap
//   $ treasure    m a monster from the floor's band    G a guardian
//   A the Amulet, in an Amulet's vault    (a space is outside the vault)
struct VaultDef
{
    std::string id;
    int minFloor = 1;                       // the floors it can be on
    int maxFloor = 1;
    bool amulet = false;                    // an Amulet's, not treasure
    const MonsterDef* guardian = nullptr;   // what each G is, if any
    std::vector<std::string> rows;

    int width() const
    {
        return rows.empty() ? 0 : static_cast<int>(rows.front().size());
    }

    int height() const
    {
        return static_cast<int>(rows.size());
    }
};

// The files an event plays, from sounds.txt: one, chosen at random, each
// time
struct SoundDef
{
    SoundEvent event = SoundEvent::Hit;
    std::vector<std::string> files;
};

// The game's content, read from the text files in its data folder, so it
// can be changed without recompiling. Every mistake in a file is reported
// with the file and the line it's on, and a file with mistakes stops the
// game: nothing is quietly given a value it wasn't
class GameData
{
public:
    // Reads every data file in the folder. Returns false if any has a
    // mistake; errors() lists them all
    bool load(const std::string& folder);

    const std::vector<MonsterDef>& monsters() const;
    const std::vector<ItemDef>& items() const;
    const std::vector<VaultDef>& vaults() const;
    const std::vector<ClassDef>& classes() const;
    const std::vector<SoundDef>& sounds() const;

    // The kind of monster or item, or the class, with this id, or nullptr
    // if there's none
    const MonsterDef* findMonster(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
    const ClassDef* findClass(const std::string& id) const;

    const std::vector<std::string>& errors() const;

private:
    void readMonsters(const std::string& folder);
    void readItems(const std::string& folder);
    void readVaults(const std::string& folder);
    void readClasses(const std::string& folder);
    void readSounds(const std::string& folder);

    std::vector<MonsterDef> monsters_;
    std::vector<ItemDef> items_;
    std::vector<VaultDef> vaults_;
    std::vector<ClassDef> classes_;
    std::vector<SoundDef> sounds_;
    std::vector<std::string> errors_;
};
