#include "Game.h"
#include <algorithm>  // std::sort, for the monsters to aim at
#include <cstdlib>    // std::abs, for the line of fire
#include <iterator>   // std::size, for the kinds of trap
#include <random>     // std::random_device, for a random seed
#include <sstream>    // std::istringstream, to break text into words
#include <utility>    // std::swap, to shuffle the doors
#include "CaveGen.h"
#include "Files.h"
#include "Fov.h"
#include "RoomGen.h"

namespace
{
    // How far the player can see, in cells
    constexpr int SIGHT_RADIUS = 9;

    // How many monsters a floor starts with: MONSTERS_BASE, and
    // MONSTERS_PER_FLOOR more for each floor down. One in DEEPER_ONE_IN is
    // of a kind from three floors deeper (all *tune*)
    constexpr int MONSTERS_BASE = 16;
    constexpr int MONSTERS_PER_FLOOR = 1;
    constexpr int DEEPER_ONE_IN = 10;

    // How many items a floor starts with: ITEMS_BASE, and one more for
    // every two floors down
    constexpr int ITEMS_BASE = 6;

    // The floors that are caves. The rest are rooms
    constexpr int CAVE_FLOORS[] = { 4, 7 };

    // A cave holds CAVE_MONSTERS_PERCENT of the monsters a floor of rooms
    // would: in the open, every one of them can see the player from afar
    // (*tune*)
    constexpr int CAVE_MONSTERS_PERCENT = 75;

    // One floor in VAULT_ONE_IN has a treasure vault (*tune*)
    constexpr int VAULT_ONE_IN = 3;

    // Each floor has GOLD_PILES_MIN to GOLD_PILES_MAX piles of gold, of
    // GOLD_MIN to GOLD_MAX pieces for each floor down (*tune*)
    constexpr int GOLD_PILES_MIN = 3;
    constexpr int GOLD_PILES_MAX = 6;
    constexpr int GOLD_MIN = 5;
    constexpr int GOLD_MAX = 20;

    // The player's health at the start of a run
    constexpr int PLAYER_HP = 30;

    // The player heals 1 health every HEAL_EVERY turns, while hurt
    constexpr int HEAL_EVERY = 10;

    // A monster that regenerates heals REGENERATION health a turn (*tune*)
    constexpr int REGENERATION = 1;

    // How much health a potion of healing gives back (*tune*)
    constexpr int HEAL_AMOUNT = 15;

    // How long the statuses from potions last, in turns (*tune*)
    constexpr int POISON_TURNS = 6;
    constexpr int REGENERATION_TURNS = 12;
    constexpr int CONFUSION_TURNS = 8;
    constexpr int FEAR_TURNS = 10;

    // How tall the health bar in the side panel is, in pixels
    constexpr float HEALTH_BAR_H = 8.0f;

    // The look panel, at the bottom of the side panel, has room for this
    // many lines of text
    constexpr int LOOK_PANEL_LINES = 9;

    // The title screen lists this many of the reasons a save couldn't be
    // loaded, at most
    constexpr int MOST_LOAD_ERRORS = 8;

    // The distance overlay's colors: warm near the player, cooling to blue
    // by FAR_DISTANCE steps away
    constexpr SDL_Color NEAR_BACK = { 95, 70, 20, 255 };
    constexpr SDL_Color FAR_BACK = { 20, 28, 80, 255 };
    constexpr SDL_Color NEAR_TEXT = { 255, 225, 130, 255 };
    constexpr SDL_Color FAR_TEXT = { 130, 160, 255, 255 };
    constexpr float FAR_DISTANCE = 40.0f;

    // One monster in AWAKE_ONE_IN starts the run awake. An asleep monster
    // that can see the player wakes with a chance of 1 in WAKE_ONE_IN each
    // turn (both *tune*)
    constexpr int AWAKE_ONE_IN = 3;
    constexpr int WAKE_ONE_IN = 20;

    // A hunter that finds the player gone searches nearby for this many
    // turns, staying within SEARCH_RADIUS cells, before it gives up
    constexpr int SEARCH_TURNS = 5;
    constexpr int SEARCH_RADIUS = 3;

    // How loud things are, in cells (*tune*): a noise reaches as far as
    // its loudness, by the ways a creature could walk. A monster asleep
    // wakes if at least WAKE_LOUDNESS is left of a noise when it gets there
    constexpr int FIGHT_LOUDNESS = 8;
    constexpr int SHRIEK_LOUDNESS = 25;
    constexpr int FOOTSTEP_LOUDNESS = 6;   // walking in noisy armor
    constexpr int WAKE_LOUDNESS = 3;

    // Doors make noises too, opening and closing. A closed door also
    // muffles a noise: passing one counts as DOOR_MUFFLE cells, three more
    // than a step (all *tune*)
    constexpr int DOOR_OPEN_LOUDNESS = 5;
    constexpr int DOOR_CLOSE_LOUDNESS = 3;
    constexpr int DOOR_MUFFLE = 4;

    // Each floor of rooms has SECRET_DOORS_MIN to SECRET_DOORS_MAX secret
    // doors. Searching finds each one next to the player with a chance of
    // 1 in FIND_ONE_IN (all *tune*)
    constexpr int SECRET_DOORS_MIN = 1;
    constexpr int SECRET_DOORS_MAX = 2;
    constexpr int FIND_ONE_IN = 3;

    // Each floor hides TRAPS_BASE traps, and one more for every two floors
    // down. What they do: a dart's damage, an alarm's loudness, and the
    // damage of a fall through a trapdoor (all *tune*)
    constexpr int TRAPS_BASE = 2;
    constexpr int DART_MIN = 2;
    constexpr int DART_MAX = 6;
    constexpr int ALARM_LOUDNESS = 30;
    constexpr int FALL_MIN = 2;
    constexpr int FALL_MAX = 5;

    // A scroll of lightning's bolt: how far it goes, and its damage, which
    // armor doesn't stop (*tune*)
    constexpr int LIGHTNING_RANGE = 8;
    constexpr int LIGHTNING_MIN = 8;
    constexpr int LIGHTNING_MAX = 14;

    // How far the player throws. A missile hits with a chance, in a
    // hundred, of HIT_CHANCE_NEAR within two cells, and HIT_CHANCE_STEP less
    // for each cell further, down to HIT_CHANCE_MIN. An arrow breaks one
    // time in BREAK_ONE_IN (all *tune*)
    constexpr int THROW_RANGE = 6;
    constexpr int HIT_CHANCE_NEAR = 95;
    constexpr int HIT_CHANCE_STEP = 5;
    constexpr int HIT_CHANCE_MIN = 50;
    constexpr int BREAK_ONE_IN = 4;

    // A scroll of fireball: how far it flies, how far its burst reaches,
    // its damage, which armor doesn't stop, and how loud it is (*tune*)
    constexpr int FIREBALL_RANGE = 8;
    constexpr int FIREBALL_RADIUS = 2;
    constexpr int FIREBALL_MIN = 6;
    constexpr int FIREBALL_MAX = 10;
    constexpr int FIREBALL_LOUDNESS = 15;

    // A monster that keeps its distance stays KEEP_MIN to KEEP_MAX steps
    // from the player. A breath of fire bursts on every cell within
    // BREATH_RADIUS steps of its target: three cells across (*tune*)
    constexpr int KEEP_MIN = 3;
    constexpr int KEEP_MAX = 5;
    constexpr int BREATH_RADIUS = 1;

    // A shrieker shrieks every SHRIEK_EVERY turns while it sees the player
    constexpr int SHRIEK_EVERY = 3;

    // The monster minds overlay's colors, one for each state
    constexpr SDL_Color ASLEEP_COLOR = { 90, 140, 255, 255 };
    constexpr SDL_Color WANDERING_COLOR = { 170, 170, 170, 255 };
    constexpr SDL_Color HUNTING_COLOR = { 255, 80, 70, 255 };
    constexpr SDL_Color FLEEING_COLOR = { 255, 220, 60, 255 };

    std::string stateName(MonsterState state)
    {
        switch (state)
        {
        case MonsterState::Asleep:
            return "asleep";
        case MonsterState::Wandering:
            return "wandering";
        case MonsterState::Hunting:
            return "hunting";
        case MonsterState::Fleeing:
            return "fleeing";
        }
        return "";
    }

    SDL_Color stateColor(MonsterState state)
    {
        switch (state)
        {
        case MonsterState::Asleep:
            return ASLEEP_COLOR;
        case MonsterState::Wandering:
            return WANDERING_COLOR;
        case MonsterState::Hunting:
            return HUNTING_COLOR;
        case MonsterState::Fleeing:
            return FLEEING_COLOR;
        }
        return WANDERING_COLOR;
    }

    // The tiles' picture and index, in the game's folder
    const char* const TILES_PATH = "assets/tiles.png";
    const char* const TILES_INDEX_PATH = "assets/tiles.txt";

    // The keys that move the player, and the step each one takes. The
    // arrow keys and the numeric keypad are matched by where they are on
    // the keyboard (their scancodes), so Num Lock makes no difference. The
    // letters are matched by what they type (their keycodes), as in many
    // roguelikes: h, j, k and l go left, down, up and right, and y, u, b and
    // n go diagonally. The period and the keypad's 5 wait a turn
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

        // A step of nowhere: the player stays put, and the turn passes
        { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_5, SDLK_PERIOD, { 0, 0 } },
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

    // The character a key types with the modifiers held, such as > for
    // Shift and the period on a US keyboard. A key event's keycode is
    // the key's own character, the period, whether Shift is held or not
    SDL_Keycode typedKey(const SDL_KeyboardEvent& key)
    {
        return SDL_GetKeyFromScancode(key.scancode, key.mod, false);
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

    // The seed a run plays: the one asked for, or a random one for 0
    unsigned int chooseSeed(unsigned int seed)
    {
        if (seed != 0)
        {
            return seed;
        }
        std::random_device entropy;
        return entropy();
    }

    // The damage one blow does: a roll from minDamage to maxDamage, plus
    // the attacker's bonus, less the defender's defense, but never less
    // than 1, so every hit hurts
    int rollDamage(Rng& rng, int minDamage, int maxDamage, int bonus,
                   int defense)
    {
        int damage = rng.roll(minDamage, maxDamage) + bonus - defense;
        return SDL_max(1, damage);
    }

    // The square of the distance between two cells. Squares compare the
    // same way as the distances do, without any square roots
    int distanceSquared(Point a, Point b)
    {
        int dx = a.x - b.x;
        int dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    // The cells of a line from one cell toward another, after the first,
    // for length cells: past the second, if it's nearer. Bresenham's way:
    // a step goes along the line's longer axis, and along the shorter one
    // too whenever the error, how far the cells have strayed from the true
    // line, would grow past half a cell. Each step changes the error by a
    // whole number, so the sums stay exact
    std::vector<Point> lineFrom(Point from, Point to, int length)
    {
        std::vector<Point> cells;
        int dx = std::abs(to.x - from.x);
        int dy = -std::abs(to.y - from.y);
        if (dx == 0 && dy == 0)
        {
            return cells;   // no way to go
        }
        int stepX = from.x < to.x ? 1 : -1;
        int stepY = from.y < to.y ? 1 : -1;
        int error = dx + dy;
        Point cell = from;
        while (static_cast<int>(cells.size()) < length)
        {
            int twice = 2 * error;
            if (twice >= dy)
            {
                error += dy;
                cell.x += stepX;
            }
            if (twice <= dx)
            {
                error += dx;
                cell.y += stepY;
            }
            cells.push_back(cell);
        }
        return cells;
    }

    // How many steps apart two cells are, diagonal steps counting as one
    int stepsBetween(Point a, Point b)
    {
        return SDL_max(std::abs(a.x - b.x), std::abs(a.y - b.y));
    }

    // A missile's chance to hit, in a hundred, cells away: HIT_CHANCE_NEAR
    // within two cells, and HIT_CHANCE_STEP less for each cell further,
    // down to HIT_CHANCE_MIN
    int hitChance(int cells)
    {
        return SDL_max(HIT_CHANCE_MIN, HIT_CHANCE_NEAR -
                       HIT_CHANCE_STEP * SDL_max(0, cells - 2));
    }

    // True if two cells touch, side by side or corner to corner
    bool isNextTo(Point a, Point b)
    {
        return distanceSquared(a, b) <= 2;
    }

    // True if a cell is inside an area, if there is one
    bool inside(Point cell, const std::optional<SDL_Rect>& area)
    {
        return area && cell.x >= area->x && cell.x < area->x + area->w &&
               cell.y >= area->y && cell.y < area->y + area->h;
    }

    // Where the player starts in an area: the walkable cell nearest its
    // middle, but not in the area to avoid, a vault's. In a room, that's
    // the middle itself
    Point startIn(const Map& map, const SDL_Rect& area,
                  const std::optional<SDL_Rect>& avoid = std::nullopt)
    {
        Point middle = RoomGen::centerOf(area);
        Point best = middle;
        int bestDistance = -1;   // none found yet
        for (int y = area.y; y < area.y + area.h; ++y)
        {
            for (int x = area.x; x < area.x + area.w; ++x)
            {
                int distance = distanceSquared({ x, y }, middle);
                bool nearer = bestDistance < 0 || distance < bestDistance;
                bool allowed = map.isWalkable(x, y) &&
                               !inside({ x, y }, avoid);
                if (allowed && nearer)
                {
                    best = { x, y };
                    bestDistance = distance;
                }
            }
        }
        return best;
    }

    bool isAmuletFloor(int depth)
    {
        for (int floor : AMULET_FLOORS)
        {
            if (floor == depth)
            {
                return true;
            }
        }
        return false;
    }

    bool isCaveFloor(int depth)
    {
        for (int floor : CAVE_FLOORS)
        {
            if (floor == depth)
            {
                return true;
            }
        }
        return false;
    }

    // The doors into a room: the door cells along its four sides, just
    // outside it, where RoomGen put them
    std::vector<Point> doorsInto(const Map& map, const SDL_Rect& room)
    {
        std::vector<Point> doors;
        for (int x = room.x; x < room.x + room.w; ++x)
        {
            for (int y : { room.y - 1, room.y + room.h })
            {
                if (map.isDoor(x, y))
                {
                    doors.push_back({ x, y });
                }
            }
        }
        for (int y = room.y; y < room.y + room.h; ++y)
        {
            for (int x : { room.x - 1, room.x + room.w })
            {
                if (map.isDoor(x, y))
                {
                    doors.push_back({ x, y });
                }
            }
        }
        return doors;
    }

    // The cell of ground farthest from start by walking, where the stairs
    // down go. A doorway isn't ground for this, nor a cell in the area to
    // avoid, a vault's
    Point farthestFrom(const Map& map, Point start,
                       const std::optional<SDL_Rect>& avoid = std::nullopt)
    {
        DijkstraMap distances;
        distances.compute(map, { start });
        Point farthest = start;
        int farthestDistance = 0;
        for (int y = 0; y < MAP_H; ++y)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                int distance = distances.at({ x, y });
                bool ground = map.isWalkable(x, y) &&
                              !map.isDoor(x, y) && !inside({ x, y }, avoid);
                if (ground && distance != DijkstraMap::UNREACHED &&
                    distance > farthestDistance)
                {
                    farthest = { x, y };
                    farthestDistance = distance;
                }
            }
        }
        return farthest;
    }

    // The name with "a" or "an" before it: "a jackal", but "an ogre"
    std::string withArticle(const std::string& name)
    {
        const std::string vowels = "aeiou";
        if (!name.empty() && vowels.find(name[0]) != std::string::npos)
        {
            return "an " + name;
        }
        return "a " + name;
    }

    // A color part of the way from a to b: t = 0 gives a, and t = 1 gives b
    SDL_Color blend(SDL_Color a, SDL_Color b, float t)
    {
        auto mix = [t](Uint8 from, Uint8 to)
        {
            return static_cast<Uint8>(from + (to - from) * t);
        };
        return { mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255 };
    }

    // The same text, with its first letter a capital, to start a sentence
    std::string capitalized(std::string text)
    {
        if (!text.empty())
        {
            text[0] = static_cast<char>(SDL_toupper(text[0]));
        }
        return text;
    }

    // True for a creature with less than a third of its health left. A
    // monster this badly wounded runs from the player
    bool isBadlyWounded(const Entity& entity)
    {
        return entity.hp * 3 < entity.maxHp;
    }

    // How hurt a creature looks, in words, from its share of health left
    std::string woundsOf(const Entity& entity)
    {
        if (entity.hp >= entity.maxHp)
        {
            return "unhurt";
        }
        if (entity.hp * 3 >= entity.maxHp * 2)
        {
            return "lightly wounded";
        }
        if (!isBadlyWounded(entity))
        {
            return "wounded";
        }
        return "badly wounded";
    }

    // Text broken between words into lines of at most width characters
    std::vector<std::string> wrapped(const std::string& text, int width)
    {
        std::vector<std::string> lines;
        std::istringstream words(text);
        std::string word;
        std::string line;
        while (words >> word)
        {
            // The line so far, with this word on the end
            std::string longer = line.empty() ? word : line + " " + word;
            if (!line.empty() && static_cast<int>(longer.size()) > width)
            {
                // Too long: the line is done, and the word starts the next
                lines.push_back(line);
                longer = word;
            }
            line = longer;
        }
        if (!line.empty())
        {
            lines.push_back(line);
        }
        return lines;
    }
}

