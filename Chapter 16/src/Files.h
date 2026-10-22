#pragma once
#include <string>   // std::string, for paths and the text of files

// Finding the files the game comes with, and reading them. SDL does the
// reading, because it takes every path as UTF-8, on every system, which is
// how SDL_GetBasePath gives the game's folder

// The full path of a file the game comes with, such as
// "data/monsters.txt". It's found from the folder the game's program is
// in, so the game finds its files however it's started
std::string resourcePath(const std::string& relative);

// Reads a whole text file into text, with every line ending in "\n".
// Returns false if the file can't be read
bool readTextFile(const std::string& path, std::string& text);
