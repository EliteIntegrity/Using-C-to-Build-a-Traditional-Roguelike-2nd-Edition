#pragma once
#include <string>   // std::string, for paths and the text of files

// Finding the files the game comes with, and the folder for the files it
// writes, and reading and writing them. SDL does the reading and writing,
// because it takes every path as UTF-8, on every system, which is how
// SDL_GetBasePath and SDL_GetPrefPath give their folders

// The full path of a file the game comes with, such as
// "data/monsters.txt". It's found from the folder the game's program is
// in, so the game finds its files however it's started
std::string resourcePath(const std::string& relative);

// Reads a whole text file into text, with every line ending in "\n".
// Returns false if the file can't be read
bool readTextFile(const std::string& path, std::string& text);

// The full path of a file the game writes, such as "save.txt", in the
// folder SDL gives the game for each user's own files. On Windows, that's
// AppData\Roaming\GameCodeSchool\AmuletsOfSDL in the user's folder
std::string userPath(const std::string& name);

// From now on, the files the game writes go in a folder of this name
// instead, beside the usual one. The bot plays in a folder of its own, so
// its runs never touch a player's save, morgue files or high scores
void useUserFolder(const std::string& name);

// Writes text to a file, whole or not at all. It's written to a file of
// its own first, which then takes the real file's place, so a crash part
// way through never leaves half a file. Returns false if it can't be
// written
bool writeTextFile(const std::string& path, const std::string& text);
