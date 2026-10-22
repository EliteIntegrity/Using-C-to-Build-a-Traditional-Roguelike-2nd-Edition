#include "GlyphCache.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>   // std::floor, to keep each character on whole pixels

GlyphCache::GlyphCache(SDL_Renderer* renderer, const std::string& fontPath,
                       float pointSize)
    : renderer_(renderer)
{
    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), pointSize);
    if (font == nullptr)
    {
        SDL_Log("Couldn't open the font %s: %s", fontPath.c_str(),
                SDL_GetError());
        return;
    }

    const SDL_Color white = { 255, 255, 255, 255 };
    for (char c = FIRST_CHAR; c <= LAST_CHAR; ++c)
    {
        SDL_Surface* surface =
            TTF_RenderGlyph_Blended(font, static_cast<Uint32>(c), white);
        if (surface == nullptr)
        {
            SDL_Log("Couldn't draw '%c': %s", c, SDL_GetError());
            continue;
        }
        glyphs_[c - FIRST_CHAR] =
            SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
    }

    // How far each character moves the pen, and how tall a line is
    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    int advance = 0;
    TTF_GetGlyphMetrics(font, 'M', &minX, &maxX, &minY, &maxY, &advance);
    charWidth_ = static_cast<float>(advance);
    lineHeight_ = static_cast<float>(TTF_GetFontHeight(font));

    TTF_CloseFont(font);
    loaded_ = true;
}

GlyphCache::~GlyphCache()
{
    for (SDL_Texture* texture : glyphs_)
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
        }
    }
}

bool GlyphCache::isLoaded() const
{
    return loaded_;
}

float GlyphCache::charWidth() const
{
    return charWidth_;
}

float GlyphCache::lineHeight() const
{
    return lineHeight_;
}

void GlyphCache::drawInCell(char c, float x, float y, float cellSize,
                            SDL_Color color) const
{
    float left = x + (cellSize - charWidth_) / 2.0f;
    float top = y + (cellSize - lineHeight_) / 2.0f;
    drawAt(c, left, top, color);
}

void GlyphCache::drawText(const std::string& text, float x, float y,
                          SDL_Color color) const
{
    for (size_t i = 0; i < text.size(); ++i)
    {
        drawAt(text[i], x + i * charWidth_, y, color);
    }
}

void GlyphCache::drawAt(char c, float x, float y, SDL_Color color) const
{
    // Anything the cache doesn't hold is drawn as a question mark
    if (c < FIRST_CHAR || c > LAST_CHAR)
    {
        c = '?';
    }
    SDL_Texture* texture = glyphs_[c - FIRST_CHAR];
    if (texture == nullptr)
    {
        return;
    }

    float width = 0.0f;
    float height = 0.0f;
    SDL_GetTextureSize(texture, &width, &height);
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_FRect where = { std::floor(x), std::floor(y), width, height };
    SDL_RenderTexture(renderer_, texture, nullptr, &where);
}
