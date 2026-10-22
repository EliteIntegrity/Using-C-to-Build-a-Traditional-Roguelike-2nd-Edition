#include "Words.h"
#include <SDL3/SDL.h>

std::string withArticle(const std::string& name)
{
    const std::string vowels = "aeiou";
    bool vowel = !name.empty() &&
                 vowels.find(static_cast<char>(SDL_tolower(name[0]))) !=
                 std::string::npos;
    return (vowel ? "an " : "a ") + name;
}

std::string capitalized(std::string text)
{
    if (!text.empty())
    {
        text[0] = static_cast<char>(SDL_toupper(text[0]));
    }
    return text;
}

std::string listed(const std::vector<std::string>& words)
{
    std::string text;
    for (size_t i = 0; i < words.size(); ++i)
    {
        if (i > 0)
        {
            text += i + 1 == words.size() ? " and " : ", ";
        }
        text += words[i];
    }
    return text;
}
