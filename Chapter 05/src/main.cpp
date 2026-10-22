// Amulets of SDL: the traditional roguelike that "Using C++ to Build a
// Traditional Roguelike" builds, one checkpoint at a time
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iterator>   // std::size, to count the kinds of monster
#include <random>     // std::random_device, for a random seed
#include <string>     // std::string and std::to_string, for the panel
#include <vector>     // std::vector, for the rooms, the log and monsters
#include "Common.h"
#include "Fov.h"
#include "GlyphCache.h"
#include "Map.h"
#include "MessageLog.h"
#include "Monster.h"
#include "Player.h"
#include "Rng.h"
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

    // How far the player can see, in cells
    constexpr int SIGHT_RADIUS = 9;

    // How many monsters a new dungeon starts with
    constexpr int MONSTER_COUNT = 8;

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

    // The monster standing on a cell, or nullptr if there's none
    Monster* monsterAt(std::vector<Monster>& monsters, Point pos)
    {
        for (Monster& monster : monsters)
        {
            if (monster.pos == pos)
            {
                return &monster;
            }
        }
        return nullptr;
    }

    // Puts MONSTER_COUNT monsters of random kinds on the floor of the
    // rooms, one to a cell. The first room is the player's, so it's left
    // empty
    std::vector<Monster> placeMonsters(const std::vector<SDL_Rect>& rooms,
                                       Rng& rng)
    {
        std::vector<Monster> monsters;
        int lastRoom = static_cast<int>(rooms.size()) - 1;
        int lastKind = static_cast<int>(std::size(MONSTER_DEFS)) - 1;
        if (lastRoom < 1)
        {
            return monsters;
        }

        for (int i = 0; i < MONSTER_COUNT; ++i)
        {
            // A few tries each: a cell that's taken just means another roll
            for (int tries = 0; tries < 20; ++tries)
            {
                const SDL_Rect& room = rooms[rng.roll(1, lastRoom)];
                Point pos = { room.x + rng.roll(0, room.w - 1),
                              room.y + rng.roll(0, room.h - 1) };
                if (monsterAt(monsters, pos) == nullptr)
                {
                    Monster monster;
                    monster.pos = pos;
                    monster.def = &MONSTER_DEFS[rng.roll(0, lastKind)];
                    monsters.push_back(monster);
                    break;
                }
            }
        }
        return monsters;
    }

    // The square of the distance between two cells. Squares compare the
    // same way as the distances do, without any square roots
    int distanceSquared(Point a, Point b)
    {
        int dx = a.x - b.x;
        int dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    // True if the monster can see the player. It can if the player can see
    // its cell, since sight is symmetric, and the player is within its
    // sight, measured as the field of view measures the player's
    bool seesPlayer(const Map& map, const Monster& monster,
                    const Player& player)
    {
        int sight = monster.def->sight;
        return map.isVisible(monster.pos.x, monster.pos.y) &&
               distanceSquared(monster.pos, player.pos) <=
                   sight * (sight + 1);
    }

    // True if a monster could step onto the cell: floor, with nobody on it
    bool isFree(const Map& map, std::vector<Monster>& monsters,
                const Player& player, Point cell)
    {
        return map.at(cell.x, cell.y) == Terrain::Floor &&
               cell != player.pos && monsterAt(monsters, cell) == nullptr;
    }

    // The monsters' turn. A monster that sees the player steps to the free
    // cell nearest them, if that's nearer than where it stands. The others
    // take a step in a random direction, if the cell there is free
    void moveMonsters(const Map& map, std::vector<Monster>& monsters,
                      const Player& player, Rng& rng)
    {
        for (Monster& monster : monsters)
        {
            if (seesPlayer(map, monster, player))
            {
                Point best = monster.pos;
                int bestDistance = distanceSquared(monster.pos, player.pos);
                for (Point step : DIRECTIONS)
                {
                    Point to = { monster.pos.x + step.x,
                                 monster.pos.y + step.y };
                    int distance = distanceSquared(to, player.pos);
                    if (distance < bestDistance &&
                        isFree(map, monsters, player, to))
                    {
                        best = to;
                        bestDistance = distance;
                    }
                }
                monster.pos = best;
            }
            else
            {
                Point step = DIRECTIONS[rng.roll(0, 7)];
                Point to = { monster.pos.x + step.x, monster.pos.y + step.y };
                if (isFree(map, monsters, player, to))
                {
                    monster.pos = to;
                }
            }
        }
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

    // The map as the player knows it: what they can see, in full color,
    // and what they remember, dimmed. With revealMap, a debugging aid, all
    // the rest is drawn as if remembered too. Walls are drawn only where
    // they border floor
    void drawMap(SDL_Renderer* renderer, const GlyphCache& glyphs,
                 const Map& map, bool revealMap)
    {
        for (int y = 0; y < MAP_H; ++y)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                bool visible = map.isVisible(x, y);
                if (!visible && !map.isExplored(x, y) && !revealMap)
                {
                    continue;
                }
                if (map.at(x, y) == Terrain::Floor)
                {
                    SDL_Color color = visible ? Palette::FLOOR
                                              : Palette::MEMORY_FLOOR;
                    drawCell(renderer, glyphs, x, y, '.', color);
                }
                else if (map.bordersFloor(x, y))
                {
                    SDL_Color color = visible ? Palette::WALL
                                              : Palette::MEMORY_WALL;
                    SDL_Color back = visible ? Palette::WALL_BACK
                                             : Palette::MEMORY_WALL_BACK;
                    drawCell(renderer, glyphs, x, y, '#', color, back);
                }
            }
        }
    }

    // The monsters the player can see. With revealMap, all of them
    void drawMonsters(SDL_Renderer* renderer, const GlyphCache& glyphs,
                      const Map& map, const std::vector<Monster>& monsters,
                      bool revealMap)
    {
        for (const Monster& monster : monsters)
        {
            if (map.isVisible(monster.pos.x, monster.pos.y) || revealMap)
            {
                drawCell(renderer, glyphs, monster.pos.x, monster.pos.y,
                         monster.def->glyph, monster.def->color);
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
    // under another, and a reminder when a debugging aid is on
    void drawSidePanel(const GlyphCache& glyphs, int turn, unsigned int seed,
                       bool revealMap)
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
        if (revealMap)
        {
            y += 2 * row;
            glyphs.drawText("DEBUG: map revealed (F1)", x, y, Palette::DEBUG);
        }
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
                   const Map& map, const std::vector<Monster>& monsters,
                   const Player& player, const MessageLog& log, int turn,
                   unsigned int seed, bool revealMap)
    {
        setColor(renderer, Palette::BACKGROUND);
        SDL_RenderClear(renderer);
        drawMap(renderer, glyphs, map, revealMap);
        drawMonsters(renderer, glyphs, map, monsters, revealMap);
        drawCell(renderer, glyphs, player.pos.x, player.pos.y, '@',
                 Palette::PLAYER);
        drawPanels(renderer);
        drawSidePanel(glyphs, turn, seed, revealMap);
        drawLog(glyphs, log);
        SDL_RenderPresent(renderer);
    }

    void runGame(SDL_Renderer* renderer, GlyphCache& glyphs,
                 unsigned int firstSeed)
    {
        // The window may already be smaller than the picture
        glyphs.setScale(presentationScale(renderer));

        // Everything random in a run comes from one Rng, started from one
        // seed: the same seed always gives the same dungeon and monsters
        std::random_device entropy;
        unsigned int seed = firstSeed;
        if (seed == 0)
        {
            seed = entropy();
        }
        Rng rng(seed);

        // The dungeon, its monsters, the player in the middle of the first
        // room, and what the player can see from there
        Map map;
        std::vector<SDL_Rect> rooms = RoomGen::generate(map, rng);
        std::vector<Monster> monsters = placeMonsters(rooms, rng);
        Player player;
        player.pos = RoomGen::centerOf(rooms.front());
        Fov::compute(map, player.pos, SIGHT_RADIUS);

        // A turn passes each time the player acts
        int turn = 0;
        MessageLog log;
        log.add("Welcome to the dungeon.");
        log.add("Move with the arrow keys, the numpad, or h j k l y u b n.");

        // A debugging aid: F1 shows the whole map, and F1 again hides it
        bool revealMap = false;

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
                     event.key.key == SDLK_F1)
            {
                if (!event.key.repeat)
                {
                    revealMap = !revealMap;
                    redraw = true;
                }
            }
            else if (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_SPACE)
            {
                // A new dungeon, from a new seed
                seed = entropy();
                rng = Rng(seed);
                rooms = RoomGen::generate(map, rng);
                monsters = placeMonsters(rooms, rng);
                player.pos = RoomGen::centerOf(rooms.front());
                Fov::compute(map, player.pos, SIGHT_RADIUS);
                turn = 0;
                log.markRead();
                log.add("You enter a new dungeon.");
                redraw = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                // A step takes a turn. Bumping into a wall or a monster
                // doesn't
                Point step;
                if (stepFor(event.key, step))
                {
                    log.markRead();
                    Point to = { player.pos.x + step.x, player.pos.y + step.y };
                    Monster* monster = monsterAt(monsters, to);
                    if (monster != nullptr)
                    {
                        std::string name = monster->def->name;
                        log.add("The " + name + " is in the way.");
                    }
                    else if (map.at(to.x, to.y) == Terrain::Floor)
                    {
                        // The player's step, then the monsters' turn
                        player.pos = to;
                        Fov::compute(map, player.pos, SIGHT_RADIUS);
                        moveMonsters(map, monsters, player, rng);
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
                drawFrame(renderer, glyphs, map, monsters, player, log,
                          turn, seed, revealMap);
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
