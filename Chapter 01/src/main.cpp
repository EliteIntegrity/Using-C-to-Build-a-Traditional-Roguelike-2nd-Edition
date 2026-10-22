// Amulets of SDL: the traditional roguelike that "Using C++ to Build a
// Traditional Roguelike" builds, one checkpoint at a time
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "Common.h"
#include "GlyphCache.h"

namespace
{
    // The window starts as big as the picture, or smaller, so that it takes
    // up no more than this share of the screen
    constexpr float MAX_SCREEN_SHARE = 0.9f;

    // The font, and its size in points. The path is relative to the working
    // directory, which Visual Studio sets to the project's folder
    const char* const FONT_PATH = "assets/RobotoMono-Light.ttf";
    constexpr float FONT_SIZE = 16.0f;

    void setColor(SDL_Renderer* renderer, SDL_Color color)
    {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    }

    // One map cell: its background first, so nothing drawn there before can
    // show through, then its character
    void drawCell(SDL_Renderer* renderer, const GlyphCache& glyphs,
                  int col, int row, char c, SDL_Color color,
                  SDL_Color background = Palette::BACKGROUND)
    {
        float x = static_cast<float>(col * CELL_PX);
        float y = static_cast<float>(row * CELL_PX);
        float size = static_cast<float>(CELL_PX);
        SDL_FRect cell = { x, y, size, size };
        setColor(renderer, background);
        SDL_RenderFillRect(renderer, &cell);
        glyphs.drawInCell(c, x, y, size, color);
    }

    // A room to test the drawing with: walls around a floor, and the player
    // in the middle
    void drawTestRoom(SDL_Renderer* renderer, const GlyphCache& glyphs)
    {
        const int left = 10;
        const int top = 6;
        const int right = 21;
        const int bottom = 13;
        for (int row = top; row <= bottom; ++row)
        {
            for (int col = left; col <= right; ++col)
            {
                bool edge = row == top || row == bottom ||
                            col == left || col == right;
                if (edge)
                {
                    drawCell(renderer, glyphs, col, row, '#', Palette::WALL,
                             Palette::WALL_BACK);
                }
                else
                {
                    drawCell(renderer, glyphs, col, row, '.', Palette::FLOOR);
                }
            }
        }
        drawCell(renderer, glyphs, (left + right) / 2, (top + bottom) / 2,
                 '@', Palette::PLAYER);
    }

    // The side panel and the message log, each with a line along the edge
    // it shares with the map
    void drawPanels(SDL_Renderer* renderer, const GlyphCache& glyphs)
    {
        setColor(renderer, Palette::PANEL);
        SDL_FRect panel = { static_cast<float>(PANEL_X), 0.0f,
                            static_cast<float>(PANEL_W),
                            static_cast<float>(MAP_VIEW_H) };
        SDL_RenderFillRect(renderer, &panel);
        SDL_FRect log = { 0.0f, static_cast<float>(LOG_Y),
                          static_cast<float>(SCREEN_W),
                          static_cast<float>(LOG_H) };
        SDL_RenderFillRect(renderer, &log);

        setColor(renderer, Palette::FRAME);
        SDL_RenderLine(renderer, static_cast<float>(PANEL_X), 0.0f,
                       static_cast<float>(PANEL_X),
                       static_cast<float>(MAP_VIEW_H));
        SDL_RenderLine(renderer, 0.0f, static_cast<float>(LOG_Y),
                       static_cast<float>(SCREEN_W),
                       static_cast<float>(LOG_Y));

        glyphs.drawText("AMULETS OF SDL", PANEL_X + MARGIN, MARGIN,
                        Palette::TEXT);
        glyphs.drawText("Messages will appear here.", MARGIN,
                        LOG_Y + MARGIN, Palette::DIM_TEXT);
    }

    void drawFrame(SDL_Renderer* renderer, const GlyphCache& glyphs)
    {
        setColor(renderer, Palette::BACKGROUND);
        SDL_RenderClear(renderer);
        drawTestRoom(renderer, glyphs);
        drawPanels(renderer, glyphs);
        SDL_RenderPresent(renderer);
    }

    void runGame(SDL_Renderer* renderer, const GlyphCache& glyphs)
    {
        bool running = true;
        bool redraw = true;
        while (running)
        {
            // Sleep until something happens. A turn-based game has nothing
            // to do while the player is thinking
            SDL_Event event;
            if (!SDL_WaitEvent(&event))
            {
                SDL_Log("SDL_WaitEvent failed: %s", SDL_GetError());
                break;
            }

            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                     event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                redraw = true;
            }

            if (redraw)
            {
                drawFrame(renderer, glyphs);
                redraw = false;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    if (!TTF_Init())
    {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Shrink the window if the screen is too small to show it whole
    float scale = 1.0f;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
    {
        float fitWidth = MAX_SCREEN_SHARE * usable.w / SCREEN_W;
        float fitHeight = MAX_SCREEN_SHARE * usable.h / SCREEN_H;
        scale = SDL_min(1.0f, SDL_min(fitWidth, fitHeight));
    }
    int windowW = static_cast<int>(SCREEN_W * scale);
    int windowH = static_cast<int>(SCREEN_H * scale);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Amulets of SDL", windowW, windowH,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer))
    {
        SDL_Log("Couldn't create the window: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    int result = 0;
    {
        GlyphCache glyphs(renderer, FONT_PATH, FONT_SIZE);
        if (glyphs.isLoaded())
        {
            runGame(renderer, glyphs);
        }
        else
        {
            result = 1;
        }
    }   // The glyphs are destroyed here, before the renderer that made them

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return result;
}
