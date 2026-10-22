#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>    // std::array, for the glyphs
#include <string>   // std::string, for the font's path and for text

// Every printable character, from the space (code 32) to the tilde (126),
// drawn once from the font as a white picture, then tinted to whatever
// color it's drawn in
class GlyphCache
{
public:
    GlyphCache(SDL_Renderer* renderer, const std::string& fontPath,
               float pointSize);
    ~GlyphCache();

    GlyphCache(const GlyphCache&) = delete;
    GlyphCache& operator=(const GlyphCache&) = delete;

    bool isLoaded() const;
    float charWidth() const;
    float lineHeight() const;

    // Draws the characters again for a picture that the window shows at
    // this scale, so they stay crisp at any size
    void setScale(float scale);

    // One character, centered in the square cell whose top-left corner is
    // at (x, y)
    void drawInCell(char c, float x, float y, float cellSize,
                    SDL_Color color) const;

    // A line of text, with its top-left corner at (x, y)
    void drawText(const std::string& text, float x, float y,
                  SDL_Color color) const;

private:
    static constexpr char FIRST_CHAR = ' ';
    static constexpr char LAST_CHAR = '~';

    void renderGlyphs();
    void destroyGlyphs();
    void drawAt(char c, float x, float y, SDL_Color color) const;

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    float pointSize_ = 0.0f;
    float scale_ = 1.0f;
    std::array<SDL_Texture*, LAST_CHAR - FIRST_CHAR + 1> glyphs_ = {};
    float charWidth_ = 0.0f;    // every character is this wide, since the
    float lineHeight_ = 0.0f;   // font is monospaced
    bool loaded_ = false;
};
