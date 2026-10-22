#include "MessageLog.h"

std::string MessageLog::Entry::withCount() const
{
    if (count == 1)
    {
        return text;
    }
    return text + " x" + std::to_string(count);
}

void MessageLog::add(const std::string& text)
{
    // The same message again only raises the count, and makes it new
    if (!entries_.empty() && entries_.back().text == text)
    {
        ++entries_.back().count;
        entries_.back().isNew = true;
        return;
    }

    entries_.push_back({ text, 1, true });
    if (entries_.size() > MAX_ENTRIES)
    {
        entries_.erase(entries_.begin());
    }
}

void MessageLog::markRead()
{
    for (Entry& entry : entries_)
    {
        entry.isNew = false;
    }
}

const std::vector<MessageLog::Entry>& MessageLog::entries() const
{
    return entries_;
}
