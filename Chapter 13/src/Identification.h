#pragma once
#include <SDL3/SDL.h>
#include <string>          // std::string, for the appearances' names
#include <unordered_map>   // std::unordered_map, for each kind's look
#include <unordered_set>   // std::unordered_set, for the kinds known
#include "Item.h"
#include "Rng.h"

// How the items of one kind look in a run, until the player knows what
// they are: a potion's color, or a scroll's title
struct Appearance
{
    std::string name;   // "crimson", or "ZELMOR THIKA"
    SDL_Color color;
};

// The identification game. Each run, every kind of potion gets a color at
// random, and every kind of scroll a title of nonsense, so the player
// can't know a potion of healing by its look. Using one names its kind,
// for the rest of the run
class Identification
{
public:
    // Gives every kind a new look, and forgets every kind known
    void randomize(Rng& rng);

    const Appearance& appearanceOf(const ItemDef& def) const;
    bool isKnown(const ItemDef& def) const;
    void learn(const ItemDef& def);

private:
    std::unordered_map<const ItemDef*, Appearance> appearances_;
    std::unordered_set<const ItemDef*> known_;
};
