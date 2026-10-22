#pragma once
#include <SDL3/SDL.h>
#include "GlyphCache.h"
#include "View.h"

// The map in characters: # for walls, . for floor, @ for the player, and
// a letter for each monster, in 20-pixel cells
class GlyphView : public View
{
public:
    GlyphView(SDL_Renderer* renderer, const GlyphCache& glyphs);

    void drawTerrain(Point cell, Terrain terrain, bool lit) const override;
    void drawPlayer(Point cell) const override;
    void drawMonster(Point cell, const MonsterDef& def) const override;
    void drawItem(Point cell, const ItemLook& look, bool lit) const override;
    void drawTrap(Point cell, SDL_Color color, bool lit) const override;

private:
    void drawCell(Point cell, char c, SDL_Color color,
                  SDL_Color background = Palette::BACKGROUND) const;

    SDL_Renderer* renderer_;
    const GlyphCache& glyphs_;
};
