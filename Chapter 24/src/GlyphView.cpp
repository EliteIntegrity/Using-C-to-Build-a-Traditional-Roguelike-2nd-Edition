#include "GlyphView.h"

GlyphView::GlyphView(SDL_Renderer* renderer, const GlyphCache& glyphs)
    : View(CELL_PX), renderer_(renderer), glyphs_(glyphs)
{
}

// Floor is a dot, the stairs down a >, and wall a # on dark stone.
// Remembered cells are drawn in the dimmer, cooler colors
void GlyphView::drawTerrain(Point cell, Terrain terrain, bool lit) const
{
    if (terrain == Terrain::Floor)
    {
        SDL_Color color = lit ? Palette::FLOOR : Palette::MEMORY_FLOOR;
        drawCell(cell, '.', color);
    }
    else if (terrain == Terrain::CaveFloor)
    {
        // Earth, in a brown that's dimmed to the same memory color
        SDL_Color color = lit ? Palette::CAVE_FLOOR : Palette::MEMORY_FLOOR;
        drawCell(cell, '.', color);
    }
    else if (terrain == Terrain::StairsDown)
    {
        SDL_Color color = lit ? Palette::STAIRS : Palette::MEMORY_WALL;
        drawCell(cell, '>', color);
    }
    else if (terrain == Terrain::ClosedDoor)
    {
        // A closed door fills its gap in the wall, so it has the wall's
        // background
        SDL_Color color = lit ? Palette::DOOR : Palette::MEMORY_WALL;
        SDL_Color back = lit ? Palette::WALL_BACK : Palette::MEMORY_WALL_BACK;
        drawCell(cell, '+', color, back);
    }
    else if (terrain == Terrain::OpenDoor)
    {
        SDL_Color color = lit ? Palette::DOOR : Palette::MEMORY_WALL;
        drawCell(cell, '\'', color);
    }
    else
    {
        SDL_Color color = lit ? Palette::WALL : Palette::MEMORY_WALL;
        SDL_Color back = lit ? Palette::WALL_BACK : Palette::MEMORY_WALL_BACK;
        drawCell(cell, '#', color, back);
    }
}

// Every class is an @ in the glyph view, so the class isn't needed, and
// its parameter has no name
void GlyphView::drawPlayer(Point cell, const ClassDef&) const
{
    drawCell(cell, '@', Palette::PLAYER);
}

void GlyphView::drawMonster(Point cell, const MonsterDef& def) const
{
    drawCell(cell, def.glyph, def.color);
}

// A remembered item keeps its character, in the memory's cool color
void GlyphView::drawItem(Point cell, const ItemLook& look, bool lit) const
{
    drawCell(cell, look.glyph, lit ? look.color : Palette::MEMORY_WALL);
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

// A trap is a ^ in its kind's color, or in the memory's color
void GlyphView::drawTrap(Point cell, SDL_Color color, bool lit) const
{
    drawCell(cell, '^', lit ? color : Palette::MEMORY_WALL);
}
