#pragma once
#include <SDL3/SDL.h>
#include "Common.h"

// Which part of the map the map view shows: a window onto the map,
// measured in the map's own pixels, at cellSize pixels a cell. It follows
// a cell, but never shows past the map's edges, so a map that fits the
// view (the glyph view's) never moves at all
class Camera
{
public:
    explicit Camera(int cellSize);

    int cellSize() const;

    // Centers the view on a cell, as near as the map's edges allow
    void follow(Point cell);

    // Where a cell is drawn on the screen
    SDL_FRect cellRect(Point cell) const;

    // True if any part of the cell is inside the map view
    bool shows(Point cell) const;

private:
    int cellSize_;
    float left_ = 0.0f;   // the map's pixel at the view's left edge
    float top_ = 0.0f;    // and the one at its top edge
};