// A new run: the first floor, the looks of this run's potions and
// scrolls, and the player, with a short sword in hand. If a run was saved,
// the title screen offers to continue it instead
Game::Game(SDL_Renderer* renderer, GlyphCache& glyphs,
           const GameData& data, unsigned int seed, bool useTiles)
    : renderer_(renderer), glyphs_(glyphs), data_(data),
      glyphView_(renderer, glyphs),
      tileView_(renderer, resourcePath(TILES_PATH),
                resourcePath(TILES_INDEX_PATH)),
      view_(&glyphView_), seed_(chooseSeed(seed)), rng_(seed_)
{
    newLevel(1);
    identification_.randomize(data_.items(), rng_);
    player_.weapon = Item{ data_.findItem("short_sword") };
    player_.hp = PLAYER_HP;
    player_.maxHp = PLAYER_HP;

    log_.add("Welcome to the dungeon.");
    log_.add("Move with the arrow keys, the numpad, or h j k l y u b n.");

    if (useTiles && tileView_.isLoaded())
    {
        view_ = &tileView_;
    }

    // A saved run waiting means the title screen comes first, to continue
    // it or not
    if (saveExists())
    {
        state_ = GameState::Title;
    }
}

bool Game::usesTiles() const
{
    return view_ == &tileView_;
}

bool Game::run()
{
    // The window may already be smaller than the picture
    glyphs_.setScale(presentationScale(renderer_));

    while (running_)
    {
        // Sleep until something happens. A turn-based game has nothing to
        // do while the player is thinking
        SDL_Event event;
        if (!SDL_WaitEvent(&event))
        {
            SDL_Log("SDL_WaitEvent failed: %s", SDL_GetError());
            break;
        }
        handleEvent(event);

        if (redraw_)
        {
            // The view's camera keeps the cursor in sight while looking
            // or aiming, and the player the rest of the time
            Point focus = player_.pos;
            if (state_ == GameState::Look ||
                state_ == GameState::Targeting)
            {
                focus = lookCursor_;
            }
            if (vaultShown_)
            {
                // F7's vault is in the middle of the map
                focus = { MAP_W / 2, MAP_H / 2 };
            }
            view_->camera().follow(focus);
            draw();
            redraw_ = false;
        }
    }
    return playAgain_;
}

// Makes the floor at a depth, in place of the one before: rooms, or a
// cave, an Amulet's vault, or one time in VAULT_ONE_IN a treasure vault,
// the player in the middle of the first area, its monsters, items and
// gold, what's in the vault, its stairs down, unless it's the last floor,
// and its secrets: secret doors, and traps. Both generators give areas to
// put things in: rooms, or the squares of a grid over the cave, which can
// have walls in them
void Game::newLevel(int depth)
{
    // An Amulet's floor has its vault, and if no room has room for it,
    // the floor is made again until one does. Other floors have a treasure
    // vault one time in VAULT_ONE_IN, if one fits
    bool amuletFloor = isAmuletFloor(depth);
    std::vector<SDL_Rect> areas;
    std::optional<VaultStamp::Contents> inVaultToFill;
    do
    {
        level_ = Level();
        level_.depth = depth;
        if (isCaveFloor(depth))
        {
            areas = CaveGen::generate(level_.map, rng_);
        }
        else
        {
            areas = RoomGen::generate(level_.map, rng_);
        }
        if (amuletFloor)
        {
            inVaultToFill = buildVault(areas, true);
        }
        else if (rng_.roll(1, VAULT_ONE_IN) == 1)
        {
            inVaultToFill = buildVault(areas, false);
        }
    } while (amuletFloor && !inVaultToFill);
    player_.pos = startIn(level_.map, areas.front(), level_.vault);
    placeMonsters(areas);
    placeItems(areas);
    placeGold(areas);
    if (inVaultToFill)
    {
        fillVault(*inVaultToFill);
    }
    if (depth < LAST_FLOOR)
    {
        placeStairs();
    }
    if (!isCaveFloor(depth))
    {
        placeSecretDoors(areas);
    }
    placeTraps(areas);
    updateSight();
    toPlayer_.compute(level_.map, { player_.pos });
}

// Hides SECRET_DOORS_MIN to SECRET_DOORS_MAX of the doors as secret doors.
// A door can be hidden only if its room keeps a door that isn't secret,
// and can still be reached from the start without this one, and never
// if the stairs, or the floor's Amulet, could no longer be reached: with
// no way back up, an Amulet missed is a run lost. The rooms beyond it
// can be hidden. The doors are tried in a random order, shuffled as the
// potions' colors are, and each that breaks a rule is put back
void Game::placeSecretDoors(const std::vector<SDL_Rect>& rooms)
{
    // Every door, with the room it leads into
    struct Doorway
    {
        Point door;
        const SDL_Rect* room;
    };
    std::vector<Doorway> doorways;
    for (const SDL_Rect& room : rooms)
    {
        for (Point door : doorsInto(level_.map, room))
        {
            doorways.push_back({ door, &room });
        }
    }
    for (int i = static_cast<int>(doorways.size()) - 1; i > 0; --i)
    {
        std::swap(doorways[i], doorways[rng_.roll(0, i)]);
    }

    int wanted = rng_.roll(SECRET_DOORS_MIN, SECRET_DOORS_MAX);
    int hidden = 0;
    for (const Doorway& doorway : doorways)
    {
        if (hidden == wanted)
        {
            return;
        }
        if (doorsInto(level_.map, *doorway.room).size() < 2)
        {
            continue;   // it's the room's only door that isn't secret
        }
        Point door = doorway.door;
        Terrain before = level_.map.at(door.x, door.y);
        level_.map.set(door.x, door.y, Terrain::SecretDoor);

        // Hidden, the door mustn't cut off its own room, tested from the
        // cell just inside it, nor the stairs
        const SDL_Rect& room = *doorway.room;
        Point inside = { SDL_clamp(door.x, room.x, room.x + room.w - 1),
                         SDL_clamp(door.y, room.y, room.y + room.h - 1) };
        DijkstraMap fromStart;
        fromStart.compute(level_.map, { player_.pos });
        bool cutOff = fromStart.at(inside) == DijkstraMap::UNREACHED ||
                      (level_.depth < LAST_FLOOR &&
                       fromStart.at(level_.stairs) == DijkstraMap::UNREACHED);
        for (const FloorItem& lying : level_.items)
        {
            bool amulet = lying.item.def->itemClass == ItemClass::Amulet;
            if (amulet && fromStart.at(lying.pos) == DijkstraMap::UNREACHED)
            {
                cutOff = true;
            }
        }
        if (cutOff)
        {
            level_.map.set(door.x, door.y, before);
            continue;
        }
        ++hidden;
    }
}

// The stairs down go on the cell farthest from the player by walking, so
// every floor has to be crossed to find the way down
void Game::placeStairs()
{
    Point farthest = farthestFrom(level_.map, player_.pos, level_.vault);
    level_.stairs = farthest;
    level_.map.set(farthest.x, farthest.y, Terrain::StairsDown);
}

// > on the stairs goes down to a new floor. The player keeps everything
// they have, and the floor they leave is gone. The run is saved on
// arrival. Leaving the floor's Amulet behind asks first: there's no way
// back up for it
void Game::descend(bool confirmed)
{
    log_.markRead();
    if (level_.map.at(player_.pos.x, player_.pos.y) != Terrain::StairsDown)
    {
        log_.add("There are no stairs down here.");
        return;
    }
    if (!confirmed && amuletStillHere())
    {
        confirmPurpose_ = ConfirmPurpose::Descend;
        state_ = GameState::Confirming;
        log_.add("Leave this floor's Amulet behind for good? (y or n)");
        return;
    }
    arriveOnFloor("You go down the stairs");
}

// True if the floor's Amulet still lies somewhere on it
bool Game::amuletStillHere() const
{
    for (const FloorItem& lying : level_.items)
    {
        if (lying.item.def->itemClass == ItemClass::Amulet)
        {
            return true;
        }
    }
    return false;
}

// A kind of trap, each as likely as the others, but for a trapdoor on an
// Amulet's floor, or the last. A fall would leave an Amulet behind, and
// with no way back up, the run could never be won; and the last floor
// has nowhere to fall to. The trapdoor is last in TRAP_DEFS, so the
// roll leaves it out there
TrapKind Game::chooseTrapKind()
{
    int lastKind = static_cast<int>(std::size(TRAP_DEFS)) - 1;
    if (level_.depth == LAST_FLOOR || isAmuletFloor(level_.depth))
    {
        --lastKind;
    }
    return TRAP_DEFS[rng_.roll(0, lastKind)].kind;
}

// Builds an Amulet's vault, or a treasure vault, if one of those for this
// floor has room: they take their turns in a random order. On a floor of
// rooms, it goes in a room; in a cave, the cave is flood filled again
// afterward, in case the vault's walls cut part of it off, and its areas
// are worked out anew. Returns the cells of what's in the vault, for
// fillVault, once the floor has its other monsters and items
std::optional<VaultStamp::Contents> Game::buildVault(
    std::vector<SDL_Rect>& areas, bool amulet)
{
    std::vector<const VaultDef*> vaults;
    for (const VaultDef& vault : data_.vaults())
    {
        bool here = vault.minFloor <= level_.depth &&
                    level_.depth <= vault.maxFloor;
        if (here && vault.amulet == amulet)
        {
            vaults.push_back(&vault);
        }
    }
    for (int i = static_cast<int>(vaults.size()) - 1; i > 0; --i)
    {
        std::swap(vaults[i], vaults[rng_.roll(0, i)]);
    }

    bool cave = isCaveFloor(level_.depth);
    for (const VaultDef* vault : vaults)
    {
        std::optional<Point> corner = cave ? caveForVault(*vault)
                                           : roomForVault(*vault, areas);
        if (!corner)
        {
            continue;
        }
        VaultStamp::Contents contents =
            VaultStamp::stamp(level_.map, *vault, *corner);
        level_.vault = SDL_Rect{ corner->x, corner->y, vault->width(),
                                 vault->height() };
        if (cave)
        {
            CaveGen::keepBiggestCave(level_.map);
            areas = CaveGen::areasOf(level_.map);
        }
        return contents;
    }
    return std::nullopt;
}

// Room for a vault on a floor of rooms: a room, but not the first, which
// is the player's, big enough to have floor all around the vault. The
// corridors still meet the room at its doorways, and the floor around the
// vault leads them to its door. Nothing, if no room is big enough
std::optional<Point> Game::roomForVault(const VaultDef& vault,
                                        const std::vector<SDL_Rect>& rooms)
{
    std::vector<const SDL_Rect*> roomy;
    for (size_t i = 1; i < rooms.size(); ++i)
    {
        bool fits = rooms[i].w >= vault.width() + 2 &&
                    rooms[i].h >= vault.height() + 2;
        if (fits)
        {
            roomy.push_back(&rooms[i]);
        }
    }
    if (roomy.empty())
    {
        return std::nullopt;
    }
    const SDL_Rect& room =
        *roomy[rng_.roll(0, static_cast<int>(roomy.size()) - 1)];
    return Point{ room.x + 1 + rng_.roll(0, room.w - 2 - vault.width()),
                  room.y + 1 + rng_.roll(0, room.h - 2 - vault.height()) };
}

// A place for a vault in a cave: inside the map's edge, with the cell
// outside its way in on the cave's floor. The cave must still hold
// together once the vault is built: the flood fill afterward mustn't take
// the vault away, nor leave the cave too small. So each place is tried out
// on a copy of the map. Nothing, if a few tries find none
std::optional<Point> Game::caveForVault(const VaultDef& vault)
{
    Point entrance = VaultStamp::entrance(vault);
    for (int tries = 0; tries < 50; ++tries)
    {
        Point corner = { rng_.roll(1, MAP_W - 1 - vault.width()),
                         rng_.roll(1, MAP_H - 1 - vault.height()) };
        Point outside = { corner.x + entrance.x, corner.y + entrance.y };
        if (level_.map.at(outside.x, outside.y) != Terrain::CaveFloor)
        {
            continue;
        }
        Map trial = level_.map;
        VaultStamp::stamp(trial, vault, corner);
        int cells = CaveGen::keepBiggestCave(trial);
        bool whole = trial.isWalkable(outside.x, outside.y) &&
                     cells >= CaveGen::MIN_CAVE_CELLS &&
                     CaveGen::areasOf(trial).size() >= 2;
        if (whole)
        {
            return corner;
        }
    }
    return std::nullopt;
}

// Fills a vault: a hidden trap on each ^, an item on each $, a monster
// from the floor's band on each m, its guardian on each G, and the Amulet
// on the A. The guardians are a pack of their own, so they wake together,
// and all start asleep or all awake
void Game::fillVault(const VaultStamp::Contents& contents)
{
    if (contents.amulet)
    {
        FloorItem amulet;
        amulet.pos = *contents.amulet;
        amulet.item.def = data_.findItem("amulet");
        level_.items.push_back(amulet);
    }
    for (Point cell : contents.traps)
    {
        level_.traps.push_back({ cell, chooseTrapKind(), false });
    }
    for (Point cell : contents.treasure)
    {
        FloorItem treasure;
        treasure.pos = cell;
        treasure.item.def = chooseItemKind();
        const ItemDef& kind = *treasure.item.def;
        if (kind.itemClass == ItemClass::Missile)
        {
            treasure.item.count = rng_.roll(kind.minFound, kind.maxFound);
        }
        level_.items.push_back(treasure);
    }
    for (Point cell : contents.monsters)
    {
        const MonsterDef* kind = chooseMonsterKind();
        bool awake = kind->has(Trait::NEVER_SLEEPS) ||
                     rng_.roll(1, AWAKE_ONE_IN) == 1;
        addMonster(*kind, cell, 0, awake);
    }
    if (contents.guardians.empty())
    {
        return;
    }
    int packId = 1;
    for (const Monster& monster : level_.monsters)
    {
        packId = SDL_max(packId, monster.packId + 1);
    }
    const MonsterDef& guardian = *contents.guardian;
    bool awake = guardian.has(Trait::NEVER_SLEEPS) ||
                 rng_.roll(1, AWAKE_ONE_IN) == 1;
    for (Point cell : contents.guardians)
    {
        addMonster(guardian, cell, packId, awake);
    }
}

// True if a cell is inside the floor's vault
bool Game::inVault(Point cell) const
{
    return inside(cell, level_.vault);
}

// The player arrives on the next floor down, by the stairs or by a fall.
// It takes no turn there; the turn count still goes up
void Game::arriveOnFloor(const std::string& how)
{
    newLevel(level_.depth + 1);
    ++turn_;
    log_.add(how + " to floor " + std::to_string(level_.depth) + ".");
    if (level_.depth == LAST_FLOOR)
    {
        log_.add("This is the bottom of the dungeon.");
    }

    // Each new floor is a checkpoint: if the game stops without saving,
    // as in a crash, the run goes on from here
    if (!writeSave())
    {
        log_.add("The run couldn't be saved: " +
                 std::string(SDL_GetError()));
    }
}

// Hides the floor's traps: TRAPS_BASE, and one more for every two floors
// down. Each goes on the ground in an area, but never in the first, where
// the player starts, nor on the stairs, an item, another trap, or in the
// vault, which has traps of its own
void Game::placeTraps(const std::vector<SDL_Rect>& areas)
{
    int lastArea = static_cast<int>(areas.size()) - 1;
    if (lastArea < 1)
    {
        return;
    }

    int wanted = TRAPS_BASE + level_.depth / 2;
    for (int i = 0; i < wanted; ++i)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            const SDL_Rect& area = areas[rng_.roll(1, lastArea)];
            Point pos = { area.x + rng_.roll(0, area.w - 1),
                          area.y + rng_.roll(0, area.h - 1) };
            bool free = level_.map.isWalkable(pos.x, pos.y) &&
                        level_.map.at(pos.x, pos.y) != Terrain::StairsDown &&
                        floorItemAt(pos) == nullptr &&
                        trapAt(pos) == nullptr && !inVault(pos);
            if (free)
            {
                Trap trap;
                trap.pos = pos;
                trap.kind = chooseTrapKind();
                level_.traps.push_back(trap);
                break;
            }
        }
    }
}

// Puts GOLD_PILES_MIN to GOLD_PILES_MAX piles of gold in the areas, the
// deeper the bigger, one to a cell, on the ground, never on the player or
// another item. Each pile has a few tries to find a cell, as items do
void Game::placeGold(const std::vector<SDL_Rect>& areas)
{
    int lastArea = static_cast<int>(areas.size()) - 1;
    int piles = rng_.roll(GOLD_PILES_MIN, GOLD_PILES_MAX);
    for (int i = 0; i < piles; ++i)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            const SDL_Rect& area = areas[rng_.roll(0, lastArea)];
            Point pos = { area.x + rng_.roll(0, area.w - 1),
                          area.y + rng_.roll(0, area.h - 1) };
            bool free = level_.map.isWalkable(pos.x, pos.y) &&
                        pos != player_.pos && floorItemAt(pos) == nullptr &&
                        !inVault(pos);
            if (free)
            {
                FloorItem pile;
                pile.pos = pos;
                pile.item.def = data_.findItem("gold");
                pile.item.count = rng_.roll(GOLD_MIN, GOLD_MAX) *
                                  level_.depth;
                level_.items.push_back(pile);
                break;
            }
        }
    }
}

