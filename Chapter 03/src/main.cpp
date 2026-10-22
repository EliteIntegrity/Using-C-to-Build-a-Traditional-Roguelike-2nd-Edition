// Amulets of SDL: the traditional roguelike that "Using C++ to Build a
// Traditional Roguelike" builds, one checkpoint at a time
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <random>   // std::random_device and std::mt19937, for the dice
#include <string>   // std::string and std::to_string, for the panel
#include <vector>   // std::vector, for the rooms and the log
#include "Common.h"
#include "GlyphCache.h"
#include "Map.h"
#include "MessageLog.h"
#include "Player.h"
#include "RoomGen.h"

namespace
{
    // The window starts as big as the picture, or smaller, so that it takes
    // up no more than this share of the screen
    constexpr float MAX_SCREEN_SHARE = 0.9f;

    // The font, and its size in points. The path is relative to the working
    // directory, which Visual Studio sets to the project's folder
    const char* const FONT_PATH = "assets/RobotoMono-Light.ttf";
    constexpr float FONT_SIZE = 16.0f;

    // The keys that move the player, and the step each one takes. The
    // arrow keys and the numeric keypad are matched by where they are on
    // the keyboard (their scancodes), so Num Lock makes no difference. The
    // letters are matched by what they type (their keycodes), as in many
    // roguelikes: h, j, k and l go left, down, up and right, and y, u, b and
    // n go diagonally
    struct KeyStep
    {
        SDL_Scancode arrow;    // SDL_SCANCODE_UNKNOWN for a diagonal
        SDL_Scancode keypad;
        SDL_Keycode letter;
        Point step;
    };

    constexpr KeyStep KEY_STEPS[] = {
        { SDL_SCANCODE_UP, SDL_SCANCODE_KP_8, SDLK_K, { 0, -1 } },
        { SDL_SCANCODE_DOWN, SDL_SCANCODE_KP_2, SDLK_J, { 0, 1 } },
        { SDL_SCANCODE_LEFT, SDL_SCANCODE_KP_4, SDLK_H, { -1, 0 } },
        { SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_6, SDLK_L, { 1, 0 } },
        { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_7, SDLK_Y, { -1, -1 } },
        { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_9, SDLK_U, { 1, -1 } },
        { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_1, SDLK_B, { -1, 1 } },
        { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_3, SDLK_N, { 1, 1 } },
    };

    // Finds the step a key asks for, if it's one of the moving keys
    bool stepFor(const SDL_KeyboardEvent& key, Point& step)
    {
        for (const KeyStep& keyStep : KEY_STEPS)
        {
            bool isArrow = keyStep.arrow != SDL_SCANCODE_UNKNOWN &&
                           key.scancode == keyStep.arrow;
            if (isArrow || key.scancode == keyStep.keypad ||
                key.key == keyStep.letter)
            {
                step = keyStep.step;
                return true;
            }
        }
        return false;
    }

    // How many of the window's pixels each pixel of the game's picture
    // covers: 1.6 when the 1600 x 900 picture fills a 2560 x 1440 screen
    float presentationScale(SDL_Renderer* renderer)
    {
        SDL_FRect shown = {};
        if (!SDL_GetRenderLogicalPresentationRect(renderer, &shown))
        {
            return 1.0f;
        }
        return shown.w / SCREEN_W;
    }

    // Switches between the window and the whole screen
    void toggleFullscreen(SDL_Renderer* renderer)
    {
        SDL_Window* window = SDL_GetRenderWindow(renderer);
        bool isFullscreen =
            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(window, !isFullscreen);
    }

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

