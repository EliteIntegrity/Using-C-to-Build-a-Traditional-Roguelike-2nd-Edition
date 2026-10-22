#pragma once
#include <SDL3/SDL.h>
#include <string>          // std::string, for the appearances' names
#include <unordered_map>   // std::unordered_map, for each kind's look
#include <unordered_set>   // std::unordered_set, for the kinds known
#include <vector>          // std::vector, for the kinds
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
    // Gives every kind of potion and scroll a new look, and forgets every
    // kind known
    void randomize(const std::vector<ItemDef>& kinds, Rng& rng);

    const Appearance& appearanceOf(const ItemDef& def) const;
    bool isKnown(const ItemDef& def) const;
    void learn(const ItemDef& def);

    // Gives a kind the look it had in a saved run
    void setAppearance(const ItemDef& def, const Appearance& look);

    // True if the kind has a look. Every potion and scroll should
    bool hasAppearance(const ItemDef& def) const;

private:
    std::unordered_map<const ItemDef*, Appearance> appearances_;
    std::unordered_set<const ItemDef*> known_;
};

// How many colors there are for potions. Each kind of potion needs one of
// its own, so there can be no more kinds of potion than this
int potionColorCount();
