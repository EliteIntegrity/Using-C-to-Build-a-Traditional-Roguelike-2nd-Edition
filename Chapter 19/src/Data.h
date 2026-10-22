#pragma once
#include <string>   // std::string, for paths and error messages
#include <vector>   // std::vector, for the kinds and the errors
#include "Item.h"
#include "Monster.h"

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

    // The kind of monster or item with this id, or nullptr if there's none
    const MonsterDef* findMonster(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;

    const std::vector<std::string>& errors() const;

private:
    void readMonsters(const std::string& folder);
    void readItems(const std::string& folder);

    std::vector<MonsterDef> monsters_;
    std::vector<ItemDef> items_;
    std::vector<std::string> errors_;
};
