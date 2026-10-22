#pragma once
#include "Camera.h"
#include "Common.h"
#include "Map.h"
#include "Monster.h"

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

protected:
    Camera camera_;
};
