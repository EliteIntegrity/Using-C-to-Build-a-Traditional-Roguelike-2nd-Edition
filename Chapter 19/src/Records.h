#pragma once
#include <SDL3/SDL.h>
#include <map>      // std::map, for a record's keys
#include <set>      // std::set, for the keys a record has been asked for
#include <string>   // std::string, for the keys, values and mistakes
#include <vector>   // std::vector, for the records, numbers and mistakes

// The text format of the game's data files: records, each a "[type]" line
// and the "key = value" lines under it, as in
//
//     [monster]
//     id = cave_orc
//     hp = 16
//
// A line starting with # is a comment, and blank lines are skipped. A #
// anywhere else is part of the line, since a wall is drawn as one. Every
// mistake is reported as "file:line: what's wrong"

// One key's value from a record, and the line it was on
struct Field
{
    std::string value;
    int line = 0;
};

// One record: its type, as in [monster], the line it starts on, and its
// keys
struct Record
{
    std::string type;
    int line = 0;
    std::map<std::string, Field> fields;
};

// The records in a file's text. The file's name is for the mistakes, which
// go into errors
std::vector<Record> readRecords(const std::string& text,
                                const std::string& file,
                                std::vector<std::string>& errors);

// True if the text is one whole number and nothing else, which goes in
// number
bool readWholeNumber(const std::string& text, int& number);

// Takes a record's values apart, a key at a time, and reports what's wrong
// with each: a key that's missing, or a value that isn't what it should
// be. When it's done, any key it was never asked for is unknown
class FieldReader
{
public:
    FieldReader(const Record& record, const std::string& file,
                std::vector<std::string>& errors);

    // The value of a key that must be there
    std::string text(const std::string& key);

    // True if the record has the key, which may be left out
    bool has(const std::string& key);

    // The line a key is on, or the record's first line if it isn't there
    int lineOf(const std::string& key) const;

    // An id: lowercase letters, digits and _, as in cave_orc, so it's one
    // word wherever it's written
    std::string id(const std::string& key);

    // Exactly count whole numbers, none of them less than least
    std::vector<int> numbers(const std::string& key, int count, int least);
    int number(const std::string& key, int least);

    // A key that may be left out, which is yes or no. Left out, it's no
    bool yesNo(const std::string& key);

    // One character the font can draw, other than a space
    char glyph(const std::string& key);

    // A color: its red, green and blue, each from 0 to 255
    SDL_Color color(const std::string& key);

    // Reports a mistake on one of the record's lines
    void report(int line, const std::string& what);

    // Reports the keys never asked for, which no record of this type has.
    // True if the record had no mistakes at all
    bool finish();

private:
    const Field* find(const std::string& key);

    const Record& record_;
    std::string file_;
    std::vector<std::string>& errors_;
    std::set<std::string> asked_;
    bool ok_ = true;
};
