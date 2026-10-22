#pragma once
#include <SDL3/SDL.h>

// The game always draws a picture this size. SDL scales it to fit the
// window, whatever size the window is
constexpr int SCREEN_W = 1600;
constexpr int SCREEN_H = 900;

// The map is a grid of square cells, MAP_W across and MAP_H down
constexpr int CELL_PX = 20;
constexpr int MAP_W = 64;
constexpr int MAP_H = 36;

// The tile view's cells are bigger: the size of one of its pictures
constexpr int TILE_PX = 32;

// The screen's three areas: the map at the top left, the side panel to its
// right, and the message log across the bottom
constexpr int MAP_VIEW_W = MAP_W * CELL_PX;
constexpr int MAP_VIEW_H = MAP_H * CELL_PX;
constexpr int PANEL_X = MAP_VIEW_W;
constexpr int PANEL_W = SCREEN_W - MAP_VIEW_W;
constexpr int LOG_Y = MAP_VIEW_H;
constexpr int LOG_H = SCREEN_H - MAP_VIEW_H;

// Space between the panels' edges and their text
constexpr int MARGIN = 12;

// The message log shows this many lines, LOG_LINE_H pixels apart
constexpr int LOG_LINES = 8;
constexpr int LOG_LINE_H = 20;

// A cell's place on the map: x across, y down
struct Point
{
    int x = 0;
    int y = 0;

    // Two points are equal when their x and y both are. C++20 writes this
    // comparison for us, member by member
    bool operator==(const Point& other) const = default;
};

// The steps from a cell to its eight neighbors, starting north and going
// clockwise
constexpr Point DIRECTIONS[] = {
    { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
    { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 },
};

namespace Palette
{
    constexpr SDL_Color BACKGROUND = { 12, 12, 18, 255 };
    constexpr SDL_Color PANEL = { 22, 22, 30, 255 };
    constexpr SDL_Color FRAME = { 64, 64, 80, 255 };
    constexpr SDL_Color TEXT = { 220, 220, 230, 255 };
    constexpr SDL_Color DIM_TEXT = { 130, 130, 145, 255 };
    constexpr SDL_Color PLAYER = { 255, 220, 120, 255 };
    constexpr SDL_Color WALL = { 150, 140, 120, 255 };
    constexpr SDL_Color WALL_BACK = { 44, 40, 34, 255 };
    constexpr SDL_Color FLOOR = { 90, 90, 100, 255 };

    // Remembered cells, out of sight: dimmer, and cooler, as if moonlit
    constexpr SDL_Color MEMORY_WALL = { 70, 74, 96, 255 };
    constexpr SDL_Color MEMORY_WALL_BACK = { 24, 25, 34, 255 };
    constexpr SDL_Color MEMORY_FLOOR = { 48, 52, 72, 255 };

    // The same for tiles: a remembered tile's colors are multiplied by
    // this, which darkens them, and the blue the least. Not by much, or
    // the dark floor tile would vanish into the unexplored dark
    constexpr SDL_Color MEMORY_TINT = { 150, 160, 210, 255 };

    // Notes from the debugging aids, in a color the game never uses
    constexpr SDL_Color DEBUG = { 240, 110, 200, 255 };

    // Health: plenty, getting low, and in danger
    constexpr SDL_Color HEALTH_GOOD = { 90, 180, 90, 255 };
    constexpr SDL_Color HEALTH_LOW = { 225, 170, 60, 255 };
    constexpr SDL_Color DANGER = { 220, 70, 60, 255 };

    // See-through black, to fade whatever is drawn behind it
    constexpr SDL_Color SHADE = { 0, 0, 0, 170 };
}
