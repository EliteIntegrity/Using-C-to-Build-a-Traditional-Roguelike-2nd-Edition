#include "Records.h"
#include <sstream>   // std::istringstream, to read lines, numbers and words

namespace
{
    // The text without the spaces at either end
    std::string trimmed(const std::string& text)
    {
        size_t first = text.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            return "";
        }
        size_t last = text.find_last_not_of(" \t");
        return text.substr(first, last - first + 1);
    }
}

std::vector<Record> readRecords(const std::string& text,
                                const std::string& file,
                                std::vector<std::string>& errors)
{
    std::vector<Record> records;
    std::istringstream lines(text);
    std::string line;
    int number = 0;
    while (std::getline(lines, line))
    {
        ++number;
        std::string where = file + ":" + std::to_string(number) + ": ";
        line = trimmed(line);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            Record record;
            record.type = line.substr(1, line.size() - 2);
            record.line = number;
            records.push_back(record);
            continue;
        }
        size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            errors.push_back(where + "expected [record] or key = value");
            continue;
        }
        if (records.empty())
        {
            errors.push_back(where + "a key comes before any [record]");
            continue;
        }

        std::string key = trimmed(line.substr(0, equals));
        std::string value = trimmed(line.substr(equals + 1));
        Record& record = records.back();
        if (record.fields.contains(key))
        {
            errors.push_back(where + "'" + key + "' is given twice");
            continue;
        }
        record.fields[key] = { value, number };
    }
    return records;
}

bool readWholeNumber(const std::string& text, int& number)
{
    std::istringstream in(text);
    return (in >> number) && (in >> std::ws).eof();
}

FieldReader::FieldReader(const Record& record, const std::string& file,
                         std::vector<std::string>& errors)
    : record_(record), file_(file), errors_(errors)
{
}

std::string FieldReader::text(const std::string& key)
{
    const Field* field = find(key);
    if (field == nullptr)
    {
        return "";
    }
    if (field->value.empty())
    {
        report(field->line, "'" + key + "' needs a value");
    }
    return field->value;
}

bool FieldReader::has(const std::string& key)
{
    asked_.insert(key);
    return record_.fields.contains(key);
}

int FieldReader::lineOf(const std::string& key) const
{
    auto found = record_.fields.find(key);
    return found != record_.fields.end() ? found->second.line : record_.line;
}

std::string FieldReader::id(const std::string& key)
{
    std::string value = text(key);
    for (char c : value)
    {
        bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                       c == '_';
        if (!allowed)
        {
            report(lineOf(key), "'" + key + "' should be lowercase letters, "
                   "digits and _");
            break;
        }
    }
    return value;
}

std::vector<int> FieldReader::numbers(const std::string& key, int count,
                                      int least)
{
    const Field* field = find(key);
    if (field == nullptr)
    {
        return std::vector<int>(count, least);
    }
    std::vector<int> values;
    std::istringstream in(field->value);
    std::string word;
    bool right = true;
    while (in >> word)
    {
        int n = 0;
        right = right && readWholeNumber(word, n);
        values.push_back(n);
    }
    right = right && static_cast<int>(values.size()) == count;
    for (int value : values)
    {
        right = right && value >= least;
    }
    if (!right)
    {
        std::string what = count == 1 ? "a whole number"
                                      : std::to_string(count) +
                                        " whole numbers";
        report(field->line, "'" + key + "' should be " + what + ", " +
               std::to_string(least) + " or more");
        return std::vector<int>(count, least);
    }
    return values;
}

int FieldReader::number(const std::string& key, int least)
{
    return numbers(key, 1, least).front();
}

bool FieldReader::yesNo(const std::string& key)
{
    if (!has(key))
    {
        return false;
    }
    std::string value = text(key);
    if (!value.empty() && value != "yes" && value != "no")
    {
        report(lineOf(key), "'" + key + "' should be yes or no");
    }
    return value == "yes";
}

char FieldReader::glyph(const std::string& key)
{
    std::string value = text(key);
    if (value.empty())
    {
        return '?';
    }
    if (value.size() != 1 || value[0] < '!' || value[0] > '~')
    {
        report(lineOf(key), "'" + key + "' should be one character");
        return '?';
    }
    return value[0];
}

SDL_Color FieldReader::color(const std::string& key)
{
    std::vector<int> parts = numbers(key, 3, 0);
    for (int part : parts)
    {
        if (part > 255)
        {
            report(lineOf(key), "'" + key + "' goes up to 255");
            return { 255, 255, 255, 255 };
        }
    }
    return { static_cast<Uint8>(parts[0]), static_cast<Uint8>(parts[1]),
             static_cast<Uint8>(parts[2]), 255 };
}

void FieldReader::report(int line, const std::string& what)
{
    errors_.push_back(file_ + ":" + std::to_string(line) + ": " + what);
    ok_ = false;
}

bool FieldReader::finish()
{
    for (const auto& [key, field] : record_.fields)
    {
        if (!asked_.contains(key))
        {
            report(field.line, "unknown key '" + key + "'");
        }
    }
    return ok_;
}

// The key's field, or nullptr, with the mistake reported, if the record
// doesn't have it
const Field* FieldReader::find(const std::string& key)
{
    asked_.insert(key);
    auto found = record_.fields.find(key);
    if (found == record_.fields.end())
    {
        report(record_.line, "[" + record_.type + "] needs '" + key + "'");
        return nullptr;
    }
    return &found->second;
}
