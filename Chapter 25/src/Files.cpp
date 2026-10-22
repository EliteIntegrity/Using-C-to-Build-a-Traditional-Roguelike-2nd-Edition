#include "Files.h"
#include <SDL3/SDL.h>

namespace
{
    // The names SDL_GetPrefPath files the game's own folder under: the
    // organization that makes the game, and the game
    const char* const ORGANIZATION = "GameCodeSchool";
    const char* const GAME = "AmuletsOfSDL";
}

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

std::string userPath(const std::string& name)
{
    // SDL makes the folder, if it isn't there yet. This text, unlike
    // SDL_GetBasePath's, is the game's to free
    char* folder = SDL_GetPrefPath(ORGANIZATION, GAME);
    if (folder == nullptr)
    {
        SDL_Log("Couldn't find a folder to write in: %s", SDL_GetError());
        return name;
    }
    std::string path = folder + name;
    SDL_free(folder);
    return path;
}

bool writeTextFile(const std::string& path, const std::string& text)
{
    // The text goes to a temporary file, and is flushed, so it's on the
    // disk itself, not waiting in memory. Then a rename replaces the old
    // file in one step, so the file is always either the old one, whole,
    // or the new one, whole, even if the computer loses its power
    std::string temporary = path + ".tmp";
    SDL_IOStream* file = SDL_IOFromFile(temporary.c_str(), "wb");
    if (file == nullptr)
    {
        return false;
    }
    size_t size = text.size();
    bool written = SDL_WriteIO(file, text.data(), size) == size &&
                   SDL_FlushIO(file);
    bool closed = SDL_CloseIO(file);
    return written && closed &&
           SDL_RenamePath(temporary.c_str(), path.c_str());
}