// Puts the floor's monsters in its areas: MONSTERS_BASE, and
// MONSTERS_PER_FLOOR more for each floor down, or CAVE_MONSTERS_PERCENT
// of that in a cave, while there's room. A pack comes all together, in one
// area, asleep or awake together. The first area is the player's, so it's
// left empty
void Game::placeMonsters(const std::vector<SDL_Rect>& areas)
{
    int lastArea = static_cast<int>(areas.size()) - 1;
    if (lastArea < 1)
    {
        return;
    }

    int wanted = MONSTERS_BASE + MONSTERS_PER_FLOOR * level_.depth;
    if (isCaveFloor(level_.depth))
    {
        wanted = wanted * CAVE_MONSTERS_PERCENT / 100;
    }
    int packsSoFar = 0;
    for (int tries = 0; tries < 100; ++tries)
    {
        if (static_cast<int>(level_.monsters.size()) >= wanted)
        {
            return;
        }

        const MonsterDef* kind = chooseMonsterKind();
        const SDL_Rect& area = areas[rng_.roll(1, lastArea)];
        int packId = 0;
        if (kind->pack > 1)
        {
            ++packsSoFar;
            packId = packsSoFar;
        }

        // Most start asleep, and the rest wandering
        bool awake = kind->has(Trait::NEVER_SLEEPS) ||
                     rng_.roll(1, AWAKE_ONE_IN) == 1;
        for (int i = 0; i < kind->pack; ++i)
        {
            std::optional<Point> cell = freeCellIn(area);
            if (cell)
            {
                addMonster(*kind, *cell, packId, awake);
            }
        }
    }
}

// A kind of monster for this floor: one that's found on it, or, one time
// in DEEPER_ONE_IN, one that's found three floors further down, if any
// kind is found that deep
const MonsterDef* Game::chooseMonsterKind()
{
    // Every kind found on a floor. The lambda captures this, the Game, to
    // use its data_
    auto kindsOn = [this](int floor)
    {
        std::vector<const MonsterDef*> kinds;
        for (const MonsterDef& def : data_.monsters())
        {
            bool here = def.minFloor <= floor && floor <= def.maxFloor;
            if (here && !def.has(Trait::VAULT_ONLY))
            {
                kinds.push_back(&def);
            }
        }
        return kinds;
    };

    std::vector<const MonsterDef*> kinds;
    if (rng_.roll(1, DEEPER_ONE_IN) == 1)
    {
        kinds = kindsOn(level_.depth + 3);
    }
    if (kinds.empty())
    {
        kinds = kindsOn(level_.depth);
    }
    return kinds[rng_.roll(0, static_cast<int>(kinds.size()) - 1)];
}

// A cell of ground in the area, with no monster on it and not in the
// vault, chosen at random, or nothing if a few tries find none
std::optional<Point> Game::freeCellIn(const SDL_Rect& area)
{
    for (int tries = 0; tries < 20; ++tries)
    {
        Point cell = { area.x + rng_.roll(0, area.w - 1),
                       area.y + rng_.roll(0, area.h - 1) };
        bool free = level_.map.isWalkable(cell.x, cell.y) &&
                    monsterAt(cell) == nullptr && !inVault(cell);
        if (free)
        {
            return cell;
        }
    }
    return std::nullopt;
}

// A new monster of a kind, on a cell, with its kind's health and defense
void Game::addMonster(const MonsterDef& kind, Point cell, int packId,
                      bool awake)
{
    Monster monster;
    monster.def = &kind;
    monster.pos = cell;
    monster.hp = kind.hp;
    monster.maxHp = kind.hp;
    monster.defense = kind.defense;
    monster.packId = packId;
    monster.state = awake ? MonsterState::Wandering : MonsterState::Asleep;
    level_.monsters.push_back(monster);
}

// Puts the floor's items in its areas, any of them: ITEMS_BASE, and one
// more for every two floors down. One to a cell, on the ground, never on
// the player
void Game::placeItems(const std::vector<SDL_Rect>& areas)
{
    int lastArea = static_cast<int>(areas.size()) - 1;
    int wanted = ITEMS_BASE + level_.depth / 2;
    for (int i = 0; i < wanted; ++i)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            const SDL_Rect& area = areas[rng_.roll(0, lastArea)];
            Point pos = { area.x + rng_.roll(0, area.w - 1),
                          area.y + rng_.roll(0, area.h - 1) };
            bool free = level_.map.isWalkable(pos.x, pos.y) &&
                        pos != player_.pos && floorItemAt(pos) == nullptr &&
                        !inVault(pos);
            if (free)
            {
                FloorItem floorItem;
                floorItem.pos = pos;
                floorItem.item.def = chooseItemKind();
                const ItemDef& kind = *floorItem.item.def;
                if (kind.itemClass == ItemClass::Missile)
                {
                    floorItem.item.count = rng_.roll(kind.minFound,
                                                     kind.maxFound);
                }
                level_.items.push_back(floorItem);
                break;
            }
        }
    }
}

// A kind of item, chosen by frequency, so a kind with a frequency of 10
// turns up twice as often as one with 5. The roll lands somewhere in the
// total, and the kinds take their shares of it in turn
const ItemDef* Game::chooseItemKind()
{
    int total = 0;
    for (const ItemDef& def : data_.items())
    {
        total += def.frequency;
    }
    int roll = rng_.roll(1, total);
    for (const ItemDef& def : data_.items())
    {
        roll -= def.frequency;
        if (roll <= 0)
        {
            return &def;
        }
    }
    return &data_.items().front();
}

// The item lying on a cell, or nullptr if there's none
const FloorItem* Game::floorItemAt(Point pos) const
{
    for (const FloorItem& floorItem : level_.items)
    {
        if (floorItem.pos == pos)
        {
            return &floorItem;
        }
    }
    return nullptr;
}

// An item's name, as the log and the inventory give it. A kind the player
// knows goes by its name: "a potion of healing", "3 scrolls of magic
// mapping". One they don't goes by its look: "a crimson potion", "a
// scroll titled ZELMOR THIKA"
std::string Game::itemName(const Item& item) const
{
    const ItemDef& def = *item.def;
    if (def.itemClass == ItemClass::Weapon)
    {
        // A weapon needs no identifying: one look says what it is
        return withArticle(def.name);
    }
    if (def.itemClass == ItemClass::Armor)
    {
        // Nor does armor, which takes no "a": "chain mail"
        return def.name;
    }
    if (def.itemClass == ItemClass::Gold)
    {
        if (item.count == 1)
        {
            return "a gold piece";
        }
        return std::to_string(item.count) + " gold pieces";
    }
    if (def.itemClass == ItemClass::Amulet)
    {
        // There are only three, and each is named
        return "the " + def.name;
    }
    if (def.itemClass == ItemClass::Bow)
    {
        return withArticle(def.name);
    }
    if (def.itemClass == ItemClass::Missile)
    {
        if (item.count == 1)
        {
            return withArticle(def.name);
        }
        return std::to_string(item.count) + " " + def.name + "s";
    }

    bool potion = def.itemClass == ItemClass::Potion;
    std::string noun = potion ? "potion" : "scroll";
    if (item.count > 1)
    {
        noun += "s";
    }

    std::string name;
    std::string look = identification_.appearanceOf(def).name;
    if (identification_.isKnown(def))
    {
        name = noun + " of " + def.name;
    }
    else if (potion)
    {
        name = look + " " + noun;
    }
    else
    {
        name = noun + " titled " + look;
    }

    if (item.count > 1)
    {
        return std::to_string(item.count) + " " + name;
    }
    return withArticle(name);
}

// How an item looks: a potion is a ! in its color, a scroll a ?, a weapon
// a ), and armor a [. Weapons' and armor's tiles keep their own colors
ItemLook Game::itemLook(const Item& item) const
{
    const ItemDef& def = *item.def;
    if (def.itemClass == ItemClass::Weapon)
    {
        return { ')', Palette::WEAPON, def.tile, false };
    }
    if (def.itemClass == ItemClass::Armor)
    {
        return { '[', Palette::ARMOR, def.tile, false };
    }
    if (def.itemClass == ItemClass::Gold)
    {
        return { '$', Palette::GOLD, def.tile, false };
    }
    if (def.itemClass == ItemClass::Amulet)
    {
        // The tile is silver: gilded with a tint, it's the Amulet of SDL
        return { '"', Palette::GOLD, def.tile, true };
    }
    if (def.itemClass == ItemClass::Bow)
    {
        return { '}', Palette::WEAPON, def.tile, false };
    }
    if (def.itemClass == ItemClass::Missile)
    {
        return { '{', Palette::WEAPON, def.tile, false };
    }
    SDL_Color color = identification_.appearanceOf(def).color;
    char glyph = def.itemClass == ItemClass::Potion ? '!' : '?';
    return { glyph, color, def.tile, true };
}

void Game::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        // Closing the window in the middle of a run saves it, as the
        // menu's "Save and quit" does
        bool inRun = state_ != GameState::Title && state_ != GameState::Dead;
        if (inRun)
        {
            putBackUnfinishedScroll();
            saveAndQuit();
        }
        else
        {
            running_ = false;
        }
    }
    else if (event.type == SDL_EVENT_KEY_DOWN)
    {
        handleKey(event.key);
    }
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        trackMouse(event);
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        handleClick(event);
    }
    else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
    {
        mouse_.reset();
        redraw_ = true;
    }
    else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
        // The picture is shown at a new size, so the characters are drawn
        // again to match it
        glyphs_.setScale(presentationScale(renderer_));
        redraw_ = true;
    }
    else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
             event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        redraw_ = true;
    }
}

// Keeps track of where the mouse is. The event gives its place in the
// window's pixels, so SDL first converts it to the game's 1600 x 900
// picture, which is why the event is taken by value: a copy to change. The
// screen is drawn again only if the mouse is over a different cell
void Game::trackMouse(SDL_Event event)
{
    SDL_ConvertEventToRenderCoordinates(renderer_, &event);
    std::optional<Point> before = hoveredCell();

    float x = event.motion.x;
    float y = event.motion.y;
    if (x >= 0.0f && x < MAP_VIEW_W && y >= 0.0f && y < MAP_VIEW_H)
    {
        mouse_ = SDL_FPoint{ x, y };
    }
    else
    {
        mouse_.reset();
    }

    if (hoveredCell() != before)
    {
        redraw_ = true;
    }

    // While aiming, the cursor follows the mouse
    std::optional<Point> cell = hoveredCell();
    if (state_ == GameState::Targeting && cell)
    {
        lookCursor_ = *cell;
    }
}

// A click on the map, while aiming, fires at the cell clicked. The event
// gives its place in the window's pixels, as a mouse motion does
void Game::handleClick(SDL_Event event)
{
    SDL_ConvertEventToRenderCoordinates(renderer_, &event);
    bool onMap = event.button.x >= 0.0f && event.button.x < MAP_VIEW_W &&
                 event.button.y >= 0.0f && event.button.y < MAP_VIEW_H;
    if (state_ != GameState::Targeting || !onMap ||
        event.button.button != SDL_BUTTON_LEFT)
    {
        return;
    }
    lookCursor_ = view_->camera().cellAt(event.button.x, event.button.y);
    fireAtCursor();
    redraw_ = true;
}

// The map cell under the mouse, worked out through the view's camera. It's
// empty when the mouse is away from the map
std::optional<Point> Game::hoveredCell() const
{
    if (!mouse_)
    {
        return std::nullopt;
    }
    Point cell = view_->camera().cellAt(mouse_->x, mouse_->y);
    if (!level_.map.inBounds(cell.x, cell.y))
    {
        return std::nullopt;
    }
    return cell;
}

// The cell the look panel describes: the cursor's while looking or aiming,
// and otherwise the one under the mouse, if any
std::optional<Point> Game::lookTarget() const
{
    if (state_ == GameState::Look || state_ == GameState::Targeting)
    {
        return lookCursor_;
    }
    return hoveredCell();
}

void Game::handleKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_RETURN && (key.mod & SDL_KMOD_ALT) != 0)
    {
        // Alt+Enter. Holding the keys down doesn't flicker between the
        // window and the screen, because repeats are ignored
        if (!key.repeat)
        {
            toggleFullscreen(renderer_);
        }
        return;
    }
    if (key.key == SDLK_F2)
    {
        if (!key.repeat)
        {
            switchView();
        }
        return;
    }
    if (key.key == SDLK_F4)
    {
        // A debugging aid: F4 shows what every monster is doing
        if (!key.repeat)
        {
            showMinds_ = !showMinds_;
            redraw_ = true;
        }
        return;
    }
    if (key.key == SDLK_F3)
    {
        // A debugging aid: F3 shows how far every cell is from the player
        if (!key.repeat)
        {
            showDistances_ = !showDistances_;
            redraw_ = true;
        }
        return;
    }
    if (key.key == SDLK_F1)
    {
        // A debugging aid: F1 shows the whole map, and F1 again hides it
        if (!key.repeat)
        {
            revealMap_ = !revealMap_;
            redraw_ = true;
        }
        return;
    }
    if (key.key == SDLK_F6)
    {
        // A debugging aid: F6 shows a cave being made, a step at a time
        if (!key.repeat)
        {
            stepCave();
        }
        return;
    }
    if (key.key == SDLK_F7)
    {
        // A debugging aid: F7 shows the vaults, one at a time
        if (!key.repeat)
        {
            stepVault();
        }
        return;
    }

    // The keys above work in every state. What the others do depends on
    // the state the game is in
    switch (state_)
    {
    case GameState::Title:
        handleTitleKey(key);
        break;
    case GameState::Playing:
        handlePlayingKey(key);
        break;
    case GameState::Look:
        handleLookKey(key);
        break;
    case GameState::Inventory:
        handleInventoryKey(key);
        break;
    case GameState::Menu:
        handleMenuKey(key);
        break;
    case GameState::Closing:
        handleClosingKey(key);
        break;
    case GameState::Confirming:
        handleConfirmKey(key);
        break;
    case GameState::Targeting:
        handleTargetingKey(key);
        break;
    case GameState::Dead:
        handleDeadKey(key);
        break;
    }
}

// F6 shows a cave being made, a step at a time: the random fill, each
// smoothing pass in turn, the biggest cave kept, and then the game again.
// The cave has dice of its own, from the run's seed and how many caves
// have been shown, so looking changes nothing in the run, and the same
// seed shows the same caves
void Game::stepCave()
{
    if (!caveSteps_)
    {
        Rng dice(seed_ + cavesShown_);
        ++cavesShown_;
        caveSteps_ = CaveSteps();
        CaveGen::fillRandomly(caveSteps_->map, dice);
    }
    else if (caveSteps_->step < CaveGen::SMOOTHING_PASSES)
    {
        CaveGen::smooth(caveSteps_->map);
        ++caveSteps_->step;
    }
    else if (caveSteps_->step == CaveGen::SMOOTHING_PASSES)
    {
        // The last step keeps the biggest cave, and shows where a player
        // would start in it, and where its stairs would go
        CaveSteps& steps = *caveSteps_;
        steps.cells = CaveGen::keepBiggestCave(steps.map);
        std::vector<SDL_Rect> areas = CaveGen::areasOf(steps.map);
        if (!areas.empty())
        {
            steps.start = startIn(steps.map, areas.front());
            Point stairs = farthestFrom(steps.map, *steps.start);
            steps.map.set(stairs.x, stairs.y, Terrain::StairsDown);
        }
        ++steps.step;
    }
    else
    {
        caveSteps_.reset();
    }
    redraw_ = true;
}

// F7 shows each vault in vaults.txt in turn, and then the game again
void Game::stepVault()
{
    int count = static_cast<int>(data_.vaults().size());
    if (!vaultShown_ && count > 0)
    {
        vaultShown_ = 0;
    }
    else if (vaultShown_ && *vaultShown_ + 1 < count)
    {
        ++*vaultShown_;
    }
    else
    {
        vaultShown_.reset();
    }
    redraw_ = true;
}

// F2 switches between the glyph view and the tile view. If the tiles
// couldn't be loaded, the glyph view is all there is, and the log says why
void Game::switchView()
{
    if (view_ == &tileView_)
    {
        view_ = &glyphView_;
    }
    else if (tileView_.isLoaded())
    {
        view_ = &tileView_;
    }
    else
    {
        log_.add("The tiles couldn't be loaded, so there's no tile view.");
    }
    redraw_ = true;
}

// Esc opens the menu, > goes down the stairs, g picks up, i shows the
// inventory, d drops, x starts looking around, c closes a door, s
// searches, f fires the bow, t throws, and the moving keys move the
// player
void Game::handlePlayingKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_ESCAPE)
    {
        state_ = GameState::Menu;
        redraw_ = true;
        return;
    }

    // > goes down the stairs. It's Shift and the period on a US keyboard,
    // so it's checked before the moving keys, where the period alone waits
    if (typedKey(key) == SDLK_GREATER)
    {
        descend();
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_G)
    {
        pickUp();
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_I)
    {
        openInventory(InventoryAction::Use);
        return;
    }
    if (key.key == SDLK_D)
    {
        openInventory(InventoryAction::Drop);
        return;
    }
    if (key.key == SDLK_X)
    {
        state_ = GameState::Look;
        lookCursor_ = player_.pos;
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_C)
    {
        log_.markRead();
        log_.add("Close a door: which way? (Esc to stop)");
        state_ = GameState::Closing;
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_S)
    {
        search();
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_F)
    {
        fire();
        redraw_ = true;
        return;
    }
    if (key.key == SDLK_T)
    {
        openInventory(InventoryAction::Throw);
        return;
    }

    Point step;
    if (stepFor(key, step))
    {
        playerStep(step);
        redraw_ = true;
    }
}

// Looking around: the moving keys move the look cursor instead of the
// player, and it stays on the map. x or Esc stops looking. Looking takes no
// turns
void Game::handleLookKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_X || key.key == SDLK_ESCAPE)
    {
        state_ = GameState::Playing;
        redraw_ = true;
        return;
    }

    Point step;
    if (stepFor(key, step))
    {
        Point to = { lookCursor_.x + step.x, lookCursor_.y + step.y };
        if (level_.map.inBounds(to.x, to.y))
        {
            lookCursor_ = to;
            redraw_ = true;
        }
    }
}

