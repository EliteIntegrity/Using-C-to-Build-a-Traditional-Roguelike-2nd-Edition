#include "HighScores.h"
#include <SDL3/SDL.h>
#include "Files.h"
#include "Records.h"

namespace
{
    const std::string SCORES_FILE = "scores.txt";
}

void HighScores::load()
{
    scores_.clear();
    std::string text;
    if (!readTextFile(userPath(SCORES_FILE), text))
    {
        return;
    }
    std::vector<std::string> errors;
    for (const Record& record : readRecords(text, SCORES_FILE, errors))
    {
        FieldReader fields(record, SCORES_FILE, errors);
        if (record.type != "score")
        {
            fields.report(record.line, "unknown record [" + record.type +
                          "]");
            continue;
        }
        HighScore run;
        run.score = fields.number("score", 0);
        run.className = fields.text("class");
        run.fate = fields.text("fate");
        run.turns = fields.number("turns", 0);
        if (fields.finish())
        {
            add(run);
        }
    }
    for (const std::string& error : errors)
    {
        SDL_Log("%s", error.c_str());
    }
}

// The run goes after every score at least as good as its own, so of two
// runs that scored the same, the earlier keeps the higher place. The list
// is then cut back to KEPT
int HighScores::add(const HighScore& run)
{
    size_t place = 0;
    while (place < scores_.size() && scores_[place].score >= run.score)
    {
        ++place;
    }
    if (place >= KEPT)
    {
        return -1;
    }
    scores_.insert(scores_.begin() + place, run);
    if (scores_.size() > KEPT)
    {
        scores_.pop_back();
    }
    return static_cast<int>(place);
}

bool HighScores::save() const
{
    std::string text = "# Amulets of SDL's high scores, best first. The game "
                       "writes this file\n# when a run ends.\n";
    for (const HighScore& run : scores_)
    {
        text += "\n[score]\n";
        text += "score = " + std::to_string(run.score) + "\n";
        text += "class = " + run.className + "\n";
        text += "fate = " + run.fate + "\n";
        text += "turns = " + std::to_string(run.turns) + "\n";
    }
    return writeTextFile(userPath(SCORES_FILE), text);
}

const std::vector<HighScore>& HighScores::all() const
{
    return scores_;
}
