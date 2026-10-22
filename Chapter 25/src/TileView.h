#pragma once
#include <SDL3/SDL.h>
#include <string>          // std::string, for the tiles' names
#include <unordered_map>   // std::unordered_map, to find tiles by name
#include "View.h"

// The map in pictures: TILE_PX tiles from one picture, tiles.png, found
// by the names that tiles.txt gives them
class TileView : public View
{
public:
    TileView(SDL_Renderer* renderer, const std::string& atlasPath,
             const std::string& indexPath);
    ~TileView();

    TileView(const TileView&) = delete;
    TileView& operator=(const TileView&) = delete;

    bool isLoaded() const;

    // A tile, by its name, anywhere on the screen and at any size, as the
    // menus show the classes
    void drawPicture(const std::string& name, SDL_FRect where) const;

    void drawTerrain(Point cell, Terrain terrain, bool lit) const override;
    void drawPlayer(Point cell, const ClassDef& def) const override;
    void drawMonster(Point cell, const MonsterDef& def) const override;
    void drawItem(Point cell, const ItemLook& look, bool lit) const override;
    void drawTrap(Point cell, SDL_Color color, bool lit) const override;

private:
    bool readIndex(const std::string& path);
    void drawTile(Point cell, const std::string& name,
                  SDL_Color tint) const;
    void drawTileAt(SDL_FRect where, const std::string& name,
                    SDL_Color tint) const;

    SDL_Renderer* renderer_;
    SDL_Texture* atlas_ = nullptr;

    // Where each tile is in the atlas, by name
    std::unordered_map<std::string, SDL_FRect> tiles_;
    bool loaded_ = false;
};
