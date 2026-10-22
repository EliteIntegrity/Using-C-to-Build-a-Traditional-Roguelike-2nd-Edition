#pragma once
#include <random>   // std::mt19937 and std::uniform_int_distribution
#include <string>   // std::string, for the dice's state as text

// The dice. Everything random in a run comes from one Rng, started from
// the run's seed, so the same seed always plays out the same way
class Rng
{
public:
    explicit Rng(unsigned int seed);

    // A whole number from low to high, both included
    int roll(int low, int high);

    // The dice's state, as text, and setting it back from that text, so a
    // saved run rolls on just as it would have. setState changes nothing,
    // and returns false, if the text isn't a state
    std::string state() const;
    bool setState(const std::string& text);

private:
    std::mt19937 engine_;
};
