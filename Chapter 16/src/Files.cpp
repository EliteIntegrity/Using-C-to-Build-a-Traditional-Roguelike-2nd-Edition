#include "Files.h"
#include <SDL3/SDL.h>

std::string resourcePath(const std::string& relative)
{
    // SDL works the folder out the first time it's asked, and keeps it.
    // The text is SDL's, so it isn't freed here. It ends with a separator,
    // ready for a file's name to be added
    const char* base = SDL_GetBasePath();
    if (base == nullptr)
    {
        SDL_Log("Couldn't find the game's folder: %s", SDL_GetError());
        return relative;
    }
    return base + relative;
}

bool readTextFile(const std::string& path, std::string& text)
{
    size_t size = 0;
    void* contents = SDL_LoadFile(path.c_str(), &size);
    if (contents == nullptr)
    {
        return false;
    }
    text.assign(static_cast<const char*>(contents), size);
    SDL_free(contents);

    // Windows ends each line of text with "\r\n", and other systems with
    // "\n". Removing every \r leaves one kind of line ending to deal with
    std::erase(text, '\r');
    return true;
}
