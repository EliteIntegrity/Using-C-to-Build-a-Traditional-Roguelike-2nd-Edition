#include "TileView.h"
#include <fstream>   // std::ifstream, to read tiles.txt
#include <sstream>   // std::istringstream, to read each of its lines

namespace
{
    // Full color: the tile drawn as it is
    constexpr SDL_Color NO_TINT = { 255, 255, 255, 255 };

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
    std::ifstream file(path);
    if (!file)
    {
        SDL_Log("Couldn't open %s", path.c_str());
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
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
// view's memory colors do
void TileView::drawTerrain(Point cell, Terrain terrain, bool lit) const
{
    const char* name = terrain == Terrain::Floor ? "floor" : "wall";
    drawTile(cell, name, lit ? NO_TINT : Palette::MEMORY_TINT);
}

void TileView::drawPlayer(Point cell) const
{
    drawTile(cell, "player", NO_TINT);
}

void TileView::drawMonster(Point cell, const MonsterDef& def) const
{
    drawTile(cell, def.tile, NO_TINT);
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
