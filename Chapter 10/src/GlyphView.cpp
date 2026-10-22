#include "GlyphView.h"

GlyphView::GlyphView(SDL_Renderer* renderer, const GlyphCache& glyphs)
    : View(CELL_PX), renderer_(renderer), glyphs_(glyphs)
{
}

// Floor is a dot, and wall a # on dark stone. Remembered cells are drawn
// in the dimmer, cooler colors
void GlyphView::drawTerrain(Point cell, Terrain terrain, bool lit) const
{
    if (terrain == Terrain::Floor)
    {
        SDL_Color color = lit ? Palette::FLOOR : Palette::MEMORY_FLOOR;
        drawCell(cell, '.', color);
    }
    else
    {
        SDL_Color color = lit ? Palette::WALL : Palette::MEMORY_WALL;
        SDL_Color back = lit ? Palette::WALL_BACK : Palette::MEMORY_WALL_BACK;
        drawCell(cell, '#', color, back);
    }
}

void GlyphView::drawPlayer(Point cell) const
{
    drawCell(cell, '@', Palette::PLAYER);
}

void GlyphView::drawMonster(Point cell, const MonsterDef& def) const
{
    drawCell(cell, def.glyph, def.color);
}

// One map cell, where the camera puts it: its background first, so
// nothing drawn there before can show through, then its character
void GlyphView::drawCell(Point cell, char c, SDL_Color color,
                         SDL_Color background) const
{
    if (!camera_.shows(cell))
    {
        return;
    }
    SDL_FRect rect = camera_.cellRect(cell);
    SDL_SetRenderDrawColor(renderer_, background.r, background.g,
                           background.b, background.a);
    SDL_RenderFillRect(renderer_, &rect);
    glyphs_.drawInCell(c, rect.x, rect.y, rect.w, color);
}
