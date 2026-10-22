#include "BotReport.h"
#include <algorithm>   // std::sort, for the causes of death
#include <cstdio>      // std::printf, for the report
#include <utility>     // std::pair, for a cause and its count

namespace
{
    // How many causes of death the report lists
    constexpr int CAUSES_SHOWN = 8;
}

void BotReport::add(const RunResult& run)
{
    if (!tallies_.contains(run.className))
    {
        classOrder_.push_back(run.className);
    }

    // The run counts for its class, and for them all
    for (Tally* tally : { &tallies_[run.className], &all_ })
    {
        ++tally->runs;
        tally->floors += run.floor;
        if (run.ending == "won")
        {
            ++tally->won;
        }
        else if (run.ending == "stuck")
        {
            ++tally->stuck;
        }
        else
        {
            ++tally->diedOn[run.floor];
        }
    }
    if (run.ending == "died")
    {
        ++causes_[run.killedBy];
    }
}

void BotReport::print() const
{
    std::printf("\nThe bot's %d runs\n\n", all_.runs);
    std::printf("%-8s %5s %5s %6s %6s   Deaths on floors 1 to %d\n",
                "Class", "Runs", "Won", "Stuck", "Floor", LAST_FLOOR);
    for (const std::string& name : classOrder_)
    {
        printRow(name, tallies_.at(name));
    }
    printRow("All", all_);

    // The causes of death, the most common first
    std::vector<std::pair<int, std::string>> causes;
    for (const auto& [cause, count] : causes_)
    {
        causes.push_back({ count, cause });
    }
    std::sort(causes.rbegin(), causes.rend());
    std::printf("\nWhat killed it most:\n");
    for (int i = 0; i < CAUSES_SHOWN && i < static_cast<int>(causes.size());
         ++i)
    {
        std::printf("%6d  %s\n", causes[i].first, causes[i].second.c_str());
    }
}

// A row of the report: the class, its runs, the share it won, the runs it
// got stuck in, the floor it reached on average, and its deaths on each
// floor
void BotReport::printRow(const std::string& name, const Tally& tally) const
{
    if (tally.runs == 0)
    {
        return;
    }
    int wonPercent = (100 * tally.won + tally.runs / 2) / tally.runs;
    double floor = static_cast<double>(tally.floors) / tally.runs;
    std::printf("%-8s %5d %4d%% %6d %6.1f  ", name.c_str(), tally.runs,
                wonPercent, tally.stuck, floor);
    for (int depth = 1; depth <= LAST_FLOOR; ++depth)
    {
        std::printf(" %3d", tally.diedOn[depth]);
    }
    std::printf("\n");
}
