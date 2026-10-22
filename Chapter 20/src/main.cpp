// Amulets of SDL: the traditional roguelike that "Using C++ to Build a
// Traditional Roguelike" builds, one checkpoint at a time
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>   // std::string, for the mistakes in the data files
#include <vector>   // std::vector, for the list of them
#include "Common.h"
#include "Data.h"
#include "Files.h"
#include "Game.h"
#include "GlyphCache.h"

namespace
{
    // The window starts as big as the picture, or smaller, so that it takes
    // up no more than this share of the screen
    constexpr float MAX_SCREEN_SHARE = 0.9f;

    // The font, and its size in points
    const char* const FONT_PATH = "assets/RobotoMono-Light.ttf";
    constexpr float FONT_SIZE = 16.0f;

    // The folder the data files are in
    const char* const DATA_FOLDER = "data/";

    // A message box can only be so tall, so it lists this many mistakes at
    // most. The console window lists them all
    constexpr int MOST_ERRORS_SHOWN = 15;

    // Lists the mistakes in the data files in the console window, and in a
    // message box, which a player sees however they started the game. The
    // box can be shown before SDL_Init, and needs no window of the game's
    void showDataErrors(const std::vector<std::string>& errors)
    {
        std::string text = "The game can't start, because its data files "
                           "have mistakes:\n\n";
        int shown = 0;
        for (const std::string& error : errors)
        {
            SDL_Log("%s", error.c_str());
            if (shown < MOST_ERRORS_SHOWN)
            {
                text += error + "\n";
                ++shown;
            }
        }
        int more = static_cast<int>(errors.size()) - shown;
        if (more > 0)
        {
            text += "and " + std::to_string(more) + " more, listed in the "
                    "console window\n";
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Amulets of SDL",
                                 text.c_str(), nullptr);
    }
}

int main(int argc, char* argv[])
{
    // The game's content comes first. A mistake in it stops the game
    // before anything else starts, with every mistake listed
    GameData data;
    if (!data.load(resourcePath(DATA_FOLDER)))
    {
        showDataErrors(data.errors());
        return 1;
    }

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

    // The window can be resized. On a high-density screen, such as a Mac's
    // Retina display, it also gets to use every pixel the screen has
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Amulets of SDL", windowW, windowH,
                                     SDL_WINDOW_RESIZABLE |
                                     SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer))
    {
        SDL_Log("Couldn't create the window: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // A seed on the command line replays that dungeon, as in
    // "AmuletsOfSDL 12345". Without one, the seed is chosen at random
    unsigned int seed = 0;
    if (argc > 1)
    {
        seed = static_cast<unsigned int>(SDL_strtoul(argv[1], nullptr, 10));
    }

    int result = 0;
    {
        GlyphCache glyphs(renderer, resourcePath(FONT_PATH), FONT_SIZE);
        if (glyphs.isLoaded())
        {
            // Each run is a new Game, with everything in it made afresh.
            // Only the first uses the seed from the command line, and each
            // starts in the view the last one ended in
            bool playAgain = true;
            bool useTiles = false;
            while (playAgain)
            {
                Game game(renderer, glyphs, data, seed, useTiles);
                playAgain = game.run();
                useTiles = game.usesTiles();
                seed = 0;
            }
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
