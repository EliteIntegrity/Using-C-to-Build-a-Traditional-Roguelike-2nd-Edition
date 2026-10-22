#include "Rng.h"

Rng::Rng(unsigned int seed)
    : engine_(seed)
{
}

int Rng::roll(int low, int high)
{
    return std::uniform_int_distribution<int>(low, high)(engine_);
}
