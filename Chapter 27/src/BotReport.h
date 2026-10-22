#pragma once
#include <map>      // std::map, for the classes and the causes of death
#include <string>   // std::string, for their names
#include <vector>   // std::vector, for the classes, in order
#include "Bot.h"
#include "Common.h"

// The bot's runs, added up: for each class, how many it played, won and
// got stuck in, the floors it reached, and the floor of each death; and
// what killed it most
class BotReport
{
public:
    // Adds a run to the report
    void add(const RunResult& run);

    // Prints the report: a row for each class, and one for them all, then
    // the causes of death, the most common first
    void print() const;

private:
    // What's added up, for a class or for them all
    struct Tally
    {
        int runs = 0;
        int won = 0;
        int stuck = 0;
        int floors = 0;                    // the floors reached, added up
        int diedOn[LAST_FLOOR + 1] = {};   // the deaths on each floor
    };

    void printRow(const std::string& name, const Tally& tally) const;

    std::vector<std::string> classOrder_;   // as they first played
    std::map<std::string, Tally> tallies_;
    Tally all_;
    std::map<std::string, int> causes_;
};
