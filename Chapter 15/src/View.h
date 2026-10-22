#pragma once
#include "Camera.h"
#include "Common.h"
#include "Map.h"
#include "Monster.h"

// How an item looks: a character and its color in the glyph view, and a
// tile in the tile view. A potion's or a scroll's tile is tinted with the
// same color; a weapon's keeps its own colors
struct ItemLook
{
    char glyph;
    SDL_Color color;
    const char* tile;
    bool tinted;
};

// The way the map is drawn. The game decides what's on each cell, and a
// View draws it there: GlyphView with characters, and TileView with
// pictures. Each has its own camera, since their cells differ in size
class View
{
public:
    explicit View(int cellSize)
        : camera_(cellSize)
    {
    }

    virtual ~View() = default;

    Camera& camera()
    {
        return camera_;
    }

    const Camera& camera() const
    {
        return camera_;
    }

    // A cell's terrain: lit if the player can see it, and otherwise as the
    // player remembers it
    virtual void drawTerrain(Point cell, Terrain terrain, bool lit) const = 0;

    // The player, and a monster, on the cell they stand on
    virtual void drawPlayer(Point cell) const = 0;
    virtual void drawMonster(Point cell, const MonsterDef& def) const = 0;

    // An item on the floor: lit if the player can see it, and otherwise as
    // the player remembers it
    virtual void drawItem(Point cell, const ItemLook& look,
                          bool lit) const = 0;

protected:
    Camera camera_;
};