// The inventory is showing. An item's letter chooses that item, for
// whatever the inventory was opened for, and Esc puts it away. (i can't
// close it: it's the letter of the ninth item.) SDL's keycodes for the
// letters run in order, from SDLK_A
void Game::handleInventoryKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_ESCAPE)
    {
        // Putting the pack away unchosen wastes a scroll of identify
        if (inventoryAction_ == InventoryAction::Identify)
        {
            log_.add("The scroll's magic is wasted.");
        }
        state_ = GameState::Playing;
        redraw_ = true;
        return;
    }

    int slot = static_cast<int>(key.key) - static_cast<int>(SDLK_A);
    int count = static_cast<int>(player_.inventory.items().size());
    if (slot < 0 || slot >= count)
    {
        return;
    }
    switch (inventoryAction_)
    {
    case InventoryAction::Use:
        useItem(slot);
        break;
    case InventoryAction::Drop:
        dropItem(slot);
        break;
    case InventoryAction::Identify:
        identifyItem(slot);
        break;
    case InventoryAction::Throw:
        chooseThrow(slot);
        break;
    }
    redraw_ = true;
}

// The death screen is up. R ends this run, and asks for another, and Esc
// quits. The other keys do nothing
void Game::handleDeadKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_R)
    {
        playAgain_ = true;
        running_ = false;
    }
    else if (key.key == SDLK_ESCAPE)
    {
        running_ = false;
    }
}

// The title screen, while a saved run is waiting. C continues it. N plays
// the new run this Game made instead, and the saved one is lost. Esc quits,
// and keeps the saved run for next time
void Game::handleTitleKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_C && loadErrors_.empty())
    {
        continueRun();
    }
    else if (key.key == SDLK_N)
    {
        removeSave();
        state_ = GameState::Playing;
    }
    else if (key.key == SDLK_ESCAPE)
    {
        running_ = false;
    }
    redraw_ = true;
}

// Loads the saved run in place of the new one, or leaves the reasons it
// couldn't for the title screen. The save stays where it is: it's the
// checkpoint the run goes back to if the game stops without saving, and
// death deletes it. A damaged save is set aside, so it's never offered
// again, but can still be looked at
void Game::continueRun()
{
    std::vector<std::string> errors;
    LoadResult result = loadSave(errors);
    if (result != LoadResult::Loaded)
    {
        loadErrors_ = errors;
        saveIsOtherVersion_ = result == LoadResult::OtherVersion;
        if (result == LoadResult::Damaged)
        {
            saveSetAside_ = setSaveAside();
        }
        return;
    }
    log_ = MessageLog();
    log_.add("Welcome back. You're on floor " +
             std::to_string(level_.depth) + ".");
    state_ = GameState::Playing;
}

// Closing a door: a moving key says which way, and Esc changes the
// player's mind. Either way, the game goes back to Playing
void Game::handleClosingKey(const SDL_KeyboardEvent& key)
{
    Point step;
    if (key.key == SDLK_ESCAPE)
    {
        state_ = GameState::Playing;
    }
    else if (stepFor(key, step) && step != Point{ 0, 0 })
    {
        state_ = GameState::Playing;
        closeDoor({ player_.pos.x + step.x, player_.pos.y + step.y });
    }
    redraw_ = true;
}

// Opens a closed door, which makes a noise, and lets sight through
void Game::openDoor(Point door)
{
    level_.map.set(door.x, door.y, Terrain::OpenDoor);
    makeNoise(door, DOOR_OPEN_LOUDNESS);
    updateSight();
}

// Closes the open door on a cell, if the doorway is empty: no one in it,
// and nothing lying in it. That takes a turn, and makes a little noise
void Game::closeDoor(Point door)
{
    if (level_.map.at(door.x, door.y) != Terrain::OpenDoor)
    {
        log_.add("There's no open door there.");
        return;
    }
    if (monsterAt(door) != nullptr || floorItemAt(door) != nullptr)
    {
        log_.add("Something is in the way.");
        return;
    }
    level_.map.set(door.x, door.y, Terrain::ClosedDoor);
    log_.add("You close the door.");
    makeNoise(door, DOOR_CLOSE_LOUDNESS);
    updateSight();
    finishTurn();
}

// Searches the eight cells around the player, which takes a turn. Each
// secret door or hidden trap there is found with a chance of 1 in
// FIND_ONE_IN. A secret door found becomes a closed door, and a trap found
// is known, and drawn
void Game::search()
{
    log_.markRead();
    bool found = false;
    for (Point step : DIRECTIONS)
    {
        Point cell = { player_.pos.x + step.x, player_.pos.y + step.y };
        bool secret = level_.map.at(cell.x, cell.y) == Terrain::SecretDoor;
        if (secret && rng_.roll(1, FIND_ONE_IN) == 1)
        {
            level_.map.set(cell.x, cell.y, Terrain::ClosedDoor);
            log_.add("You find a secret door!");
            found = true;
        }
        Trap* trap = trapAt(cell);
        bool hidden = trap != nullptr && !trap->known;
        if (hidden && rng_.roll(1, FIND_ONE_IN) == 1)
        {
            trap->known = true;
            log_.add("You find " +
                     withArticle(trapDefOf(trap->kind).name) + "!");
            found = true;
        }
    }
    if (!found)
    {
        log_.add("You search, and find nothing.");
    }
    finishTurn();
}

// Esc's menu: S saves the run and quits, and Esc goes back to the game
void Game::handleMenuKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_S)
    {
        saveAndQuit();
    }
    else if (key.key == SDLK_ESCAPE)
    {
        state_ = GameState::Playing;
        redraw_ = true;
    }
}

// Saves the run and ends the game. If the save can't be written, the game
// stays open, and says why, so the run isn't lost
void Game::saveAndQuit()
{
    if (writeSave())
    {
        running_ = false;
        return;
    }
    log_.add("The run couldn't be saved: " + std::string(SDL_GetError()));
    state_ = GameState::Playing;
    redraw_ = true;
}

// The save has no place for a choice half made, such as what a scroll of
// identify will name, or where a scroll of lightning or fireball goes.
// The scroll has left the pack by then, so a run saved as it is would lose
// it. Instead, the scroll goes back in the pack, as though it hadn't been
// read, though its name stays known. There's room for it, since it has
// only just left
void Game::putBackUnfinishedScroll()
{
    ItemEffect effect = ItemEffect::None;
    if (state_ == GameState::Inventory &&
        inventoryAction_ == InventoryAction::Identify)
    {
        effect = ItemEffect::Identify;
    }
    else if (state_ == GameState::Targeting &&
             targetPurpose_ == TargetPurpose::Lightning)
    {
        effect = ItemEffect::Lightning;
    }
    else if (state_ == GameState::Targeting &&
             targetPurpose_ == TargetPurpose::Fireball)
    {
        effect = ItemEffect::Fireball;
    }
    if (effect == ItemEffect::None)
    {
        return;
    }
    for (const ItemDef& def : data_.items())
    {
        if (def.effect == effect)
        {
            player_.inventory.add(Item{ &def });
            return;
        }
    }
}

// Stepping onto a monster attacks it. An attack or a step takes a turn, and
// then the rest of the turn plays out. Bumping into a wall takes no turn,
// unless the player staggered into it. Stepping onto a trap springs it,
// and a step onto a trap the player knows of is asked about first, unless
// it's confirmed already
void Game::playerStep(Point step, bool confirmed)
{
    log_.markRead();

    // Confused, half of the player's steps go in a direction of their own.
    // A confirmed step has had its roll
    bool moving = step.x != 0 || step.y != 0;
    bool staggered = false;
    if (!confirmed && moving && player_.confused > 0 &&
        rng_.roll(0, 1) == 0)
    {
        step = DIRECTIONS[rng_.roll(0, 7)];
        log_.add("You stagger.");
        staggered = true;
    }

    Point to = { player_.pos.x + step.x, player_.pos.y + step.y };
    Monster* monster = monsterAt(to);
    if (monster != nullptr)
    {
        playerAttacks(*monster);
        removeDeadMonsters();
        finishTurn();
    }
    else if (level_.map.at(to.x, to.y) == Terrain::ClosedDoor)
    {
        // Bumping a closed door opens it, which is the turn
        openDoor(to);
        log_.add("You open the door.");
        finishTurn();
    }
    else if (level_.map.isWalkable(to.x, to.y))
    {
        // A trap the player knows of isn't walked onto by accident, but
        // can't be avoided in a stagger
        Trap* trap = trapAt(to);
        bool knownTrap = trap != nullptr && trap->known && moving;
        if (knownTrap && !staggered && !confirmed)
        {
            std::string name = trapDefOf(trap->kind).name;
            pendingStep_ = step;
            confirmPurpose_ = ConfirmPurpose::Step;
            state_ = GameState::Confirming;
            log_.add("Step onto the " + name + "? (y or n)");
            return;
        }

        player_.pos = to;
        updateSight();
        // Walking in noisy armor makes a noise. Arriving on an item, the
        // player sees what it is
        bool moved = step.x != 0 || step.y != 0;
        if (moved && player_.armor && player_.armor->def->noisy)
        {
            makeNoise(player_.pos, FOOTSTEP_LOUDNESS);
        }
        const FloorItem* here = floorItemAt(player_.pos);
        if (moved && here != nullptr)
        {
            log_.add("You see " + itemName(here->item) + " here.");
        }

        // A trapdoor ends the turn on the floor below, as the stairs do
        if (moved && trap != nullptr && springTrap(*trap))
        {
            return;
        }
        finishTurn();
    }
    else
    {
        log_.add("You bump into the wall.");

        // A stagger into the wall still costs the turn, since the player
        // didn't choose it
        if (staggered)
        {
            finishTurn();
        }
    }
}

// Starts aiming: the cursor goes to the nearest monster in sight, if
// there is one, and otherwise stays on the player until it's moved
void Game::startTargeting(TargetPurpose purpose)
{
    targetPurpose_ = purpose;
    state_ = GameState::Targeting;
    lookCursor_ = player_.pos;
    targetNext();
    log_.add("Tab picks a monster, the mouse or the moving keys aim, and "
             "Enter or a click fires.");
    redraw_ = true;
}

// Aiming: the moving keys move the cursor, Tab moves it to the next
// monster in sight, Enter fires at it, and Esc stops
void Game::handleTargetingKey(const SDL_KeyboardEvent& key)
{
    Point step;
    if (key.key == SDLK_ESCAPE)
    {
        stopTargeting();
    }
    else if (key.key == SDLK_TAB)
    {
        targetNext();
    }
    else if (key.key == SDLK_RETURN || key.key == SDLK_KP_ENTER)
    {
        fireAtCursor();
    }
    else if (stepFor(key, step))
    {
        Point to = { lookCursor_.x + step.x, lookCursor_.y + step.y };
        if (level_.map.inBounds(to.x, to.y))
        {
            lookCursor_ = to;
        }
    }
    redraw_ = true;
}

// Moves the cursor to the next monster in sight, nearest first: each Tab
// goes one further out, and after the farthest, back to the nearest
void Game::targetNext()
{
    std::vector<const Monster*> inSight;
    for (const Monster& monster : level_.monsters)
    {
        if (level_.map.isVisible(monster.pos.x, monster.pos.y))
        {
            inSight.push_back(&monster);
        }
    }
    if (inSight.empty())
    {
        return;
    }

    // Nearest first, and between two as near, the one higher up and then
    // further left, so the order is always the same
    Point from = player_.pos;
    std::sort(inSight.begin(), inSight.end(),
              [from](const Monster* a, const Monster* b)
              {
                  int nearA = distanceSquared(a->pos, from);
                  int nearB = distanceSquared(b->pos, from);
                  if (nearA != nearB)
                  {
                      return nearA < nearB;
                  }
                  if (a->pos.y != b->pos.y)
                  {
                      return a->pos.y < b->pos.y;
                  }
                  return a->pos.x < b->pos.x;
              });
    size_t next = 0;
    for (size_t i = 0; i < inSight.size(); ++i)
    {
        if (inSight[i]->pos == lookCursor_)
        {
            next = (i + 1) % inSight.size();
        }
    }
    lookCursor_ = inSight[next]->pos;
}

// Esc while aiming, which on its own takes no turn. But a scroll that's
// aimed has been read, so its magic is gone, and the reading took a turn
void Game::stopTargeting()
{
    state_ = GameState::Playing;
    if (targetPurpose_ == TargetPurpose::Lightning)
    {
        log_.add("The lightning crackles away into nothing.");
        finishTurn();
    }
    else if (targetPurpose_ == TargetPurpose::Fireball)
    {
        log_.add("The fire flickers out.");
        finishTurn();
    }
}

// f fires an arrow from the quiver with the bow, at whatever the player
// aims at. Without a bow ready, or arrows, there's nothing to fire
void Game::fire()
{
    log_.markRead();
    if (!player_.bow)
    {
        log_.add("You have no bow ready.");
        return;
    }
    if (!player_.quiver)
    {
        log_.add("Your quiver is empty.");
        return;
    }
    startTargeting(TargetPurpose::Fire);
}

// t has the player choose what to throw: only what's made for throwing,
// a dagger or a dart. Then they aim it
void Game::chooseThrow(int slot)
{
    if (!player_.inventory.items()[slot].def->thrown)
    {
        log_.markRead();
        log_.add("That isn't made for throwing.");
        return;
    }
    throwSlot_ = slot;
    startTargeting(TargetPurpose::Throw);
}

// Fires at the cursor, along the line of fire, which takes the turn: the
// lightning, an arrow from the quiver, or one of what's thrown
void Game::fireAtCursor()
{
    std::vector<Point> path = lineOfFire();
    if (path.empty())
    {
        // The cursor is on the player, or the line's first cell is a wall
        // or a closed door
        if (lookCursor_ == player_.pos)
        {
            log_.add("Move the cursor off yourself first.");
        }
        else
        {
            log_.add("Something solid is in the way.");
        }
        return;
    }
    state_ = GameState::Playing;
    if (targetPurpose_ == TargetPurpose::Lightning)
    {
        castLightning(path);
    }
    else if (targetPurpose_ == TargetPurpose::Fireball)
    {
        castFireball(path.back());
    }
    else if (targetPurpose_ == TargetPurpose::Fire)
    {
        const ItemDef& bow = *player_.bow->def;
        Item arrow = *player_.quiver;
        arrow.count = 1;
        --player_.quiver->count;
        if (player_.quiver->count == 0)
        {
            player_.quiver.reset();
        }
        log_.add("You shoot " + itemName(arrow) + ".");
        shootMissile(arrow, path, bow.minDamage, bow.maxDamage);
    }
    else
    {
        Item thrown = player_.inventory.items()[throwSlot_];
        thrown.count = 1;
        player_.inventory.removeOne(throwSlot_);
        log_.add("You throw " + itemName(thrown) + ".");
        shootMissile(thrown, path, thrown.def->minDamage,
                     thrown.def->maxDamage);
    }
    removeDeadMonsters();
    finishTurn();
}

// The cells a shot at the cursor passes through: along the line from the
// player, as far as its range, past the cursor if it's nearer. It stops
// short of a wall or a closed door
std::vector<Point> Game::lineOfFire() const
{
    int range = LIGHTNING_RANGE;
    if (targetPurpose_ == TargetPurpose::Fire)
    {
        range = player_.bow->def->range;
    }
    else if (targetPurpose_ == TargetPurpose::Throw)
    {
        range = THROW_RANGE;
    }
    else if (targetPurpose_ == TargetPurpose::Fireball)
    {
        // A fireball goes no further than the cursor, where it bursts
        range = SDL_min(FIREBALL_RANGE,
                        stepsBetween(player_.pos, lookCursor_));
    }

    // Lightning goes through creatures; anything else stops at the first,
    // whose cell is the path's last
    bool throughCreatures = targetPurpose_ == TargetPurpose::Lightning;

    std::vector<Point> path;
    for (Point cell : lineFrom(player_.pos, lookCursor_, range))
    {
        if (!level_.map.isWalkable(cell.x, cell.y))
        {
            break;
        }
        path.push_back(cell);
        bool creature = false;
        for (const Monster& monster : level_.monsters)
        {
            creature = creature || monster.pos == cell;
        }
        if (creature && !throughCreatures)
        {
            break;
        }
    }
    return path;
}

// A missile flies along the path, to the first creature on it, or the
// path's end. A creature is hit with a chance that falls with distance,
// and a hit does a roll of the damage, less its defense, but at least 1.
// Hit or miss, the creature hunts the player if it lives, and the missile
// lands where it stopped, but an arrow breaks one time in BREAK_ONE_IN
void Game::shootMissile(Item missile, const std::vector<Point>& path,
                        int minDamage, int maxDamage)
{
    std::string name = "the " + missile.def->name;
    Point landing = path.back();
    for (size_t i = 0; i < path.size(); ++i)
    {
        Monster* target = monsterAt(path[i]);
        if (target == nullptr)
        {
            continue;
        }
        landing = path[i];
        int chance = hitChance(static_cast<int>(i) + 1);
        if (rng_.roll(1, 100) <= chance)
        {
            int damage = rollDamage(rng_, minDamage, maxDamage, 0,
                                    target->defense);
            hurtMonster(*target, damage, name);
            makeNoise(target->pos, FIGHT_LOUDNESS);
        }
        else
        {
            log_.add(capitalized(name) + " misses the " +
                     target->def->name + ".");
        }
        if (target->isAlive())
        {
            alarm(*target);
        }
        break;
    }

    bool arrow = missile.def->itemClass == ItemClass::Missile &&
                 !missile.def->thrown;
    if (arrow && rng_.roll(1, BREAK_ONE_IN) == 1)
    {
        log_.add(capitalized(name) + " breaks.");
        return;
    }
    dropNear(missile, landing);
}

