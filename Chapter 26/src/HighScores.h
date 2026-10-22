#pragma once
#include <string>   // std::string, for a run's class and fate
#include <vector>   // std::vector, for the scores

// One run's line in the high scores
struct HighScore
{
    int score = 0;
    std::string className;   // as in Fighter
    std::string fate;        // "won", or "killed by a jackal on floor 3"
    int turns = 0;
};

// The best runs, best first, at most KEPT of them, kept in scores.txt in
// the player's own folder
class HighScores
{
public:
    static constexpr int KEPT = 10;

    // Reads scores.txt. With no file yet, there are no scores. A mistake
    // is logged, and that score is left out: scores aren't worth stopping
    // the game for
    void load();

    // Adds a run, if it's one of the best. Returns its place, from 0 for
    // the best, or -1 if it didn't make the list
    int add(const HighScore& run);

    // Writes scores.txt. Returns false if it couldn't
    bool save() const;

    const std::vector<HighScore>& all() const;

private:
    std::vector<HighScore> scores_;
};
