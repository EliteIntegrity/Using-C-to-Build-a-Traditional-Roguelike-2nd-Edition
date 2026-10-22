#include "GlyphCache.h"
#include <cmath>   // std::floor, to keep each character on whole pixels

GlyphCache::GlyphCache(SDL_Renderer* renderer, const std::string& fontPath,
                       float pointSize)
    : renderer_(renderer), pointSize_(pointSize)
{
    // The font stays open, so the characters can be drawn again at a new
    // size whenever the window changes size
    font_ = TTF_OpenFont(fontPath.c_str(), pointSize);
    if (font_ == nullptr)
    {
        SDL_Log("Couldn't open the font %s: %s", fontPath.c_str(),
                SDL_GetError());
        return;
    }

    // How far each character moves the pen, and how tall a line is. These
    // are in the game's own 1600 x 900 picture, so they never change
    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    int advance = 0;
    TTF_GetGlyphMetrics(font_, 'M', &minX, &maxX, &minY, &maxY, &advance);
    charWidth_ = static_cast<float>(advance);
    lineHeight_ = static_cast<float>(TTF_GetFontHeight(font_));

    renderGlyphs();
    loaded_ = true;
}

GlyphCache::~GlyphCache()
{
    destroyGlyphs();
    if (font_ != nullptr)
    {
        TTF_CloseFont(font_);
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

void GlyphCache::setScale(float scale)
{
    if (font_ == nullptr || scale <= 0.0f || scale == scale_)
    {
        return;
    }
    scale_ = scale;
    renderGlyphs();
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

// Draws every character at the size the window shows it: a picture shown
// at 1.6 times its size needs characters 1.6 times as big
void GlyphCache::renderGlyphs()
{
    destroyGlyphs();
    TTF_SetFontSize(font_, pointSize_ * scale_);

    const SDL_Color white = { 255, 255, 255, 255 };
    for (char c = FIRST_CHAR; c <= LAST_CHAR; ++c)
    {
        SDL_Surface* surface =
            TTF_RenderGlyph_Blended(font_, static_cast<Uint32>(c), white);
        if (surface == nullptr)
        {
            SDL_Log("Couldn't draw '%c': %s", c, SDL_GetError());
            continue;
        }
        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
        if (texture == nullptr)
        {
            SDL_Log("Couldn't make a texture for '%c': %s", c,
                    SDL_GetError());
            continue;
        }

        // Each pixel of the texture lands on one pixel of the window, so
        // it's copied as it is, never blended with its neighbors
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        glyphs_[c - FIRST_CHAR] = texture;
    }
}

void GlyphCache::destroyGlyphs()
{
    for (SDL_Texture*& texture : glyphs_)
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
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

    // The texture is scale_ times the character's size in the picture, so
    // it's drawn that much smaller, and the window scales it back up
    float width = 0.0f;
    float height = 0.0f;
    SDL_GetTextureSize(texture, &width, &height);
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_FRect where = { std::floor(x), std::floor(y), width / scale_,
                        height / scale_ };
    SDL_RenderTexture(renderer_, texture, nullptr, &where);
}
