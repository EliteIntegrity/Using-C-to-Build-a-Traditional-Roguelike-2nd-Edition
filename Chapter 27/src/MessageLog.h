#pragma once
#include <string>   // std::string, for the messages
#include <vector>   // std::vector, to keep them in

// Everything the game has told the player, oldest first. A message that
// repeats the one before it isn't added again: its count goes up instead
class MessageLog
{
public:
    struct Entry
    {
        std::string text;
        int count = 1;
        bool isNew = true;   // added since the player's last command

        // The text as the log shows it, with " x3" after a message that
        // came three times in a row
        std::string withCount() const;
    };

    void add(const std::string& text);

    // Marks every message as read, so only those added after this are new
    void markRead();

    const std::vector<Entry>& entries() const;

private:
    // The oldest messages are dropped once there are this many
    static constexpr size_t MAX_ENTRIES = 100;

    std::vector<Entry> entries_;
};
