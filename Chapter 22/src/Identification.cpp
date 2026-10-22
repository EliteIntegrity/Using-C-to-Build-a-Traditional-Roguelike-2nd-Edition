#include "Identification.h"
#include <iterator>   // std::begin, std::end and std::size, for the tables
#include <utility>    // std::swap, to shuffle the colors
#include <vector>     // std::vector, for the shuffled colors

namespace
{
    // The colors a potion can be. The data's checks make sure there are at
    // least as many as there are kinds of potion
    struct PotionColor
    {
        const char* name;
        SDL_Color color;
    };

    constexpr PotionColor POTION_COLORS[] = {
        { "crimson", { 220, 60, 60, 255 } },
        { "azure", { 80, 140, 255, 255 } },
        { "emerald", { 60, 200, 100, 255 } },
        { "golden", { 235, 195, 60, 255 } },
        { "violet", { 170, 100, 230, 255 } },
        { "murky", { 130, 115, 80, 255 } },
        { "silver", { 200, 200, 215, 255 } },
        { "amber", { 230, 140, 40, 255 } },
        { "pink", { 240, 130, 180, 255 } },
        { "teal", { 50, 180, 180, 255 } },
    };

    // The scrolls' titles are made of these, run together into words
    constexpr const char* SYLLABLES[] = {
        "ka", "zel", "mor", "thi", "van", "qua", "lor", "bex",
        "nim", "pho", "rax", "sul", "gor", "dra", "ush", "tel",
    };

    // A word of two or three syllables, in capitals
    std::string makeWord(Rng& rng)
    {
        int last = static_cast<int>(std::size(SYLLABLES)) - 1;
        int length = rng.roll(2, 3);
        std::string word;
        for (int i = 0; i < length; ++i)
        {
            word += SYLLABLES[rng.roll(0, last)];
        }
        for (char& c : word)
        {
            c = static_cast<char>(SDL_toupper(c));
        }
        return word;
    }
}

void Identification::randomize(const std::vector<ItemDef>& kinds, Rng& rng)
{
    appearances_.clear();
    known_.clear();

    // The colors in a random order, shuffled the way a pack of cards is:
    // each place from the last to the second swaps with one at random at
    // or before it. Then each kind of potion takes the next color
    std::vector<PotionColor> colors(std::begin(POTION_COLORS),
                                    std::end(POTION_COLORS));
    for (int i = static_cast<int>(colors.size()) - 1; i > 0; --i)
    {
        std::swap(colors[i], colors[rng.roll(0, i)]);
    }

    // True if some kind already has this title
    auto taken = [this](const std::string& title)
    {
        for (const auto& entry : appearances_)
        {
            if (entry.second.name == title)
            {
                return true;
            }
        }
        return false;
    };

    int nextColor = 0;
    for (const ItemDef& def : kinds)
    {
        if (def.itemClass == ItemClass::Potion)
        {
            const PotionColor& color = colors[nextColor];
            appearances_[&def] = { color.name, color.color };
            ++nextColor;
        }
        else if (def.itemClass == ItemClass::Scroll)
        {
            // Every scroll is the same color: only its title tells it
            // apart, so no two kinds may share one
            std::string title = makeWord(rng) + " " + makeWord(rng);
            while (taken(title))
            {
                title = makeWord(rng) + " " + makeWord(rng);
            }
            appearances_[&def] = { title, Palette::SCROLL };
        }
    }
}

const Appearance& Identification::appearanceOf(const ItemDef& def) const
{
    return appearances_.at(&def);
}

// A kind with no look, such as a weapon, isn't disguised: the player
// knows it at a glance
bool Identification::isKnown(const ItemDef& def) const
{
    return !appearances_.contains(&def) || known_.contains(&def);
}

void Identification::learn(const ItemDef& def)
{
    known_.insert(&def);
}

void Identification::setAppearance(const ItemDef& def,
                                   const Appearance& look)
{
    appearances_[&def] = look;
}

bool Identification::hasAppearance(const ItemDef& def) const
{
    return appearances_.contains(&def);
}

int potionColorCount()
{
    return static_cast<int>(std::size(POTION_COLORS));
}
