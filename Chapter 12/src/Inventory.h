#pragma once
#include <vector>   // std::vector, for the items
#include "Item.h"

// What the player carries: up to SLOTS stacks of items, each shown with a
// letter, from a to t
class Inventory
{
public:
    static constexpr int SLOTS = 20;

    // Adds the items, onto a stack of the same kind if there is one.
    // Returns false, and adds nothing, if they need a slot and there's
    // none free
    bool add(const Item& item);

    const std::vector<Item>& items() const;

    // Takes one item from a slot's stack, and the slot itself once the
    // stack is gone, so the slots after it move up a letter
    void removeOne(int slot);

    // Takes a slot's whole stack out of the pack, and returns it
    Item removeStack(int slot);

    // The letter that goes with a slot: a for slot 0, b for 1, and so on
    static char letterFor(int slot);

private:
    std::vector<Item> items_;
};