// Puts an item down where it landed: on the cell, onto a stack of its own
// kind there if it stacks, or if anything else lies there, on a free cell
// beside it. With nowhere free, it's lost
void Game::dropNear(Item item, Point cell)
{
    std::vector<Point> cells = { cell };
    for (Point step : DIRECTIONS)
    {
        cells.push_back({ cell.x + step.x, cell.y + step.y });
    }
    for (Point here : cells)
    {
        if (!level_.map.isWalkable(here.x, here.y))
        {
            continue;
        }
        bool taken = false;
        for (FloorItem& lying : level_.items)
        {
            if (lying.pos == here && lying.item.def == item.def &&
                item.def->stacks())
            {
                lying.item.count += item.count;
                return;
            }
            taken = taken || lying.pos == here;
        }
        if (!taken)
        {
            FloorItem dropped;
            dropped.pos = here;
            dropped.item = item;
            dropped.seen = level_.map.isVisible(here.x, here.y);
            level_.items.push_back(dropped);
            return;
        }
    }
}

// A bolt of lightning, along the path: every monster on it is struck, for
// LIGHTNING_MIN to LIGHTNING_MAX damage, since magic ignores armor. They're
// all struck at once, before any of them is heard, and those it didn't
// kill hunt the player
void Game::castLightning(const std::vector<Point>& path)
{
    log_.add("A bolt of lightning flashes out!");
    std::vector<Monster*> struck;
    for (Point cell : path)
    {
        Monster* monster = monsterAt(cell);
        if (monster != nullptr && monster->isAlive())
        {
            int damage = rng_.roll(LIGHTNING_MIN, LIGHTNING_MAX);
            hurtMonster(*monster, damage, "the lightning");
            struck.push_back(monster);
        }
    }
    for (Monster* monster : struck)
    {
        makeNoise(monster->pos, FIGHT_LOUDNESS);
        if (monster->isAlive())
        {
            alarm(*monster);
        }
    }
}

// A monster takes damage from afar, and the player is told: "The
// lightning hits the goblin for 9." On its own, it wakes no one, and sets
// no one hunting
void Game::hurtMonster(Monster& monster, int damage, const std::string& by)
{
    monster.hp -= damage;
    std::string name = monster.def->name;
    if (!monster.isAlive())
    {
        log_.add(capitalized(by) + " kills the " + name + "!");
        return;
    }
    log_.add(capitalized(by) + " hits the " + name + " for " +
             std::to_string(damage) + ".");
}

// The fireball bursts at the end of its path. Everyone within
// FIREBALL_RADIUS steps that the burst can reach takes FIREBALL_MIN to
// FIREBALL_MAX damage, armor or no armor, the player too, if they stand
// too close. All are burned at once, before the blast is heard, and the
// monsters it didn't kill hunt the player
void Game::castFireball(Point center)
{
    log_.add("The fireball bursts!");
    std::vector<Monster*> struck;
    for (Point cell : burstAt(center, FIREBALL_RADIUS))
    {
        Monster* monster = monsterAt(cell);
        if (cell == player_.pos)
        {
            int damage = rng_.roll(FIREBALL_MIN, FIREBALL_MAX);
            player_.hp -= damage;
            log_.add("You're caught in the blast, for " +
                     std::to_string(damage) + "!");
            if (!player_.isAlive())
            {
                player_.killedBy = "your own fireball";
            }
        }
        else if (monster != nullptr && monster->isAlive())
        {
            int damage = rng_.roll(FIREBALL_MIN, FIREBALL_MAX);
            hurtMonster(*monster, damage, "the fireball");
            struck.push_back(monster);
        }
    }
    makeNoise(center, FIREBALL_LOUDNESS);
    for (Monster* monster : struck)
    {
        if (monster->isAlive())
        {
            alarm(*monster);
        }
    }
}

// The cells a burst reaches from its center: those within radius steps
// that the center can see, with no wall in between, so walls shelter
// what's behind them. The center is one of them
std::vector<Point> Game::burstAt(Point center, int radius) const
{
    std::vector<Point> cells;
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            Point cell = { center.x + dx, center.y + dy };
            if (!level_.map.isWalkable(cell.x, cell.y))
            {
                continue;
            }
            bool open = true;
            int steps = stepsBetween(center, cell);
            for (Point between : lineFrom(center, cell, steps))
            {
                open = open && (between == cell ||
                                level_.map.isWalkable(between.x, between.y));
            }
            if (open)
            {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

// A yes or no, about stepping onto a trap the player knows of, or about
// going down without this floor's Amulet: y does it, and any other key
// stays put
void Game::handleConfirmKey(const SDL_KeyboardEvent& key)
{
    state_ = GameState::Playing;
    if (key.key != SDLK_Y)
    {
        log_.add("You stay where you are.");
    }
    else if (confirmPurpose_ == ConfirmPurpose::Descend)
    {
        descend(true);
    }
    else
    {
        playerStep(pendingStep_, true);
    }
    redraw_ = true;
}

// The player has stepped onto a trap: it's known from now on, and it does
// what it does. Returns true if the player fell through a trapdoor to the
// floor below, which ends the turn there
bool Game::springTrap(Trap& trap)
{
    trap.known = true;
    switch (trap.kind)
    {
    case TrapKind::Dart:
        log_.add("A dart flies out of the wall, and hits you!");
        player_.hp -= rollDamage(rng_, DART_MIN, DART_MAX, 0,
                                 player_.defense);
        if (!player_.isAlive())
        {
            player_.killedBy = "a dart trap";
        }
        break;
    case TrapKind::PoisonNeedle:
        log_.add("A needle pricks you. You feel very sick!");
        player_.poisoned = SDL_max(player_.poisoned, POISON_TURNS);
        break;
    case TrapKind::Alarm:
        log_.add("An alarm rings out across the floor!");
        makeNoise(trap.pos, ALARM_LOUDNESS);
        break;
    case TrapKind::Teleport:
        log_.add("The floor flashes, and you're somewhere else!");
        teleportPlayer();
        break;
    case TrapKind::Trapdoor:
        log_.add("A trapdoor opens under you!");
        player_.hp -= rng_.roll(FALL_MIN, FALL_MAX);
        if (!player_.isAlive())
        {
            player_.killedBy = "a fall through a trapdoor";
            break;
        }
        arriveOnFloor("You fall through");
        return true;
    }
    return false;
}

// The player strikes a monster. A blow in melee always hits
void Game::playerAttacks(Monster& monster)
{
    int damage = rollDamage(rng_, player_.minDamage(), player_.maxDamage(),
                            player_.strength, monster.defense);
    monster.hp -= damage;
    std::string name = monster.def->name;
    if (!monster.isAlive())
    {
        log_.add("You kill the " + name + "!");
        makeNoise(player_.pos, FIGHT_LOUDNESS);
        return;
    }

    log_.add("You hit the " + name + " for " + std::to_string(damage) + ".");

    // The noise comes after the blow is told, so the log keeps the order
    makeNoise(player_.pos, FIGHT_LOUDNESS);

    // A blow wakes a monster, and sets it hunting whoever struck it
    alarm(monster);
}

// A monster the player attacks, if it was asleep or wandering, hunts them
// now, and its pack with it
void Game::alarm(Monster& monster)
{
    bool unaware = monster.state == MonsterState::Asleep ||
                   monster.state == MonsterState::Wandering;
    if (unaware)
    {
        monster.state = MonsterState::Hunting;
        monster.lastSeen = player_.pos;
        wakePack(monster);
    }
}

// g picks up the item the player stands on, which takes a turn: gold and
// an Amulet are counted, arrows like those in the quiver go into it, and
// anything else into the pack, if there's room. Finding nothing there, or
// no room, takes no turn
void Game::pickUp()
{
    log_.markRead();
    const FloorItem* here = floorItemAt(player_.pos);
    if (here == nullptr)
    {
        log_.add("There's nothing here to pick up.");
        return;
    }
    if (here->item.def->itemClass == ItemClass::Gold)
    {
        player_.gold += here->item.count;
    }
    else if (here->item.def->itemClass == ItemClass::Amulet)
    {
        ++player_.amulets;
    }
    else if (player_.quiver && here->item.def == player_.quiver->def)
    {
        // Arrows of the kind in the quiver go straight back into it
        player_.quiver->count += here->item.count;
    }
    else if (!player_.inventory.add(here->item))
    {
        log_.add("Your pack is full.");
        return;
    }

    log_.add("You pick up " + itemName(here->item) + ".");
    if (here->item.def->itemClass == ItemClass::Amulet)
    {
        log_.add(player_.amulets == AMULETS
                 ? "You have all three Amulets!"
                 : "That's " + std::to_string(player_.amulets) + " of the " +
                   std::to_string(AMULETS) + " Amulets.");
    }
    std::erase_if(level_.items, [this](const FloorItem& floorItem)
    {
        return floorItem.pos == player_.pos;
    });
    finishTurn();
}

// Wields a weapon from the pack, which takes a turn. The one in hand, if
// there is one, goes back into the pack
void Game::wield(int slot)
{
    state_ = GameState::Playing;
    log_.markRead();
    Item weapon = player_.inventory.removeStack(slot);
    if (player_.weapon)
    {
        player_.inventory.add(*player_.weapon);
        log_.add("You put away " + itemName(*player_.weapon) + ".");
    }
    player_.weapon = weapon;
    log_.add("You wield " + itemName(weapon) + ".");
    finishTurn();
}

// Readies a bow from the pack, which takes a turn. The bow that was
// ready, if there was one, goes back into the pack
void Game::readyBow(int slot)
{
    state_ = GameState::Playing;
    log_.markRead();
    Item bow = player_.inventory.removeStack(slot);
    if (player_.bow)
    {
        player_.inventory.add(*player_.bow);
        log_.add("You put away " + itemName(*player_.bow) + ".");
    }
    player_.bow = bow;
    log_.add("You ready " + itemName(bow) + ".");
    finishTurn();
}

// Puts a stack of arrows from the pack in the quiver, which takes a turn.
// Arrows of the same kind join the ones there; arrows of another kind go
// back into the pack
void Game::fillQuiver(int slot)
{
    state_ = GameState::Playing;
    log_.markRead();
    Item arrows = player_.inventory.removeStack(slot);
    if (player_.quiver && player_.quiver->def == arrows.def)
    {
        player_.quiver->count += arrows.count;
    }
    else
    {
        if (player_.quiver)
        {
            player_.inventory.add(*player_.quiver);
        }
        player_.quiver = arrows;
    }
    log_.add("Your quiver holds " + itemName(*player_.quiver) + ".");
    finishTurn();
}

// Puts on armor from the pack, which takes a turn. Armor already worn, if
// there is any, goes back into the pack. The new armor's defense is the
// player's
void Game::wear(int slot)
{
    state_ = GameState::Playing;
    log_.markRead();
    Item armor = player_.inventory.removeStack(slot);
    if (player_.armor)
    {
        player_.inventory.add(*player_.armor);
        log_.add("You take off " + itemName(*player_.armor) + ".");
    }
    player_.armor = armor;
    player_.defense = armor.def->defense;
    log_.add("You put on " + itemName(armor) + ".");
    finishTurn();
}

// Opens the inventory, for using an item, dropping one or identifying one.
// There's no point in choosing something to drop from an empty pack
void Game::openInventory(InventoryAction action)
{
    if (action == InventoryAction::Drop &&
        player_.inventory.items().empty())
    {
        log_.markRead();
        log_.add("You have nothing to drop.");
        redraw_ = true;
        return;
    }
    bool throwable = false;
    for (const Item& item : player_.inventory.items())
    {
        throwable = throwable || item.def->thrown;
    }
    if (action == InventoryAction::Throw && !throwable)
    {
        log_.markRead();
        log_.add("You have nothing made for throwing.");
        redraw_ = true;
        return;
    }
    state_ = GameState::Inventory;
    inventoryAction_ = action;
    redraw_ = true;
}

// Uses an item from the pack, which takes a turn. A weapon is wielded,
// armor worn, a bow readied, and arrows put in the quiver; darts are only
// thrown. A potion is drunk, or a scroll read: its kind is known from then
// on, so the player learns what it was, and then sees what it does
void Game::useItem(int slot)
{
    const ItemDef& kind = *player_.inventory.items()[slot].def;
    ItemClass itemClass = kind.itemClass;
    if (itemClass == ItemClass::Weapon)
    {
        wield(slot);
        return;
    }
    if (itemClass == ItemClass::Bow)
    {
        readyBow(slot);
        return;
    }
    if (itemClass == ItemClass::Missile && !kind.thrown)
    {
        fillQuiver(slot);
        return;
    }
    if (itemClass == ItemClass::Missile)
    {
        state_ = GameState::Playing;
        log_.markRead();
        log_.add("Darts are for throwing: t throws one.");
        return;
    }
    if (itemClass == ItemClass::Armor)
    {
        wear(slot);
        return;
    }

    state_ = GameState::Playing;
    log_.markRead();

    // One of the stack, copied before it leaves the pack
    Item used = player_.inventory.items()[slot];
    used.count = 1;
    player_.inventory.removeOne(slot);

    bool potion = used.def->itemClass == ItemClass::Potion;
    log_.add((potion ? "You drink " : "You read ") + itemName(used) + ".");
    if (!identification_.isKnown(*used.def))
    {
        identification_.learn(*used.def);
        log_.add("It was " + itemName(used) + ".");
    }
    applyEffect(*used.def);

    // A scroll that's aimed finishes its turn when the player fires it
    if (state_ != GameState::Targeting)
    {
        finishTurn();
    }
}

// Drops a whole stack from the pack where the player stands, which takes a
// turn. A cell holds one stack, so it can only join one of its own kind,
// and only if its kind stacks
void Game::dropItem(int slot)
{
    state_ = GameState::Playing;
    log_.markRead();
    const FloorItem* here = floorItemAt(player_.pos);
    const ItemDef* kind = player_.inventory.items()[slot].def;
    if (here != nullptr && (here->item.def != kind || !kind->stacks()))
    {
        log_.add("There's already something here.");
        return;
    }

    Item stack = player_.inventory.removeStack(slot);
    log_.add("You drop " + itemName(stack) + ".");
    if (here != nullptr)
    {
        for (FloorItem& floorItem : level_.items)
        {
            if (floorItem.pos == player_.pos)
            {
                floorItem.item.count += stack.count;
            }
        }
    }
    else
    {
        FloorItem dropped;
        dropped.pos = player_.pos;
        dropped.item = stack;
        dropped.seen = true;
        level_.items.push_back(dropped);
    }
    finishTurn();
}

// The scroll of identify's choice: learns the kind of the item chosen.
// The scroll took the turn, so this takes none
void Game::identifyItem(int slot)
{
    const Item& chosen = player_.inventory.items()[slot];
    if (identification_.isKnown(*chosen.def))
    {
        log_.add("You already know what that is.");
        return;
    }
    identification_.learn(*chosen.def);
    log_.add("You identify " + itemName(chosen) + ".");
    state_ = GameState::Playing;
}

// What an item does when it's used
void Game::applyEffect(const ItemDef& def)
{
    switch (def.effect)
    {
    case ItemEffect::Heal:
        player_.hp = SDL_min(player_.maxHp, player_.hp + HEAL_AMOUNT);
        log_.add("You feel better.");
        break;
    case ItemEffect::HealFully:
        player_.hp = player_.maxHp;
        player_.poisoned = 0;
        log_.add("You feel much better.");
        break;
    case ItemEffect::Strengthen:
        ++player_.strength;
        log_.add("You feel stronger!");
        break;
    case ItemEffect::Teleport:
        teleportPlayer();
        log_.add("You are whisked away!");
        break;
    case ItemEffect::MapFloor:
        // Every cell of floor, and every wall beside floor, is known, and
        // every item lying on the floor is seen
        for (int y = 0; y < MAP_H; ++y)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                bool ground = level_.map.isWalkable(x, y);
                if (ground || level_.map.bordersFloor(x, y))
                {
                    level_.map.setExplored(x, y);
                }
            }
        }
        for (FloorItem& onFloor : level_.items)
        {
            onFloor.seen = true;
        }
        log_.add("A map of the floor forms in your mind.");
        break;
    case ItemEffect::Identify:
        // The player chooses what to identify, if there's anything left
        // to learn in the pack
        for (const Item& item : player_.inventory.items())
        {
            if (!identification_.isKnown(*item.def))
            {
                openInventory(InventoryAction::Identify);
                log_.add("Choose an item to identify.");
                return;
            }
        }
        log_.add("You already know everything you carry.");
        break;
    case ItemEffect::Poison:
        player_.poisoned = SDL_max(player_.poisoned, POISON_TURNS);
        log_.add("You feel very sick!");
        break;
    case ItemEffect::Regenerate:
        player_.regenerating = SDL_max(player_.regenerating,
                                       REGENERATION_TURNS);
        log_.add("You feel your wounds begin to close.");
        break;
    case ItemEffect::Confuse:
        player_.confused = SDL_max(player_.confused, CONFUSION_TURNS);
        log_.add("You feel dizzy!");
        break;
    case ItemEffect::Frighten:
        frightenMonsters();
        break;
    case ItemEffect::Lightning:
        log_.add("Lightning crackles in the air. Where will you aim it?");
        startTargeting(TargetPurpose::Lightning);
        break;
    case ItemEffect::Fireball:
        log_.add("Fire gathers in your hands. Where will you throw it?");
        startTargeting(TargetPurpose::Fireball);
        break;
    }
}