    // The map: floor everywhere it's been carved, and walls only where
    // they border it
    void drawMap(SDL_Renderer* renderer, const GlyphCache& glyphs,
                 const Map& map)
    {
        for (int y = 0; y < MAP_H; ++y)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                if (map.at(x, y) == Terrain::Floor)
                {
                    drawCell(renderer, glyphs, x, y, '.', Palette::FLOOR);
                }
                else if (map.bordersFloor(x, y))
                {
                    drawCell(renderer, glyphs, x, y, '#', Palette::WALL,
                             Palette::WALL_BACK);
                }
            }
        }
    }

    // The side panel and the message log, each with a line along the edge
    // it shares with the map
    void drawPanels(SDL_Renderer* renderer)
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
    }

    // The side panel's text: the game's name, then the run so far, one row
    // under another
    void drawSidePanel(const GlyphCache& glyphs, int turn, unsigned int seed)
    {
        float x = PANEL_X + MARGIN;
        float y = MARGIN;
        float row = glyphs.lineHeight();
        glyphs.drawText("AMULETS OF SDL", x, y, Palette::TEXT);
        y += 2 * row;
        glyphs.drawText("Turn " + std::to_string(turn), x, y, Palette::TEXT);
        y += row;
        glyphs.drawText("Seed " + std::to_string(seed), x, y,
                        Palette::DIM_TEXT);
    }

    // The last LOG_LINES messages, oldest at the top. The ones that came
    // since the player's last command are bright, and the rest are dim
    void drawLog(const GlyphCache& glyphs, const MessageLog& log)
    {
        const std::vector<MessageLog::Entry>& entries = log.entries();
        int count = static_cast<int>(entries.size());
        int first = SDL_max(0, count - LOG_LINES);
        float y = LOG_Y + (LOG_H - LOG_LINES * LOG_LINE_H) / 2.0f;
        for (int i = first; i < count; ++i)
        {
            const MessageLog::Entry& entry = entries[i];
            SDL_Color color = entry.isNew ? Palette::TEXT : Palette::DIM_TEXT;
            glyphs.drawText(entry.withCount(), MARGIN, y, color);
            y += LOG_LINE_H;
        }
    }

    void drawFrame(SDL_Renderer* renderer, const GlyphCache& glyphs,
                   const Map& map, const Player& player,
                   const MessageLog& log, int turn, unsigned int seed)
    {
        setColor(renderer, Palette::BACKGROUND);
        SDL_RenderClear(renderer);
        drawMap(renderer, glyphs, map);
        drawCell(renderer, glyphs, player.pos.x, player.pos.y, '@',
                 Palette::PLAYER);
        drawPanels(renderer);
        drawSidePanel(glyphs, turn, seed);
        drawLog(glyphs, log);
        SDL_RenderPresent(renderer);
    }

    void runGame(SDL_Renderer* renderer, GlyphCache& glyphs,
                 unsigned int firstSeed)
    {
        // The window may already be smaller than the picture
        glyphs.setScale(presentationScale(renderer));

        // Everything random in a run comes from one engine, started from
        // one seed: the same seed always gives the same dungeon
        std::random_device entropy;
        unsigned int seed = firstSeed;
        if (seed == 0)
        {
            seed = entropy();
        }
        std::mt19937 rng(seed);

        // The dungeon, and the player, in the middle of its first room
        Map map;
        std::vector<SDL_Rect> rooms = RoomGen::generate(map, rng);
        Player player;
        player.pos = RoomGen::centerOf(rooms.front());

        // A turn passes each time the player acts
        int turn = 0;
        MessageLog log;
        log.add("Welcome to the dungeon.");
        log.add("Move with the arrow keys, the numpad, or h j k l y u b n.");

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
            else if (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_RETURN &&
                     (event.key.mod & SDL_KMOD_ALT) != 0)
            {
                // Alt+Enter. Holding the keys down doesn't flicker between
                // the window and the screen, because repeats are ignored
                if (!event.key.repeat)
                {
                    toggleFullscreen(renderer);
                }
            }
            else if (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_SPACE)
            {
                // A new dungeon, from a new seed
                seed = entropy();
                rng.seed(seed);
                rooms = RoomGen::generate(map, rng);
                player.pos = RoomGen::centerOf(rooms.front());
                turn = 0;
                log.markRead();
                log.add("You enter a new dungeon.");
                redraw = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                // A step takes a turn. Bumping into a wall doesn't
                Point step;
                if (stepFor(event.key, step))
                {
                    log.markRead();
                    Point to = { player.pos.x + step.x, player.pos.y + step.y };
                    if (map.at(to.x, to.y) == Terrain::Floor)
                    {
                        player.pos = to;
                        ++turn;
                    }
                    else
                    {
                        log.add("You bump into the wall.");
                    }
                    redraw = true;
                }
            }
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                // The picture is shown at a new size, so the characters
                // are drawn again to match it
                glyphs.setScale(presentationScale(renderer));
                redraw = true;
            }
            else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                     event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                redraw = true;
            }

            if (redraw)
            {
                drawFrame(renderer, glyphs, map, player, log, turn, seed);
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
        GlyphCache glyphs(renderer, FONT_PATH, FONT_SIZE);
        if (glyphs.isLoaded())
        {
            runGame(renderer, glyphs, seed);
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
