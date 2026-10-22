#pragma once
#include <random>   // std::mt19937 and std::uniform_int_distribution

// The dice. Everything random in a run comes from one Rng, started from
// the run's seed, so the same seed always plays out the same way
class Rng
{
public:
    explicit Rng(unsigned int seed);

    // A whole number from low to high, both included
    int roll(int low, int high);

private:
    std::mt19937 engine_;
};
