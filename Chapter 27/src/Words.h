#pragma once
#include <string>   // std::string, for the words
#include <vector>   // std::vector, for a list of them

// Small helpers with the game's words, which the game and its morgue file
// share

// The name with "a" or "an" before it: "a jackal", but "an ogre", and "an
// Archer"
std::string withArticle(const std::string& name);

// The same text, with its first letter a capital, to start a sentence
std::string capitalized(std::string text);

// Words as a list: "a dagger, leather armor and a short bow"
std::string listed(const std::vector<std::string>& words);