// The scroll of fear: every monster in sight is afraid for FEAR_TURNS,
// waking if it was asleep, and flees, whatever its health. A monster that
// can't move can't flee, and one that never flees doesn't, so they take
// no notice
void Game::frightenMonsters()
{
    bool anyone = false;
    for (Monster& monster : level_.monsters)
    {
        bool inSight = level_.map.isVisible(monster.pos.x, monster.pos.y);
        bool unmoved = monster.def->has(Trait::STATIONARY) ||
                       monster.def->has(Trait::NEVER_FLEES);
        if (inSight && !unmoved)
        {
            monster.afraid = FEAR_TURNS;
            monster.state = MonsterState::Fleeing;
            monster.lastSeen.reset();
            announce(monster, "flees in terror!");
            anyone = true;
        }
    }
    if (!anyone)
    {
        log_.add("A dreadful cry echoes, and fades.");
    }
}

// One turn of an entity's statuses, each counting down: poison hurts,
// then regeneration heals. Poison that kills ends it there, so no
// regeneration can bring the dead back
void Game::tickStatuses(Entity& entity)
{
    if (entity.confused > 0)
    {
        --entity.confused;
    }
    if (entity.afraid > 0)
    {
        --entity.afraid;
    }
    if (entity.poisoned > 0)
    {
        --entity.poisoned;
        --entity.hp;
    }
    if (!entity.isAlive())
    {
        return;
    }
    if (entity.regenerating > 0)
    {
        --entity.regenerating;
        entity.hp = SDL_min(entity.maxHp, entity.hp + 1);
    }
}

// Works out what the player can see from where they stand, and notes
// every item that comes into sight: from then on, they know it's there
void Game::updateSight()
{
    Fov::compute(level_.map, player_.pos, SIGHT_RADIUS);
    for (FloorItem& floorItem : level_.items)
    {
        if (level_.map.isVisible(floorItem.pos.x, floorItem.pos.y))
        {
            floorItem.seen = true;
        }
    }
}

// Moves the player to a free floor cell chosen at random
void Game::teleportPlayer()
{
    for (int tries = 0; tries < 100; ++tries)
    {
        std::optional<Point> cell = randomFloorCell();
        if (cell && *cell != player_.pos && monsterAt(*cell) == nullptr)
        {
            player_.pos = *cell;
            updateSight();
            return;
        }
    }
}

// Takes the dead monsters out of the dungeon
void Game::removeDeadMonsters()
{
    std::erase_if(level_.monsters, [](const Monster& monster)
    {
        return !monster.isAlive();
    });
}

// The rest of a turn, once the player has acted, always in this order:
// the monsters act; statuses tick, for the player and then the monsters;
// the player heals a little now and then; and last of all comes the one
// check for the player's death
void Game::finishTurn()
{
    // The player may have moved, so the distances to them are new, and so
    // is the way to run from them
    toPlayer_.compute(level_.map, { player_.pos });
    fromPlayer_ = toPlayer_.fleeMap(level_.map);
    monstersAct();
    ++turn_;

    // A player the monsters have killed has no statuses left to tick
    if (player_.isAlive())
    {
        bool wasPoisoned = player_.poisoned > 0;
        bool wasRegenerating = player_.regenerating > 0;
        bool wasConfused = player_.confused > 0;
        tickStatuses(player_);
        if (wasConfused && player_.confused == 0)
        {
            log_.add("Your head clears.");
        }
        if (!player_.isAlive())
        {
            player_.killedBy = "poison";
        }
        else if (wasPoisoned && player_.poisoned == 0)
        {
            log_.add("You feel the poison leave you.");
        }
        if (player_.isAlive() && wasRegenerating &&
            player_.regenerating == 0)
        {
            log_.add("Your wounds stop closing.");
        }
    }
    for (Monster& monster : level_.monsters)
    {
        // A monster killed this turn, as by a dragon's fire, has had its
        // death told already
        if (!monster.isAlive())
        {
            continue;
        }
        bool wasAfraid = monster.afraid > 0;
        tickStatuses(monster);
        // Fear that runs out ends the flight it began, unless the monster
        // is badly wounded, and would run anyway
        if (wasAfraid && monster.afraid == 0 && !isBadlyWounded(monster))
        {
            monster.state = MonsterState::Hunting;
        }
        if (!monster.isAlive())
        {
            announce(monster, "dies.");
        }
        else if (monster.def->has(Trait::REGENERATES))
        {
            monster.hp = SDL_min(monster.maxHp, monster.hp + REGENERATION);
        }
    }
    removeDeadMonsters();

    bool healing = player_.isAlive() && player_.hp < player_.maxHp;
    if (healing && turn_ % HEAL_EVERY == 0)
    {
        ++player_.hp;
    }

    if (!player_.isAlive())
    {
        // Death is final: the save goes too, so the run can't be continued
        log_.add("You die...");
        state_ = GameState::Dead;
        removeSave();
    }
}

// The monsters' turn. What each one does depends on its state. Once the
// player is dead, no monster acts
void Game::monstersAct()
{
    for (Monster& monster : level_.monsters)
    {
        if (!player_.isAlive())
        {
            return;
        }

        // A monster killed this turn, by another's fire, does nothing
        if (!monster.isAlive())
        {
            continue;
        }

        // A slow monster acts only every other turn
        if (monster.def->has(Trait::SLOW) && turn_ % 2 == 1)
        {
            continue;
        }

        // An attack from afar gets ready again, a turn at a time
        if (monster.cooldown > 0)
        {
            --monster.cooldown;
        }

        // A monster that can't move, once awake, does only what it can
        // do where it is
        if (monster.def->has(Trait::STATIONARY) &&
            monster.state != MonsterState::Asleep)
        {
            actStationary(monster);
            continue;
        }

        switch (monster.state)
        {
        case MonsterState::Asleep:
            actAsleep(monster);
            break;
        case MonsterState::Wandering:
            actWandering(monster);
            break;
        case MonsterState::Hunting:
            actHunting(monster);
            break;
        case MonsterState::Fleeing:
            actFleeing(monster);
            break;
        }
    }
}

// An asleep monster doesn't move. It wakes if the player comes next to it,
// and, while it could see them, with a chance of 1 in WAKE_ONE_IN a turn.
// Woken, it hunts them
void Game::actAsleep(Monster& monster)
{
    bool nextTo = isNextTo(monster.pos, player_.pos);
    bool stirs = seesPlayer(monster) && rng_.roll(1, WAKE_ONE_IN) == 1;
    if (nextTo || stirs)
    {
        monster.state = MonsterState::Hunting;
        monster.lastSeen = player_.pos;
        announce(monster, "wakes up!");
        wakePack(monster);
    }
}

// A pack wakes together: when one of its members wakes, the others still
// asleep wake too, and do as it does. The player is told of each one they
// can see. A dead member, not yet taken off the floor, is left alone
void Game::wakePack(const Monster& woken)
{
    if (woken.packId == 0)
    {
        return;
    }
    for (Monster& other : level_.monsters)
    {
        bool sameAsleep = other.packId == woken.packId &&
                          other.state == MonsterState::Asleep &&
                          other.isAlive();
        if (sameAsleep)
        {
            other.state = woken.state;
            other.lastSeen = woken.lastSeen;
            other.target = woken.target;
            announce(other, "wakes up!");
        }
    }
}

// A wandering monster walks toward a cell it chose at random, and chooses
// another when it gets there, or can't. If it sees the player, it starts
// hunting them, from its next turn
void Game::actWandering(Monster& monster)
{
    if (seesPlayer(monster))
    {
        monster.state = MonsterState::Hunting;
        monster.lastSeen = player_.pos;
        announce(monster, "notices you!");
        return;
    }

    if (!monster.target || monster.pos == *monster.target)
    {
        monster.target = randomFloorCell();
    }
    if (monster.target && !walkToward(monster, *monster.target))
    {
        monster.target.reset();
    }
}

// A hunting monster goes after the player, unless it's badly wounded or
// afraid, when it flees instead. Seeing them, it attacks from afar if it
// can; if not, it attacks if it's next to them, and otherwise comes closer
// by the shortest way, remembering where they are. One that keeps its
// distance backs away first, if they're too close, and comes no closer
// than it has to. Having lost them, it goes to where it last saw them, and
// if they're not there, it searches nearby for a while, then gives up
void Game::actHunting(Monster& monster)
{
    bool fearless = monster.def->has(Trait::NEVER_FLEES);
    if (!fearless && (isBadlyWounded(monster) || monster.afraid > 0))
    {
        monster.state = MonsterState::Fleeing;
        monster.lastSeen.reset();
        announce(monster, "flees!");
        actFleeing(monster);
        return;
    }

    if (seesPlayer(monster))
    {
        monster.lastSeen = player_.pos;
        monster.searchTurns = 0;
        bool keeps = monster.def->has(Trait::KEEPS_DISTANCE);
        if (keeps && backsAway(monster))
        {
            return;
        }
        if (attacksFromAfar(monster))
        {
            return;
        }
        if (keeps)
        {
            keepDistance(monster);
        }
        else if (isNextTo(monster.pos, player_.pos))
        {
            monsterAttacks(monster);
        }
        else if (!movesErratically(monster))
        {
            moveMonster(monster, stepDown(monster, toPlayer_));
        }
        return;
    }

    // It goes to where it last saw them, but not back there once it's
    // searching. If it can't get any closer, as when another monster is in
    // the way, it gives up
    bool onTheWay = monster.lastSeen && monster.pos != *monster.lastSeen &&
                    monster.searchTurns == 0;
    if (onTheWay)
    {
        if (!walkToward(monster, *monster.lastSeen))
        {
            giveUp(monster);
        }
        return;
    }

    // The player isn't where it last saw them. It searches nearby, a step
    // at a time in any direction, for a few turns
    if (monster.lastSeen && monster.searchTurns < SEARCH_TURNS)
    {
        ++monster.searchTurns;
        Point next = wanderStep(monster);
        int away = distanceSquared(next, *monster.lastSeen);
        if (away <= SEARCH_RADIUS * SEARCH_RADIUS)
        {
            moveMonster(monster, next);
        }
        return;
    }
    giveUp(monster);
}

// A fleeing monster runs from the player, down the flee map, while it can
// see them, and lies low while it can't. Cornered, with nowhere lower to
// go, it turns and fights. Healed past two thirds of its health, and no
// longer afraid, it hunts again
void Game::actFleeing(Monster& monster)
{
    bool healed = monster.hp * 3 > monster.maxHp * 2;
    if (healed && monster.afraid == 0)
    {
        monster.state = MonsterState::Hunting;
        actHunting(monster);
        return;
    }
    if (!seesPlayer(monster) || movesErratically(monster))
    {
        return;
    }

    Point next = stepDown(monster, fromPlayer_);
    if (next != monster.pos)
    {
        moveMonster(monster, next);
    }
    else if (isNextTo(monster.pos, player_.pos))
    {
        monsterAttacks(monster);
    }
}

// Takes a step toward a cell, on a Dijkstra map of the monster's own.
// Returns false if it couldn't get any closer
bool Game::walkToward(Monster& monster, Point goal)
{
    if (movesErratically(monster))
    {
        return true;
    }
    DijkstraMap toGoal;
    toGoal.compute(level_.map, { goal });
    Point next = stepDown(monster, toGoal);
    bool moved = next != monster.pos;
    moveMonster(monster, next);
    return moved;
}

// Half of an erratic monster's moves are random. Returns true if this one
// is: the monster has made it, and has no other move this turn
bool Game::movesErratically(Monster& monster)
{
    if (!monster.def->has(Trait::ERRATIC) || rng_.roll(0, 1) == 0)
    {
        return false;
    }
    moveMonster(monster, wanderStep(monster));
    return true;
}

// A hunter that's lost the player goes back to wandering
void Game::giveUp(Monster& monster)
{
    monster.state = MonsterState::Wandering;
    monster.lastSeen.reset();
    monster.target.reset();
    monster.searchTurns = 0;
    announce(monster, "gives up the hunt.");
}

// A floor cell chosen at random, for a wandering monster to head for, or
// nothing, if none turns up in a few hundred tries
std::optional<Point> Game::randomFloorCell()
{
    for (int tries = 0; tries < 500; ++tries)
    {
        Point cell = { rng_.roll(0, MAP_W - 1), rng_.roll(0, MAP_H - 1) };
        if (level_.map.isWalkable(cell.x, cell.y))
        {
            return cell;
        }
    }
    return std::nullopt;
}

// A monster that can't move. While it sees the player, a shrieker shrieks
// every SHRIEK_EVERY turns: a noise that brings other monsters to it
void Game::actStationary(Monster& monster)
{
    bool shrieks = monster.def->has(Trait::SHRIEKS) && seesPlayer(monster);
    if (shrieks && turn_ % SHRIEK_EVERY == 0)
    {
        announce(monster, "shrieks!");
        makeNoise(monster.pos, SHRIEK_LOUDNESS);
    }
}

// A noise spreads from where it's made, as far as its loudness, by the
// ways a creature could walk: a wall stops it, so it goes around. A monster
// asleep wakes if enough of the noise is left when it gets there, and goes
// to see what it was. One wandering goes to see, too. Hunters and those
// fleeing have better things to do
void Game::makeNoise(Point origin, int loudness)
{
    DijkstraMap spread;
    spread.compute(level_.map, { origin }, DOOR_MUFFLE);
    for (Monster& monster : level_.monsters)
    {
        // A monster the noise's own blow has killed hears nothing
        if (!monster.isAlive())
        {
            continue;
        }
        int heard = loudness - spread.at(monster.pos);
        if (heard < 0)
        {
            continue;
        }
        if (monster.state == MonsterState::Asleep && heard >= WAKE_LOUDNESS)
        {
            monster.state = MonsterState::Wandering;
            monster.target = origin;
            announce(monster, "wakes up!");
            wakePack(monster);
        }
        else if (monster.state == MonsterState::Wandering)
        {
            monster.target = origin;
        }
    }
}

// Tells the player what a monster does, if they can see it do it
void Game::announce(const Monster& monster, const std::string& what)
{
    if (level_.map.isVisible(monster.pos.x, monster.pos.y))
    {
        std::string name = monster.def->name;
        log_.add("The " + name + " " + what);
    }
}

// A monster strikes the player. If the blow kills, the player remembers
// what dealt it, for the death screen. A monster that drains heals by half
// the damage it does
void Game::monsterAttacks(Monster& monster)
{
    int damage = rollDamage(rng_, monster.def->minDamage,
                            monster.def->maxDamage, 0, player_.defense);
    player_.hp -= damage;
    if (monster.def->has(Trait::DRAINS))
    {
        monster.hp = SDL_min(monster.maxHp, monster.hp + damage / 2);
    }
    std::string name = monster.def->name;
    log_.add("The " + name + " hits you for " + std::to_string(damage) +
             ".");
    makeNoise(player_.pos, FIGHT_LOUDNESS);
    if (!player_.isAlive())
    {
        player_.killedBy = withArticle(name);
    }
    else if (monster.def->venom > 0)
    {
        // A venomous bite poisons, for at least as long as its venom
        // lasts. Only the first bite needs saying so
        if (player_.poisoned == 0)
        {
            log_.add("You are poisoned!");
        }
        player_.poisoned = SDL_max(player_.poisoned, monster.def->venom);
    }
}

// A monster with an attack from afar uses it if it's ready, the player is
// within its range, and its shot is clear. Then it has to wait its kind's
// turns to attack from afar again. Returns true if it attacked
bool Game::attacksFromAfar(Monster& monster)
{
    const RangedAttack& ranged = monster.def->ranged;
    if (ranged.kind == RangedKind::None || monster.cooldown > 0)
    {
        return false;
    }
    if (stepsBetween(monster.pos, player_.pos) > ranged.range)
    {
        return false;
    }
    if (!clearShot(monster.pos, player_.pos))
    {
        return false;
    }

    if (ranged.kind == RangedKind::Arrows)
    {
        monsterShoots(monster);
    }
    else if (ranged.kind == RangedKind::Lightning)
    {
        monsterCastsLightning(monster);
    }
    else
    {
        monsterBreathes(monster);
    }
    monster.cooldown = ranged.every;
    return true;
}

// True if a line from one cell reaches another with nothing in the way:
// nothing solid, and no other creature, since a monster doesn't shoot
// through another
bool Game::clearShot(Point from, Point to)
{
    for (Point cell : lineFrom(from, to, stepsBetween(from, to)))
    {
        if (cell == to)
        {
            return true;
        }
        bool blocked = !level_.map.isWalkable(cell.x, cell.y) ||
                       monsterAt(cell) != nullptr;
        if (blocked)
        {
            return false;
        }
    }
    return false;
}

// An arrow at the player: it hits with the chance a missile has at that
// distance, for its damage less the player's defense
void Game::monsterShoots(Monster& monster)
{
    const RangedAttack& ranged = monster.def->ranged;
    std::string name = monster.def->name;
    int chance = hitChance(stepsBetween(monster.pos, player_.pos));
    if (rng_.roll(1, 100) > chance)
    {
        log_.add("The " + name + "'s arrow misses you.");
        return;
    }
    int damage = rollDamage(rng_, ranged.minDamage, ranged.maxDamage, 0,
                            player_.defense);
    player_.hp -= damage;
    log_.add("The " + name + "'s arrow hits you for " +
             std::to_string(damage) + ".");
    makeNoise(player_.pos, FIGHT_LOUDNESS);
    if (!player_.isAlive())
    {
        player_.killedBy = withArticle(name) + "'s arrow";
    }
}

// A bolt of lightning at the player. It's magic, so armor doesn't help
void Game::monsterCastsLightning(Monster& monster)
{
    const RangedAttack& ranged = monster.def->ranged;
    std::string name = monster.def->name;
    int damage = rng_.roll(ranged.minDamage, ranged.maxDamage);
    player_.hp -= damage;
    log_.add("The " + name + " casts lightning at you, for " +
             std::to_string(damage) + "!");
    makeNoise(player_.pos, FIGHT_LOUDNESS);
    if (!player_.isAlive())
    {
        player_.killedBy = withArticle(name) + "'s lightning";
    }
}

