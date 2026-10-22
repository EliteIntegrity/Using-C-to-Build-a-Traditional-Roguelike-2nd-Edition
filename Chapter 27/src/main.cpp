// Amulets of SDL: the traditional roguelike that "Using C++ to Build a
// Traditional Roguelike" builds, one checkpoint at a time
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>   // std::string, for the mistakes in the data files
#include <vector>   // std::vector, for the list of them
#include "Bot.h"
#include "Common.h"
#include "Data.h"
#include "Files.h"
#include "Game.h"
#include "GlyphCache.h"
#include "SoundBank.h"

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

    // A number on the command line, such as a seed
    unsigned int numberFrom(const char* text)
    {
        return static_cast<unsigned int>(SDL_strtoul(text, nullptr, 10));
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

    // "AmuletsOfSDL --hits" prints the table of hits to kill, worked out
    // from the data files, and stops: it needs no window
    if (argc > 1 && SDL_strcmp(argv[1], "--hits") == 0)
    {
        Game::printHitsToKill(data);
        return 0;
    }

    // A seed on the command line replays that dungeon, as in
    // "AmuletsOfSDL 12345". Without one, the seed is chosen at random.
    // "AmuletsOfSDL --bot 20" has the bot play 20 runs as each class, from
    // seed 1, or from a seed given after the number, as "--bot 20 500",
    // and as one class only, if its id comes after that: "--bot 20 1 mage"
    unsigned int seed = 0;
    int botRuns = 0;
    std::string botClass;
    if (argc > 2 && SDL_strcmp(argv[1], "--bot") == 0)
    {
        botRuns = static_cast<int>(numberFrom(argv[2]));
        seed = argc > 3 ? numberFrom(argv[3]) : 1;
        botClass = argc > 4 ? argv[4] : "";
    }
    else if (argc > 1)
    {
        seed = numberFrom(argv[1]);
    }

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    // Sound is welcome, but the game plays on without it. The bot plays
    // without it always
    if (botRuns == 0 && !SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        SDL_Log("No sound: %s", SDL_GetError());
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
    // Retina display, it also gets to use every pixel the screen has. For
    // the bot, which needs one to make a game but draws nothing, it's
    // hidden
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (botRuns > 0)
    {
        flags |= SDL_WINDOW_HIDDEN;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Amulets of SDL", windowW, windowH,
                                     flags, &window, &renderer))
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
        GlyphCache glyphs(renderer, resourcePath(FONT_PATH), FONT_SIZE);
        SoundBank sounds(data);
        if (glyphs.isLoaded() && botRuns > 0)
        {
            // The bot plays in a folder of its own, so its runs never
            // touch the player's save, morgue files or high scores
            useUserFolder("AmuletsOfSDL-bot");
            playBotRuns(renderer, glyphs, data, sounds, botRuns, seed,
                        botClass);
        }
        else if (glyphs.isLoaded())
        {
            // Each run is a new Game, with everything in it made afresh.
            // Only the first uses the seed from the command line, and only
            // the first starts at the title screen: playing again goes
            // straight to the class menu. Each starts in the view the last
            // one ended in
            bool playAgain = true;
            bool firstRun = true;
            bool useTiles = false;
            while (playAgain)
            {
                Game game(renderer, glyphs, data, sounds, seed, useTiles,
                          firstRun);
                playAgain = game.run();
                useTiles = game.usesTiles();
                seed = 0;
                firstRun = false;
            }
        }
        else
        {
            result = 1;
        }
    }   // The sounds and the glyphs are destroyed here, the glyphs before
        // the renderer that made them

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return result;
}
