#include "TileView.h"
#include <sstream>   // std::istringstream, to read tiles.txt's lines
#include "Files.h"

namespace
{
    // Full color: the tile drawn as it is
    constexpr SDL_Color NO_TINT = { 255, 255, 255, 255 };

    // The dirt tile is bright and busy for a floor, so a cave's floor is
    // drawn darker, and what stands on it stays easy to see
    constexpr SDL_Color DIRT_TINT = { 150, 140, 130, 255 };

    // A tint, with the memory's tint on top of it, for something
    // remembered: the two are multiplied, as color mods multiply
    SDL_Color remembered(SDL_Color tint)
    {
        tint.r = static_cast<Uint8>(tint.r * Palette::MEMORY_TINT.r / 255);
        tint.g = static_cast<Uint8>(tint.g * Palette::MEMORY_TINT.g / 255);
        tint.b = static_cast<Uint8>(tint.b * Palette::MEMORY_TINT.b / 255);
        return tint;
    }

    // Each tile sits in a slot 2 pixels bigger than itself, with a 1-pixel
    // border that copies its edge pixels. Scaling blends a little across a
    // tile's edges, and the border keeps that blend to the tile's own
    // colors, so no seams show between tiles
    constexpr int TILE_BORDER = 1;
    constexpr int TILE_SLOT = TILE_PX + 2 * TILE_BORDER;
}

TileView::TileView(SDL_Renderer* renderer, const std::string& atlasPath,
                   const std::string& indexPath)
    : View(TILE_PX), renderer_(renderer)
{
    SDL_Surface* surface = SDL_LoadPNG(atlasPath.c_str());
    if (surface == nullptr)
    {
        SDL_Log("Couldn't load %s: %s", atlasPath.c_str(), SDL_GetError());
        return;
    }
    atlas_ = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (atlas_ == nullptr)
    {
        SDL_Log("Couldn't make a texture of %s: %s", atlasPath.c_str(),
                SDL_GetError());
        return;
    }

    // Pixel art stays crisp at any size: each pixel of a tile becomes a
    // sharp block, with only its edges smoothed when the scale isn't whole
    SDL_SetTextureScaleMode(atlas_, SDL_SCALEMODE_PIXELART);
    loaded_ = readIndex(indexPath);
}

TileView::~TileView()
{
    if (atlas_ != nullptr)
    {
        SDL_DestroyTexture(atlas_);
    }
}

bool TileView::isLoaded() const
{
    return loaded_;
}

// Reads lines like "kobold 5 0": a tile's name, then the column and row of
// its slot in the atlas. Lines that start with # are comments, and blank
// lines are skipped
bool TileView::readIndex(const std::string& path)
{
    std::string text;
    if (!readTextFile(path, text))
    {
        SDL_Log("Couldn't read %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    std::istringstream lines(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream fields(line);
        std::string name;
        int col = 0;
        int row = 0;
        if (!(fields >> name >> col >> row))
        {
            SDL_Log("%s:%d: expected a name, a column and a row",
                    path.c_str(), lineNumber);
            return false;
        }
        tiles_[name] = { static_cast<float>(col * TILE_SLOT + TILE_BORDER),
                         static_cast<float>(row * TILE_SLOT + TILE_BORDER),
                         static_cast<float>(TILE_PX),
                         static_cast<float>(TILE_PX) };
    }
    return true;
}

// Remembered cells are tinted, which darkens and cools them, as the glyph
// view's memory colors do. The stairs' tile is see-through around its
// edges, so a floor is drawn under it first
void TileView::drawTerrain(Point cell, Terrain terrain, bool lit) const
{
    const char* name = "wall";
    SDL_Color tint = NO_TINT;
    if (terrain == Terrain::Floor)
    {
        name = "floor";
    }
    else if (terrain == Terrain::CaveFloor)
    {
        name = "dirt";
        tint = DIRT_TINT;
    }
    else if (terrain == Terrain::StairsDown)
    {
        drawTile(cell, "floor", lit ? NO_TINT : remembered(NO_TINT));
        name = "stairs";
    }
    drawTile(cell, name, lit ? tint : remembered(tint));
}

void TileView::drawPlayer(Point cell) const
{
    drawTile(cell, "player", NO_TINT);
}

void TileView::drawMonster(Point cell, const MonsterDef& def) const
{
    drawTile(cell, def.tile, NO_TINT);
}

// An item's tile is tinted with its color, if it takes one
void TileView::drawItem(Point cell, const ItemLook& look, bool lit) const
{
    SDL_Color tint = look.tinted ? look.color : NO_TINT;
    drawTile(cell, look.tile, lit ? tint : remembered(tint));
}

// One tile on its cell, where the camera puts it. A name tiles.txt doesn't
// have is drawn as a square in the debugging color, so the mistake can't
// go unnoticed
void TileView::drawTile(Point cell, const std::string& name,
                        SDL_Color tint) const
{
    if (!camera_.shows(cell))
    {
        return;
    }
    SDL_FRect where = camera_.cellRect(cell);
    auto found = tiles_.find(name);
    if (found == tiles_.end())
    {
        SDL_SetRenderDrawColor(renderer_, Palette::DEBUG.r, Palette::DEBUG.g,
                               Palette::DEBUG.b, Palette::DEBUG.a);
        SDL_RenderFillRect(renderer_, &where);
        return;
    }
    SDL_SetTextureColorMod(atlas_, tint.r, tint.g, tint.b);
    SDL_RenderTexture(renderer_, atlas_, &found->second, &where);
}