// A burst of fire on the cells within BREATH_RADIUS steps of the player.
// Everyone in it is burned, armor or no armor: the player, and any other
// monster that's too close, but not the breather
void Game::monsterBreathes(Monster& monster)
{
    const RangedAttack& ranged = monster.def->ranged;
    std::string name = monster.def->name;
    log_.add("The " + name + " breathes fire!");
    for (Point cell : burstAt(player_.pos, BREATH_RADIUS))
    {
        Monster* other = monsterAt(cell);
        if (cell == player_.pos)
        {
            int damage = rng_.roll(ranged.minDamage, ranged.maxDamage);
            player_.hp -= damage;
            log_.add("The fire burns you for " + std::to_string(damage) +
                     "!");
            if (!player_.isAlive())
            {
                player_.killedBy = withArticle(name) + "'s fire";
            }
        }
        else if (other != nullptr && other != &monster && other->isAlive())
        {
            int damage = rng_.roll(ranged.minDamage, ranged.maxDamage);
            hurtMonster(*other, damage, "the fire");
        }
    }
    makeNoise(player_.pos, FIGHT_LOUDNESS);
}

// A monster that keeps its distance, with the player closer than KEEP_MIN
// steps, backs away from them, down the flee map, if it can. Returns true
// if it did
bool Game::backsAway(Monster& monster)
{
    if (stepsBetween(monster.pos, player_.pos) >= KEEP_MIN)
    {
        return false;
    }
    Point away = stepDown(monster, fromPlayer_);
    if (away == monster.pos)
    {
        return false;
    }
    moveMonster(monster, away);
    return true;
}

// A monster that keeps its distance, and can't attack from afar this turn,
// comes closer if it's more than KEEP_MAX steps from the player, and
// otherwise stays put until it can attack again. Cornered, with the player
// next to it, it fights
void Game::keepDistance(Monster& monster)
{
    if (stepsBetween(monster.pos, player_.pos) > KEEP_MAX)
    {
        moveMonster(monster, stepDown(monster, toPlayer_));
    }
    else if (isNextTo(monster.pos, player_.pos))
    {
        monsterAttacks(monster);
    }
}

// A step downhill on a map of distances: to the free neighbor with the
// lowest distance, if that's lower than where the monster stands, or
// nowhere. Diagonal steps count the same as straight ones, so several
// neighbors are often equally low. Of those, a straight step wins, so a
// monster walks in straight lines where it can, not in a zigzag
Point Game::stepDown(const Monster& monster, const DijkstraMap& distances)
{
    Point best = monster.pos;
    int bestDistance = distances.at(monster.pos);
    bool bestIsDiagonal = false;
    for (Point step : DIRECTIONS)
    {
        Point to = { monster.pos.x + step.x, monster.pos.y + step.y };
        if (!canEnter(monster, to))
        {
            continue;
        }
        int distance = distances.at(to);
        bool diagonal = step.x != 0 && step.y != 0;
        bool lower = distance < bestDistance;
        bool asLowButStraight = distance == bestDistance &&
                                bestIsDiagonal && !diagonal;
        if (lower || asLowButStraight)
        {
            best = to;
            bestDistance = distance;
            bestIsDiagonal = diagonal;
        }
    }
    return best;
}

// Where a monster that can't see the player steps: in a random direction,
// if the cell there is free, or nowhere
Point Game::wanderStep(const Monster& monster)
{
    Point step = DIRECTIONS[rng_.roll(0, 7)];
    Point to = { monster.pos.x + step.x, monster.pos.y + step.y };
    if (canEnter(monster, to))
    {
        return to;
    }
    return monster.pos;
}

// True if the monster can see the player. It can if the player can see its
// cell, since sight is symmetric, and the player is within its sight,
// measured as the field of view measures the player's
bool Game::seesPlayer(const Monster& monster) const
{
    int sight = monster.def->sight;
    return level_.map.isVisible(monster.pos.x, monster.pos.y) &&
           distanceSquared(monster.pos, player_.pos) <= sight * (sight + 1);
}

// True if a monster could step onto the cell: floor, with nobody on it
bool Game::isFree(Point cell)
{
    return level_.map.isWalkable(cell.x, cell.y) &&
           cell != player_.pos && monsterAt(cell) == nullptr;
}

// True if a monster could step onto the cell, or, being one that opens
// doors, open the closed door there
bool Game::canEnter(const Monster& monster, Point cell)
{
    bool door = level_.map.at(cell.x, cell.y) == Terrain::ClosedDoor;
    return isFree(cell) || (door && monster.def->has(Trait::OPENS_DOORS));
}

// Moves a monster onto a cell next to it. If that's a closed door, it
// opens the door instead, and that's its move
void Game::moveMonster(Monster& monster, Point to)
{
    if (level_.map.at(to.x, to.y) == Terrain::ClosedDoor)
    {
        openDoor(to);
        announce(monster, "opens a door.");
        return;
    }
    monster.pos = to;
}

// The trap on a cell, or nullptr if there's none
Trap* Game::trapAt(Point pos)
{
    for (Trap& trap : level_.traps)
    {
        if (trap.pos == pos)
        {
            return &trap;
        }
    }
    return nullptr;
}

// The monster standing on a cell, or nullptr if there's none
Monster* Game::monsterAt(Point pos)
{
    for (Monster& monster : level_.monsters)
    {
        if (monster.pos == pos)
        {
            return &monster;
        }
    }
    return nullptr;
}

void Game::draw() const
{
    setColor(Palette::BACKGROUND);
    SDL_RenderClear(renderer_);

    // The title screen has the window to itself
    if (state_ == GameState::Title)
    {
        drawTitle();
        SDL_RenderPresent(renderer_);
        return;
    }

    // Cells half in the map view are cut off at its edges, so nothing in
    // the map is ever drawn over the panels
    SDL_Rect mapView = { 0, 0, MAP_VIEW_W, MAP_VIEW_H };
    SDL_SetRenderClipRect(renderer_, &mapView);
    if (caveSteps_)
    {
        // The cave being made on F6 has the map view to itself
        drawCaveSteps();
    }
    else if (vaultShown_)
    {
        // And so does F7's vault
        drawVaultShown();
    }
    else
    {
        drawMap();
        if (showDistances_)
        {
            drawDistances();
        }
        drawTraps();
        drawItems();
        drawMonsters();
        view_->drawPlayer(player_.pos);
        if (state_ == GameState::Targeting)
        {
            drawLineOfFire();
        }
        if (showMinds_)
        {
            drawMinds();
        }
        std::optional<Point> target = lookTarget();
        if (target)
        {
            drawHighlight(*target);
        }
    }
    SDL_SetRenderClipRect(renderer_, nullptr);

    drawPanels();
    drawSidePanel();
    drawLookPanel();
    drawLog();
    if (state_ == GameState::Dead)
    {
        drawDeathScreen();
    }
    else if (state_ == GameState::Inventory)
    {
        drawInventory();
    }
    else if (state_ == GameState::Menu)
    {
        drawMenu();
    }
    SDL_RenderPresent(renderer_);
}

void Game::setColor(SDL_Color color) const
{
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

// The map as the player knows it: what they can see, lit, and what they
// remember. With revealMap_, a debugging aid, all the rest is drawn as if
// remembered too. Walls are drawn only where they border floor. The view
// decides how each cell looks
void Game::drawMap() const
{
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            bool visible = level_.map.isVisible(x, y);
            if (!visible && !level_.map.isExplored(x, y) && !revealMap_)
            {
                continue;
            }
            // The debugging aid shows secret doors for what they are
            Terrain terrain = level_.map.at(x, y);
            if (revealMap_ && terrain == Terrain::SecretDoor)
            {
                terrain = Terrain::ClosedDoor;
            }
            bool ground = level_.map.isWalkable(x, y);
            if (ground || level_.map.bordersFloor(x, y))
            {
                view_->drawTerrain({ x, y }, terrain, visible);
            }
        }
    }
}

// The F6 debugging aid: the cave being made, every cell of it, lit, and
// every wall too, so the random fill can be seen for what it is
void Game::drawCaveSteps() const
{
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            view_->drawTerrain({ x, y }, caveSteps_->map.at(x, y), true);
        }
    }
    if (caveSteps_->start)
    {
        view_->drawPlayer(*caveSteps_->start);
    }
}

// The F7 debugging aid: a vault's map, in the middle of the map view.
// The view draws its walls, floors and doors. Each other symbol is drawn
// as it is, in the debugging color, on a plain cell: a secret door's S on
// the walls' background
void Game::drawVaultShown() const
{
    const VaultDef& vault = data_.vaults()[*vaultShown_];
    Point corner = { (MAP_W - vault.width()) / 2,
                     (MAP_H - vault.height()) / 2 };
    for (int y = 0; y < vault.height(); ++y)
    {
        for (int x = 0; x < vault.width(); ++x)
        {
            char symbol = vault.rows[y][x];
            Point cell = { corner.x + x, corner.y + y };
            if (symbol == '#')
            {
                view_->drawTerrain(cell, Terrain::Wall, true);
            }
            else if (symbol == '.')
            {
                view_->drawTerrain(cell, Terrain::Floor, true);
            }
            else if (symbol == '+')
            {
                view_->drawTerrain(cell, Terrain::ClosedDoor, true);
            }
            else if (symbol != ' ')
            {
                SDL_FRect where = view_->camera().cellRect(cell);
                setColor(symbol == 'S' ? Palette::WALL_BACK
                                       : Palette::BACKGROUND);
                SDL_RenderFillRect(renderer_, &where);
                float left = where.x + (where.w - glyphs_.charWidth()) / 2;
                float top = where.y + (where.h - glyphs_.lineHeight()) / 2;
                glyphs_.drawText(std::string(1, symbol), left, top,
                                 Palette::DEBUG);
            }
        }
    }
}

// A debugging aid (F3): every cell's distance from the player, from
// toPlayer_, as its last digit, over a color that cools with distance.
// It's drawn for the whole map, seen or not
void Game::drawDistances() const
{
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            Point cell = { x, y };
            int distance = toPlayer_.at(cell);
            if (distance == DijkstraMap::UNREACHED ||
                !view_->camera().shows(cell))
            {
                continue;
            }
            float t = SDL_min(1.0f, distance / FAR_DISTANCE);
            SDL_FRect rect = view_->camera().cellRect(cell);
            setColor(blend(NEAR_BACK, FAR_BACK, t));
            SDL_RenderFillRect(renderer_, &rect);
            char digit = static_cast<char>('0' + distance % 10);
            glyphs_.drawInCell(digit, rect.x, rect.y, rect.w,
                               blend(NEAR_TEXT, FAR_TEXT, t));
        }
    }
}

// A debugging aid (F4): every monster's state, as an outline in the
// state's color, seen or not, and a line to where it's heading: the cell
// a wanderer chose, or where a hunter last saw the player
void Game::drawMinds() const
{
    for (const Monster& monster : level_.monsters)
    {
        SDL_FRect from = view_->camera().cellRect(monster.pos);
        setColor(stateColor(monster.state));
        SDL_RenderRect(renderer_, &from);

        std::optional<Point> goal;
        if (monster.state == MonsterState::Wandering)
        {
            goal = monster.target;
        }
        else if (monster.state == MonsterState::Hunting)
        {
            goal = monster.lastSeen;
        }
        if (goal)
        {
            SDL_FRect to = view_->camera().cellRect(*goal);
            SDL_RenderLine(renderer_, from.x + from.w / 2, from.y + from.h / 2,
                           to.x + to.w / 2, to.y + to.h / 2);
        }
    }
}

// An outline around a cell, in whichever view is in use: its camera knows
// where the cell is, and how big
void Game::drawHighlight(Point cell) const
{
    SDL_FRect rect = view_->camera().cellRect(cell);
    setColor(Palette::HIGHLIGHT);
    SDL_RenderRect(renderer_, &rect);
}

// The items on the floor that the player knows of: those in sight, lit,
// and those they've seen before, as they remember them. Items don't move,
// so a remembered one is still there. With revealMap_, all of them
void Game::drawItems() const
{
    for (const FloorItem& floorItem : level_.items)
    {
        Point cell = floorItem.pos;
        bool visible = level_.map.isVisible(cell.x, cell.y);
        if (visible || floorItem.seen || revealMap_)
        {
            view_->drawItem(cell, itemLook(floorItem.item), visible);
        }
    }
}

