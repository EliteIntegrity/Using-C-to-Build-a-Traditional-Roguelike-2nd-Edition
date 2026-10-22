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

// A cell's place on the map: x across, y down
struct Point
{
    int x = 0;
    int y = 0;
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
}
