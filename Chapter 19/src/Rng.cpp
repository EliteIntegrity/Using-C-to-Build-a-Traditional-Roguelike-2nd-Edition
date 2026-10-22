#include "Rng.h"
#include <sstream>   // std::ostringstream and std::istringstream, for the state

Rng::Rng(unsigned int seed)
    : engine_(seed)
{
}

int Rng::roll(int low, int high)
{
    return std::uniform_int_distribution<int>(low, high)(engine_);
}

// The standard engines write their state to a stream, and read it back,
// as whole numbers with spaces between: 624 of them for a std::mt19937
std::string Rng::state() const
{
    std::ostringstream out;
    out << engine_;
    return out.str();
}

bool Rng::setState(const std::string& text)
{
    std::istringstream in(text);
    std::mt19937 engine;
    in >> engine;
    if (in.fail() || !(in >> std::ws).eof())
    {
        return false;
    }
    engine_ = engine;
    return true;
}
