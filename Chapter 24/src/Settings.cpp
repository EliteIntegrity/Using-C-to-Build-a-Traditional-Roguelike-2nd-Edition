#include "Settings.h"
#include <SDL3/SDL.h>
#include <string>   // std::string, for the file's name and text
#include <vector>   // std::vector, for the records and the mistakes
#include "Files.h"
#include "Records.h"

namespace
{
    const std::string SETTINGS_FILE = "settings.txt";
}

Settings loadSettings()
{
    Settings settings;
    std::string text;
    if (!readTextFile(userPath(SETTINGS_FILE), text))
    {
        return settings;
    }
    std::vector<std::string> errors;
    for (const Record& record : readRecords(text, SETTINGS_FILE, errors))
    {
        FieldReader fields(record, SETTINGS_FILE, errors);
        if (record.type != "settings")
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }
        // A value with a mistake in it is reported, and the setting keeps
        // its default: a slip of the keyboard mustn't change a setting
        if (fields.has("shake"))
        {
            size_t mistakes = errors.size();
            bool shake = fields.yesNo("shake");
            if (errors.size() == mistakes)
            {
                settings.shake = shake;
            }
        }
        fields.finish();
    }
    for (const std::string& error : errors)
    {
        SDL_Log("%s", error.c_str());
    }
    return settings;
}

bool saveSettings(const Settings& settings)
{
    std::string text = "# Amulets of SDL's settings. The game writes this "
                       "file when a setting\n# changes.\n\n[settings]\n";
    text += std::string("shake = ") + (settings.shake ? "yes" : "no") +
            "\n";
    return writeTextFile(userPath(SETTINGS_FILE), text);
}
