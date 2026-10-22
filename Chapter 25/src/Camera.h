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

    // Where a cell is drawn on the screen, shaken, if the view is shaking
    SDL_FRect cellRect(Point cell) const;

    // Shakes the view: from now on, every cell is drawn this many pixels
    // off, until the shake is set back to { 0, 0 }. Where the mouse points
    // isn't shaken
    void setShake(SDL_FPoint offset);

    // True if any part of the cell is inside the map view
    bool shows(Point cell) const;

    // The cell at a point on the screen: the other way from cellRect
    Point cellAt(float x, float y) const;

private:
    int cellSize_;
    float left_ = 0.0f;   // the map's pixel at the view's left edge
    float top_ = 0.0f;    // and the one at its top edge
    SDL_FPoint shake_ = { 0.0f, 0.0f };
};
