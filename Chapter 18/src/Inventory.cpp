#include "Inventory.h"

bool Inventory::add(const Item& item)
{
    for (Item& carried : items_)
    {
        if (item.def->stacks() && carried.def == item.def)
        {
            carried.count += item.count;
            return true;
        }
    }
    if (static_cast<int>(items_.size()) >= SLOTS)
    {
        return false;
    }
    items_.push_back(item);
    return true;
}

const std::vector<Item>& Inventory::items() const
{
    return items_;
}

Item Inventory::removeStack(int slot)
{
    Item stack = items_[slot];
    items_.erase(items_.begin() + slot);
    return stack;
}

void Inventory::removeOne(int slot)
{
    --items_[slot].count;
    if (items_[slot].count == 0)
    {
        items_.erase(items_.begin() + slot);
    }
}

char Inventory::letterFor(int slot)
{
    return static_cast<char>('a' + slot);
}