// While aiming: the cells the shot would pass through, in a see-through
// highlight, so the player sees where it would stop before firing. Only
// the cells in sight are shaded: past them, where the shading stopped
// would tell of a wall or a monster the player can't see
void Game::drawLineOfFire() const
{
    SDL_Color shade = Palette::HIGHLIGHT;
    shade.a = 70;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    setColor(shade);
    std::vector<Point> path = lineOfFire();
    bool allSeen = true;
    for (Point cell : path)
    {
        if (!level_.map.isVisible(cell.x, cell.y))
        {
            allSeen = false;
            break;
        }
        SDL_FRect rect = view_->camera().cellRect(cell);
        SDL_RenderFillRect(renderer_, &rect);
    }

    // A fireball's burst, around the end of its path, in a fainter shade,
    // if the whole path is in sight
    if (targetPurpose_ == TargetPurpose::Fireball && !path.empty() && allSeen)
    {
        shade.a = 40;
        setColor(shade);
        for (Point cell : burstAt(path.back(), FIREBALL_RADIUS))
        {
            if (level_.map.isVisible(cell.x, cell.y))
            {
                SDL_FRect rect = view_->camera().cellRect(cell);
                SDL_RenderFillRect(renderer_, &rect);
            }
        }
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

// The traps the player knows of, in sight or remembered. With revealMap_,
// a debugging aid, the hidden ones too
void Game::drawTraps() const
{
    for (const Trap& trap : level_.traps)
    {
        Point cell = trap.pos;
        bool visible = level_.map.isVisible(cell.x, cell.y);
        bool seen = visible || level_.map.isExplored(cell.x, cell.y);
        if ((trap.known && seen) || revealMap_)
        {
            view_->drawTrap(cell, trapDefOf(trap.kind).color, visible);
        }
    }
}

// The trap on a cell, if the player knows of it, or nullptr. With
// revealMap_, any trap
const Trap* Game::knownTrapAt(Point cell) const
{
    for (const Trap& trap : level_.traps)
    {
        if (trap.pos == cell && (trap.known || revealMap_))
        {
            return &trap;
        }
    }
    return nullptr;
}

// The monsters the player can see. With revealMap_, all of them
void Game::drawMonsters() const
{
    for (const Monster& monster : level_.monsters)
    {
        if (level_.map.isVisible(monster.pos.x, monster.pos.y) || revealMap_)
        {
            view_->drawMonster(monster.pos, *monster.def);
        }
    }
}

// The side panel and the message log, each with a line along the edge it
// shares with the map
void Game::drawPanels() const
{
    setColor(Palette::PANEL);
    SDL_FRect panel = { static_cast<float>(PANEL_X), 0.0f,
                        static_cast<float>(PANEL_W),
                        static_cast<float>(MAP_VIEW_H) };
    SDL_RenderFillRect(renderer_, &panel);
    SDL_FRect log = { 0.0f, static_cast<float>(LOG_Y),
                      static_cast<float>(SCREEN_W),
                      static_cast<float>(LOG_H) };
    SDL_RenderFillRect(renderer_, &log);

    setColor(Palette::FRAME);
    SDL_RenderLine(renderer_, static_cast<float>(PANEL_X), 0.0f,
                   static_cast<float>(PANEL_X),
                   static_cast<float>(MAP_VIEW_H));
    SDL_RenderLine(renderer_, 0.0f, static_cast<float>(LOG_Y),
                   static_cast<float>(SCREEN_W), static_cast<float>(LOG_Y));
}

// A bar that shows how much health is left: green while there's plenty,
// orange below half, and red below a quarter
void Game::drawHealthBar(float x, float y, float width,
                         const Entity& entity) const
{
    float share = static_cast<float>(entity.hp) / entity.maxHp;
    share = SDL_clamp(share, 0.0f, 1.0f);
    SDL_Color color = Palette::HEALTH_GOOD;
    if (share < 0.25f)
    {
        color = Palette::DANGER;
    }
    else if (share < 0.5f)
    {
        color = Palette::HEALTH_LOW;
    }

    SDL_FRect whole = { x, y, width, HEALTH_BAR_H };
    setColor(Palette::FRAME);
    SDL_RenderFillRect(renderer_, &whole);
    SDL_FRect left = { x, y, width * share, HEALTH_BAR_H };
    setColor(color);
    SDL_RenderFillRect(renderer_, &left);
}

// The side panel: the game's name, the player's health, then the run so
// far, one row under another, and a reminder when a debugging aid is on
void Game::drawSidePanel() const
{
    float x = PANEL_X + MARGIN;
    float y = MARGIN;
    float row = glyphs_.lineHeight();
    glyphs_.drawText("AMULETS OF SDL", x, y, Palette::TEXT);
    y += 2 * row;
    glyphs_.drawText("Floor " + std::to_string(level_.depth), x, y,
                     Palette::TEXT);
    y += row;
    glyphs_.drawText("Health " + std::to_string(player_.hp) + "/" +
                     std::to_string(player_.maxHp), x, y, Palette::TEXT);
    y += row;
    drawHealthBar(x, y, PANEL_W - 2 * MARGIN, player_);
    y += row;
    // The damage of a blow, with strength added
    int low = player_.minDamage() + player_.strength;
    int high = player_.maxDamage() + player_.strength;
    glyphs_.drawText("Damage " + std::to_string(low) + "-" +
                     std::to_string(high), x, y, Palette::TEXT);
    y += row;
    glyphs_.drawText("Defense " + std::to_string(player_.defense), x, y,
                     Palette::TEXT);
    y += row;

    // What the player has in hand and on their back, and their bow
    glyphs_.drawText(wieldingText(), x, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(wearingText(), x, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(bowText(), x, y, Palette::DIM_TEXT);
    y += row;

    // Statuses, with the turns each has left
    if (player_.poisoned > 0)
    {
        glyphs_.drawText("Poisoned (" + std::to_string(player_.poisoned) +
                         ")", x, y, Palette::POISONED);
        y += row;
    }
    if (player_.regenerating > 0)
    {
        glyphs_.drawText("Regenerating (" +
                         std::to_string(player_.regenerating) + ")", x, y,
                         Palette::REGENERATING);
        y += row;
    }
    if (player_.confused > 0)
    {
        glyphs_.drawText("Confused (" + std::to_string(player_.confused) +
                         ")", x, y, Palette::CONFUSED);
        y += row;
    }
    glyphs_.drawText("Gold " + std::to_string(player_.gold), x, y,
                     Palette::GOLD);
    y += row;
    glyphs_.drawText("Amulets " + std::to_string(player_.amulets) + " of " +
                     std::to_string(AMULETS), x, y, Palette::GOLD);
    y += row;
    glyphs_.drawText("Turn " + std::to_string(turn_), x, y, Palette::TEXT);
    y += row;
    glyphs_.drawText("Seed " + std::to_string(seed_), x, y,
                     Palette::DIM_TEXT);
    y += row;

    // Each debugging aid that's on, a row each
    if (revealMap_)
    {
        y += row;
        glyphs_.drawText("DEBUG: map revealed (F1)", x, y, Palette::DEBUG);
    }
    if (showDistances_)
    {
        y += row;
        glyphs_.drawText("DEBUG: distances (F3)", x, y, Palette::DEBUG);
    }
    if (caveSteps_)
    {
        std::string step = "the random fill";
        if (caveSteps_->step > CaveGen::SMOOTHING_PASSES)
        {
            step = "the biggest cave: " + std::to_string(caveSteps_->cells) +
                   " cells";
        }
        else if (caveSteps_->step > 0)
        {
            step = "smoothing pass " + std::to_string(caveSteps_->step) +
                   " of " + std::to_string(CaveGen::SMOOTHING_PASSES);
        }
        y += row;
        glyphs_.drawText("DEBUG: a cave (F6)", x, y, Palette::DEBUG);
        y += row;
        glyphs_.drawText(step, x, y, Palette::DEBUG);
        bool tooSmall = caveSteps_->step > CaveGen::SMOOTHING_PASSES &&
                        caveSteps_->cells < CaveGen::MIN_CAVE_CELLS;
        if (tooSmall)
        {
            y += row;
            glyphs_.drawText("too few, so it's made again", x, y,
                             Palette::DEBUG);
        }
    }
    if (vaultShown_)
    {
        const VaultDef& vault = data_.vaults()[*vaultShown_];
        int count = static_cast<int>(data_.vaults().size());
        y += row;
        glyphs_.drawText("DEBUG: vault " + std::to_string(*vaultShown_ + 1) +
                         " of " + std::to_string(count) + " (F7)", x, y,
                         Palette::DEBUG);
        y += row;
        glyphs_.drawText(vault.id + ", floors " +
                         std::to_string(vault.minFloor) + " to " +
                         std::to_string(vault.maxFloor), x, y,
                         Palette::DEBUG);
    }
    if (showMinds_)
    {
        // The note, and a key to the overlay's colors
        float word = glyphs_.charWidth();
        y += row;
        glyphs_.drawText("DEBUG: minds (F4)", x, y, Palette::DEBUG);
        y += row;
        glyphs_.drawText("asleep", x, y, ASLEEP_COLOR);
        glyphs_.drawText("wandering", x + 7 * word, y, WANDERING_COLOR);
        y += row;
        glyphs_.drawText("hunting", x, y, HUNTING_COLOR);
        glyphs_.drawText("fleeing", x + 8 * word, y, FLEEING_COLOR);

        // Then every monster that fits above the look panel, while it's
        // showing, or above the side panel's bottom: its letter, its cell,
        // and its state
        y += row;
        float bottom = MAP_VIEW_H;
        if (lookTarget())
        {
            bottom -= LOOK_PANEL_LINES * row + 2 * MARGIN;
        }
        float lastY = bottom - 2 * row;
        int shown = 0;
        for (const Monster& monster : level_.monsters)
        {
            if (y + row > lastY)
            {
                break;
            }
            y += row;
            std::string line = std::string(1, monster.def->glyph) + " " +
                               std::to_string(monster.pos.x) + "," +
                               std::to_string(monster.pos.y) + " " +
                               stateName(monster.state);
            if (monster.searchTurns > 0)
            {
                line += ", searching " + std::to_string(monster.searchTurns);
            }
            glyphs_.drawText(line, x, y, stateColor(monster.state));
            ++shown;
        }
        int hidden = static_cast<int>(level_.monsters.size()) - shown;
        if (hidden > 0)
        {
            y += row;
            glyphs_.drawText("and " + std::to_string(hidden) + " more", x, y,
                             Palette::DEBUG);
        }
    }
}

// The monster on a cell, if the player can see it there (or the map is
// revealed). The look panel must never tell the player more than they know
const Monster* Game::visibleMonsterAt(Point cell) const
{
    if (!level_.map.isVisible(cell.x, cell.y) && !revealMap_)
    {
        return nullptr;
    }
    for (const Monster& monster : level_.monsters)
    {
        if (monster.pos == cell)
        {
            return &monster;
        }
    }
    return nullptr;
}

// Text in the look panel, broken between words onto as many lines as it
// takes to fit inside the panel's margins. Returns the y of the next line
float Game::drawWrapped(const std::string& text, float y,
                        SDL_Color color) const
{
    float x = PANEL_X + MARGIN;
    int width = static_cast<int>((PANEL_W - 2 * MARGIN) /
                                 glyphs_.charWidth());
    for (const std::string& line : wrapped(text, width))
    {
        glyphs_.drawText(line, x, y, color);
        y += glyphs_.lineHeight();
    }
    return y;
}

// The look panel, at the bottom of the side panel: what's on the cell
// under the mouse, as far as the player knows. A creature comes first,
// with its health, then the ground
void Game::drawLookPanel() const
{
    std::optional<Point> cell = lookTarget();
    if (!cell)
    {
        return;
    }

    float x = PANEL_X + MARGIN;
    float row = glyphs_.lineHeight();
    float top = MAP_VIEW_H - (LOOK_PANEL_LINES * row + 2 * MARGIN);
    setColor(Palette::FRAME);
    SDL_RenderLine(renderer_, static_cast<float>(PANEL_X), top,
                   static_cast<float>(SCREEN_W), top);

    float y = top + MARGIN;
    if (state_ == GameState::Look)
    {
        y = drawWrapped("Looking: x or Esc to stop", y, Palette::HIGHLIGHT);
    }
    else if (state_ == GameState::Targeting)
    {
        y = drawWrapped("Aim: Enter fires, Esc stops", y, Palette::HIGHLIGHT);
    }

    bool known = level_.map.isExplored(cell->x, cell->y) || revealMap_;
    if (!known)
    {
        drawWrapped("Unexplored.", y, Palette::DIM_TEXT);
        return;
    }

    const Monster* monster = visibleMonsterAt(*cell);
    if (*cell == player_.pos)
    {
        y = drawWrapped("You.", y, Palette::TEXT);
    }
    else if (monster != nullptr)
    {
        // Its name, how hurt it looks, and anything else that's plain to
        // see
        std::string text = capitalized(withArticle(monster->def->name)) +
                           ", " + woundsOf(*monster);
        if (monster->state == MonsterState::Asleep)
        {
            text += ", asleep";
        }
        if (monster->afraid > 0)
        {
            text += ", afraid";
        }
        if (monster->poisoned > 0)
        {
            text += ", poisoned";
        }
        y = drawWrapped(text + ".", y, Palette::TEXT);
        drawHealthBar(x, y, PANEL_W - 2 * MARGIN, *monster);
        y += row;
    }

    // An item on the cell, if the player has seen it there
    const FloorItem* item = floorItemAt(*cell);
    bool itemKnown = item != nullptr && (item->seen || revealMap_);
    if (itemKnown)
    {
        y = drawWrapped(capitalized(itemName(item->item)) + ".", y,
                        Palette::TEXT);
    }

    // The ground: what it is, and whether it's in sight or remembered
    std::string ground = "A wall";
    Terrain terrain = level_.map.at(cell->x, cell->y);
    if (terrain == Terrain::Floor)
    {
        ground = "Floor";
    }
    else if (terrain == Terrain::CaveFloor)
    {
        ground = "The cave floor";
    }
    else if (terrain == Terrain::StairsDown)
    {
        ground = "Stairs down";
    }
    else if (terrain == Terrain::ClosedDoor)
    {
        ground = "A closed door";
    }
    else if (terrain == Terrain::OpenDoor)
    {
        ground = "An open door";
    }
    const Trap* trap = knownTrapAt(*cell);
    if (trap != nullptr)
    {
        ground = capitalized(withArticle(trapDefOf(trap->kind).name));
    }
    if (!level_.map.isVisible(cell->x, cell->y))
    {
        ground += ", as you remember it";
    }
    drawWrapped(ground + ".", y, Palette::DIM_TEXT);
}

// The last LOG_LINES messages, oldest at the top. The ones that came since
// the player's last command are bright, and the rest are dim
void Game::drawLog() const
{
    const std::vector<MessageLog::Entry>& entries = log_.entries();
    int count = static_cast<int>(entries.size());
    int first = SDL_max(0, count - LOG_LINES);
    float y = LOG_Y + (LOG_H - LOG_LINES * LOG_LINE_H) / 2.0f;
    for (int i = first; i < count; ++i)
    {
        const MessageLog::Entry& entry = entries[i];
        SDL_Color color = entry.isNew ? Palette::TEXT : Palette::DIM_TEXT;
        glyphs_.drawText(entry.withCount(), MARGIN, y, color);
        y += LOG_LINE_H;
    }
}

// A line of text, centered on centerX
void Game::drawCentered(const std::string& text, float centerX, float y,
                        SDL_Color color) const
{
    float width = text.size() * glyphs_.charWidth();
    glyphs_.drawText(text, centerX - width / 2.0f, y, color);
}

// A dialog over the map: the dungeon fades, and a box of the given size
// sits in the middle of it. Returns the box, for the text to go in
SDL_FRect Game::drawDialog(float width, float height) const
{
    // The shade is see-through, so it's blended with what's under it
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    setColor(Palette::SHADE);
    SDL_FRect mapView = { 0.0f, 0.0f, static_cast<float>(MAP_VIEW_W),
                          static_cast<float>(MAP_VIEW_H) };
    SDL_RenderFillRect(renderer_, &mapView);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

    SDL_FRect box = { (MAP_VIEW_W - width) / 2.0f,
                      (MAP_VIEW_H - height) / 2.0f, width, height };
    setColor(Palette::PANEL);
    SDL_RenderFillRect(renderer_, &box);
    setColor(Palette::FRAME);
    SDL_RenderRect(renderer_, &box);
    return box;
}

// The death screen: a dialog that says what killed the player, and when
void Game::drawDeathScreen() const
{
    float centerX = MAP_VIEW_W / 2.0f;
    float row = glyphs_.lineHeight();
    SDL_FRect box = drawDialog(600.0f, 6 * row);

    float y = box.y + row;
    drawCentered("YOU HAVE DIED", centerX, y, Palette::DANGER);
    y += 2 * row;
    drawCentered("Killed by " + player_.killedBy + ", on turn " +
                 std::to_string(turn_) + ".", centerX, y, Palette::TEXT);
    y += row;
    drawCentered("Press R to play again, or Esc to quit.", centerX, y,
                 Palette::DIM_TEXT);
}

// The title screen: the game's name, and the keys. If the saved run
// couldn't be loaded, what became of it, and the first few reasons why
void Game::drawTitle() const
{
    float centerX = SCREEN_W / 2.0f;
    float row = glyphs_.lineHeight();
    float y = SCREEN_H / 3.0f;
    drawCentered("AMULETS OF SDL", centerX, y, Palette::PLAYER);
    y += 3 * row;

    std::vector<std::string> keys;
    if (loadErrors_.empty())
    {
        drawCentered("A saved run is waiting.", centerX, y, Palette::TEXT);
        keys = { "C    Continue it",
                 "N    Start a new run, and lose the saved one",
                 "Esc  Quit, and keep the saved run for later" };
    }
    else
    {
        std::string what = "The saved run is damaged, so it's been set "
                           "aside as save.bad:";
        if (!saveSetAside_)
        {
            what = "The saved run is damaged, and couldn't be set aside:";
        }
        keys = { "N    Start a new run", "Esc  Quit" };
        if (saveIsOtherVersion_)
        {
            what = "The saved run is from another version of the game:";
            keys = { "N    Start a new run, and lose the saved one",
                     "Esc  Quit, and keep the saved run" };
        }
        drawCentered(what, centerX, y, Palette::DANGER);
        int shown = SDL_min(static_cast<int>(loadErrors_.size()),
                            MOST_LOAD_ERRORS);
        for (int i = 0; i < shown; ++i)
        {
            y += row;
            drawCentered(loadErrors_[i], centerX, y, Palette::DIM_TEXT);
        }
        int more = static_cast<int>(loadErrors_.size()) - shown;
        if (more > 0)
        {
            y += row;
            drawCentered("and " + std::to_string(more) + " more",
                         centerX, y, Palette::DIM_TEXT);
        }
    }
    y += 3 * row;

    // The keys line up on the left, as a block in the middle
    size_t longest = 0;
    for (const std::string& line : keys)
    {
        longest = SDL_max(longest, line.size());
    }
    float x = centerX - longest * glyphs_.charWidth() / 2.0f;
    for (const std::string& line : keys)
    {
        glyphs_.drawText(line, x, y, Palette::TEXT);
        y += row;
    }
}

// Esc's menu, over the dungeon
void Game::drawMenu() const
{
    float row = glyphs_.lineHeight();
    SDL_FRect box = drawDialog(480.0f, 6 * row);
    float x = box.x + 2 * MARGIN;
    float y = box.y + row;
    glyphs_.drawText("MENU", x, y, Palette::TEXT);
    y += 2 * row;
    glyphs_.drawText("S    Save the run, and quit", x, y, Palette::TEXT);
    y += row;
    glyphs_.drawText("Esc  Back to the game", x, y, Palette::TEXT);
}

// What the player is wielding and wearing, in words, for the panel and the
// inventory, and their bow
std::string Game::wieldingText() const
{
    if (player_.weapon)
    {
        return "Wielding " + itemName(*player_.weapon);
    }
    return "Fighting with bare fists";
}

std::string Game::wearingText() const
{
    if (player_.armor)
    {
        return "Wearing " + itemName(*player_.armor);
    }
    return "Wearing no armor";
}

// The bow the player has ready, and what's in the quiver
std::string Game::bowText() const
{
    if (!player_.bow)
    {
        return "No bow ready";
    }
    std::string text = capitalized(itemName(*player_.bow));
    if (player_.quiver)
    {
        return text + ", " + itemName(*player_.quiver);
    }
    return text + ", no arrows";
}

// The inventory: a dialog with a line for each stack the player carries,
// lettered from a, room for all 20 slots
void Game::drawInventory() const
{
    float row = glyphs_.lineHeight();
    SDL_FRect box = drawDialog(560.0f, (Inventory::SLOTS + 9) * row);
    float x = box.x + 2 * MARGIN;
    float y = box.y + row;
    // The title and the hint say what choosing an item will do
    std::string title = "INVENTORY";
    std::string hint = "Press an item's letter to use it.";
    if (inventoryAction_ == InventoryAction::Drop)
    {
        title = "DROP WHICH?";
        hint = "Press an item's letter to drop it.";
    }
    else if (inventoryAction_ == InventoryAction::Identify)
    {
        title = "IDENTIFY WHICH?";
        hint = "Press the letter of something you don't know.";
    }
    else if (inventoryAction_ == InventoryAction::Throw)
    {
        title = "THROW WHICH?";
        hint = "Press the letter of a dagger, or darts.";
    }
    glyphs_.drawText(title, x, y, Palette::TEXT);
    float hintX = box.x + box.w - 2 * MARGIN - 12 * glyphs_.charWidth();
    glyphs_.drawText("Esc to close", hintX, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(hint, x, y, Palette::DIM_TEXT);
    y += 2 * row;

    // The equipment first, which isn't in the pack, then the pack
    glyphs_.drawText(wieldingText(), x, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(wearingText(), x, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(bowText(), x, y, Palette::DIM_TEXT);
    y += 2 * row;

    const std::vector<Item>& items = player_.inventory.items();
    if (items.empty())
    {
        glyphs_.drawText("Your pack is empty.", x, y, Palette::DIM_TEXT);
        return;
    }
    for (int slot = 0; slot < static_cast<int>(items.size()); ++slot)
    {
        std::string line = std::string(1, Inventory::letterFor(slot)) + ") " +
                           itemName(items[slot]);

        // Choosing what to identify, the things already known are dim,
        // and choosing what to throw, the things not made for it
        bool known = identification_.isKnown(*items[slot].def);
        bool dim = (inventoryAction_ == InventoryAction::Identify && known) ||
                   (inventoryAction_ == InventoryAction::Throw &&
                    !items[slot].def->thrown);
        glyphs_.drawText(line, x, y, dim ? Palette::DIM_TEXT : Palette::TEXT);
        y += row;
    }
}
