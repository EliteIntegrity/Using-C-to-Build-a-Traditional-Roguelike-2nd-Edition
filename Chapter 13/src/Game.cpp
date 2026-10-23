#include "Game.h"
#include <iterator>   // std::size, to count the kinds of monster
#include <random>     // std::random_device, for a random seed
#include <sstream>    // std::istringstream, to break text into words
#include "Fov.h"
#include "RoomGen.h"

namespace
{
    // How far the player can see, in cells
    constexpr int SIGHT_RADIUS = 9;

    // How many monsters a new dungeon starts with
    constexpr int MONSTER_COUNT = 8;

    // How many items a new dungeon starts with
    constexpr int ITEM_COUNT = 6;

    // The player's health at the start of a run
    constexpr int PLAYER_HP = 30;

    // The player heals 1 health every HEAL_EVERY turns, while hurt
    constexpr int HEAL_EVERY = 10;

    // How much health a potion of healing gives back (*tune*)
    constexpr int HEAL_AMOUNT = 15;

    // How tall the health bar in the side panel is, in pixels
    constexpr float HEALTH_BAR_H = 8.0f;

    // The look panel, at the bottom of the side panel, has room for this
    // many lines of text
    constexpr int LOOK_PANEL_LINES = 7;

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

    // The tiles' picture and index. Like the font's, the paths are
    // relative to the working directory
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

    // True if two cells touch, side by side or corner to corner
    bool isNextTo(Point a, Point b)
    {
        return distanceSquared(a, b) <= 2;
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

// A new run: the dungeon, its monsters, the player in the middle of the
// first room, and what the player can see from there
Game::Game(SDL_Renderer* renderer, GlyphCache& glyphs, unsigned int seed,
           bool useTiles)
    : renderer_(renderer), glyphs_(glyphs), glyphView_(renderer, glyphs),
      tileView_(renderer, TILES_PATH, TILES_INDEX_PATH),
      view_(&glyphView_), seed_(chooseSeed(seed)), rng_(seed_)
{
    std::vector<SDL_Rect> rooms = RoomGen::generate(map_, rng_);
    player_.pos = RoomGen::centerOf(rooms.front());
    placeMonsters(rooms);
    placeItems(rooms);
    identification_.randomize(rng_);

    // The player starts with a short sword in hand
    player_.weapon = Item{ findItemDef("short sword") };
    player_.hp = PLAYER_HP;
    player_.maxHp = PLAYER_HP;
    updateSight();
    toPlayer_.compute(map_, { player_.pos });

    log_.add("Welcome to the dungeon.");
    log_.add("Move with the arrow keys, the numpad, or h j k l y u b n.");

    if (useTiles && tileView_.isLoaded())
    {
        view_ = &tileView_;
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
            // The view's camera keeps the look cursor in sight while
            // looking, and the player the rest of the time
            Point focus = player_.pos;
            if (state_ == GameState::Look)
            {
                focus = lookCursor_;
            }
            view_->camera().follow(focus);
            draw();
            redraw_ = false;
        }
    }
    return playAgain_;
}

// Puts MONSTER_COUNT monsters of random kinds on the floor of the rooms,
// one to a cell. The first room is the player's, so it's left empty
void Game::placeMonsters(const std::vector<SDL_Rect>& rooms)
{
    int lastRoom = static_cast<int>(rooms.size()) - 1;
    int lastKind = static_cast<int>(std::size(MONSTER_DEFS)) - 1;
    if (lastRoom < 1)
    {
        return;
    }

    for (int i = 0; i < MONSTER_COUNT; ++i)
    {
        // A few tries each: a cell that's taken just means another roll
        for (int tries = 0; tries < 20; ++tries)
        {
            const SDL_Rect& room = rooms[rng_.roll(1, lastRoom)];
            Point pos = { room.x + rng_.roll(0, room.w - 1),
                          room.y + rng_.roll(0, room.h - 1) };
            if (monsterAt(pos) == nullptr)
            {
                Monster monster;
                monster.pos = pos;
                monster.def = &MONSTER_DEFS[rng_.roll(0, lastKind)];
                monster.hp = monster.def->hp;
                monster.maxHp = monster.def->hp;
                monster.defense = monster.def->defense;

                // Most start asleep, and the rest wandering
                bool awake = monster.def->has(Trait::NEVER_SLEEPS) ||
                             rng_.roll(1, AWAKE_ONE_IN) == 1;
                monster.state = awake ? MonsterState::Wandering
                                      : MonsterState::Asleep;
                monsters_.push_back(monster);
                break;
            }
        }
    }
}

// Puts ITEM_COUNT items of random kinds on the floor of the rooms, any of
// them, one to a cell, but never on the player
void Game::placeItems(const std::vector<SDL_Rect>& rooms)
{
    int lastRoom = static_cast<int>(rooms.size()) - 1;
    int lastKind = static_cast<int>(std::size(ITEM_DEFS)) - 1;
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            const SDL_Rect& room = rooms[rng_.roll(0, lastRoom)];
            Point pos = { room.x + rng_.roll(0, room.w - 1),
                          room.y + rng_.roll(0, room.h - 1) };
            if (pos != player_.pos && floorItemAt(pos) == nullptr)
            {
                FloorItem floorItem;
                floorItem.pos = pos;
                floorItem.item.def = &ITEM_DEFS[rng_.roll(0, lastKind)];
                floorItems_.push_back(floorItem);
                break;
            }
        }
    }
}

// The item lying on a cell, or nullptr if there's none
const FloorItem* Game::floorItemAt(Point pos) const
{
    for (const FloorItem& floorItem : floorItems_)
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
    SDL_Color color = identification_.appearanceOf(def).color;
    char glyph = def.itemClass == ItemClass::Potion ? '!' : '?';
    return { glyph, color, def.tile, true };
}

void Game::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        running_ = false;
    }
    else if (event.type == SDL_EVENT_KEY_DOWN)
    {
        handleKey(event.key);
    }
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        trackMouse(event);
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
    if (!map_.inBounds(cell.x, cell.y))
    {
        return std::nullopt;
    }
    return cell;
}

// The cell the look panel describes: the look cursor's while looking, and
// otherwise the one under the mouse, if any
std::optional<Point> Game::lookTarget() const
{
    if (state_ == GameState::Look)
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

    // A key held down repeats, and its repeats count only in the state it
    // was pressed in. A key that changes the state has done its job, and
    // its repeats would go on to do something else in the new one
    if (!key.repeat)
    {
        keyState_ = state_;
    }
    else if (state_ != keyState_)
    {
        return;
    }

    // The keys above work in every state. What the others do depends on
    // the state the game is in
    switch (state_)
    {
    case GameState::Playing:
        handlePlayingKey(key);
        break;
    case GameState::Look:
        handleLookKey(key);
        break;
    case GameState::Inventory:
        handleInventoryKey(key);
        break;
    case GameState::Dead:
        handleDeadKey(key);
        break;
    }
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

// Esc quits, g picks up, i shows the inventory, d drops, x starts looking
// around, and the moving keys move the player
void Game::handlePlayingKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_ESCAPE)
    {
        running_ = false;
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
        if (map_.inBounds(to.x, to.y))
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

// Stepping onto a monster attacks it. An attack or a step takes a turn, and
// then the rest of the turn plays out. Bumping into a wall takes no turn
void Game::playerStep(Point step)
{
    log_.markRead();
    Point to = { player_.pos.x + step.x, player_.pos.y + step.y };
    Monster* monster = monsterAt(to);
    if (monster != nullptr)
    {
        playerAttacks(*monster);
        removeDeadMonsters();
        finishTurn();
    }
    else if (map_.at(to.x, to.y) == Terrain::Floor)
    {
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
        finishTurn();
    }
    else
    {
        log_.add("You bump into the wall.");
    }
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
    bool unaware = monster.state == MonsterState::Asleep ||
                   monster.state == MonsterState::Wandering;
    if (unaware)
    {
        monster.state = MonsterState::Hunting;
        monster.lastSeen = player_.pos;
    }
}

// g picks up the item the player stands on, if the pack has room for it.
// That takes a turn; finding nothing there, or no room, doesn't
void Game::pickUp()
{
    log_.markRead();
    const FloorItem* here = floorItemAt(player_.pos);
    if (here == nullptr)
    {
        log_.add("There's nothing here to pick up.");
        return;
    }
    if (!player_.inventory.add(here->item))
    {
        log_.add("Your pack is full.");
        return;
    }

    log_.add("You pick up " + itemName(here->item) + ".");
    std::erase_if(floorItems_, [this](const FloorItem& floorItem)
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
    state_ = GameState::Inventory;
    inventoryAction_ = action;
    redraw_ = true;
}

// Uses an item from the pack, which takes a turn. A weapon is wielded, and
// armor worn. A potion is drunk, or a scroll read: its kind is known from
// then on, so the player learns what it was, and then sees what it does
void Game::useItem(int slot)
{
    ItemClass itemClass = player_.inventory.items()[slot].def->itemClass;
    if (itemClass == ItemClass::Weapon)
    {
        wield(slot);
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
    finishTurn();
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
        for (FloorItem& floorItem : floorItems_)
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
        floorItems_.push_back(dropped);
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
                if (map_.at(x, y) == Terrain::Floor || map_.bordersFloor(x, y))
                {
                    map_.setExplored(x, y);
                }
            }
        }
        for (FloorItem& onFloor : floorItems_)
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
    }
}

// Works out what the player can see from where they stand, and notes
// every item that comes into sight: from then on, they know it's there
void Game::updateSight()
{
    Fov::compute(map_, player_.pos, SIGHT_RADIUS);
    for (FloorItem& floorItem : floorItems_)
    {
        if (map_.isVisible(floorItem.pos.x, floorItem.pos.y))
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
    std::erase_if(monsters_, [](const Monster& monster)
    {
        return !monster.isAlive();
    });
}

// The rest of a turn, once the player has acted: the monsters act, the
// player heals a little now and then, and last of all comes the one check
// for the player's death
void Game::finishTurn()
{
    // The player may have moved, so the distances to them are new, and so
    // is the way to run from them
    toPlayer_.compute(map_, { player_.pos });
    fromPlayer_ = toPlayer_.fleeMap(map_);
    monstersAct();
    ++turn_;

    bool healing = player_.isAlive() && player_.hp < player_.maxHp;
    if (healing && turn_ % HEAL_EVERY == 0)
    {
        ++player_.hp;
    }

    if (!player_.isAlive())
    {
        log_.add("You die...");
        state_ = GameState::Dead;
    }
}

// The monsters' turn. What each one does depends on its state. Once the
// player is dead, no monster acts
void Game::monstersAct()
{
    for (Monster& monster : monsters_)
    {
        if (!player_.isAlive())
        {
            return;
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

// A hunting monster goes after the player, unless it's badly wounded, when
// it flees instead. Seeing them, it attacks if it's next to them, and
// otherwise comes closer by the shortest way, remembering where they are.
// Having lost them, it goes to where it last saw them, and if they're not
// there, it searches nearby for a while, then gives up
void Game::actHunting(Monster& monster)
{
    if (isBadlyWounded(monster))
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
        if (isNextTo(monster.pos, player_.pos))
        {
            monsterAttacks(monster);
        }
        else if (!movesErratically(monster))
        {
            monster.pos = stepDown(monster, toPlayer_);
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
            monster.pos = next;
        }
        return;
    }
    giveUp(monster);
}

// A fleeing monster runs from the player, down the flee map, while it can
// see them, and lies low while it can't. Cornered, with nowhere lower to
// go, it turns and fights. Healed past two thirds of its health, it hunts
// again
void Game::actFleeing(Monster& monster)
{
    if (monster.hp * 3 > monster.maxHp * 2)
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
        monster.pos = next;
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
    toGoal.compute(map_, { goal });
    Point next = stepDown(monster, toGoal);
    bool moved = next != monster.pos;
    monster.pos = next;
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
    monster.pos = wanderStep(monster);
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
        if (map_.at(cell.x, cell.y) == Terrain::Floor)
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
    spread.compute(map_, { origin });
    for (Monster& monster : monsters_)
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
    if (map_.isVisible(monster.pos.x, monster.pos.y))
    {
        std::string name = monster.def->name;
        log_.add("The " + name + " " + what);
    }
}

// A monster strikes the player. If the blow kills, the player remembers
// what dealt it, for the death screen
void Game::monsterAttacks(const Monster& monster)
{
    int damage = rollDamage(rng_, monster.def->minDamage,
                            monster.def->maxDamage, 0, player_.defense);
    player_.hp -= damage;
    std::string name = monster.def->name;
    log_.add("The " + name + " hits you for " + std::to_string(damage) +
             ".");
    makeNoise(player_.pos, FIGHT_LOUDNESS);
    if (!player_.isAlive())
    {
        player_.killedBy = withArticle(name);
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
        if (!isFree(to))
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
    if (isFree(to))
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
    return map_.isVisible(monster.pos.x, monster.pos.y) &&
           distanceSquared(monster.pos, player_.pos) <= sight * (sight + 1);
}

// True if a monster could step onto the cell: floor, with nobody on it
bool Game::isFree(Point cell)
{
    return map_.at(cell.x, cell.y) == Terrain::Floor &&
           cell != player_.pos && monsterAt(cell) == nullptr;
}

// The monster standing on a cell, or nullptr if there's none
Monster* Game::monsterAt(Point pos)
{
    for (Monster& monster : monsters_)
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

    // Cells half in the map view are cut off at its edges, so nothing in
    // the map is ever drawn over the panels
    SDL_Rect mapView = { 0, 0, MAP_VIEW_W, MAP_VIEW_H };
    SDL_SetRenderClipRect(renderer_, &mapView);
    drawMap();
    if (showDistances_)
    {
        drawDistances();
    }
    drawItems();
    drawMonsters();
    view_->drawPlayer(player_.pos);
    if (showMinds_)
    {
        drawMinds();
    }
    std::optional<Point> target = lookTarget();
    if (target)
    {
        drawHighlight(*target);
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
            bool visible = map_.isVisible(x, y);
            if (!visible && !map_.isExplored(x, y) && !revealMap_)
            {
                continue;
            }
            Terrain terrain = map_.at(x, y);
            if (terrain == Terrain::Floor || map_.bordersFloor(x, y))
            {
                view_->drawTerrain({ x, y }, terrain, visible);
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
    for (const Monster& monster : monsters_)
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
    for (const FloorItem& floorItem : floorItems_)
    {
        Point cell = floorItem.pos;
        bool visible = map_.isVisible(cell.x, cell.y);
        if (visible || floorItem.seen || revealMap_)
        {
            view_->drawItem(cell, itemLook(floorItem.item), visible);
        }
    }
}

// The monsters the player can see. With revealMap_, all of them
void Game::drawMonsters() const
{
    for (const Monster& monster : monsters_)
    {
        if (map_.isVisible(monster.pos.x, monster.pos.y) || revealMap_)
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

    // What the player has in hand and on their back
    glyphs_.drawText(wieldingText(), x, y, Palette::DIM_TEXT);
    y += row;
    glyphs_.drawText(wearingText(), x, y, Palette::DIM_TEXT);
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

        // Then every monster: its letter, its cell, and its state
        y += row;
        for (const Monster& monster : monsters_)
        {
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
        }
    }
}

// The monster on a cell, if the player can see it there (or the map is
// revealed). The look panel must never tell the player more than they know
const Monster* Game::visibleMonsterAt(Point cell) const
{
    if (!map_.isVisible(cell.x, cell.y) && !revealMap_)
    {
        return nullptr;
    }
    for (const Monster& monster : monsters_)
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

    bool known = map_.isExplored(cell->x, cell->y) || revealMap_;
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
        std::string name = withArticle(monster->def->name);
        y = drawWrapped(capitalized(name) + ", " + woundsOf(*monster) + ".",
                        y, Palette::TEXT);
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
    bool floor = map_.at(cell->x, cell->y) == Terrain::Floor;
    std::string ground = floor ? "Floor" : "A wall";
    if (!map_.isVisible(cell->x, cell->y))
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

// What the player is wielding and wearing, in words, for the panel and the
// inventory
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

// The inventory: a dialog with a line for each stack the player carries,
// lettered from a, room for all 20 slots
void Game::drawInventory() const
{
    float row = glyphs_.lineHeight();
    SDL_FRect box = drawDialog(560.0f, (Inventory::SLOTS + 8) * row);
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

        // Choosing what to identify, the things already known are dim
        bool known = identification_.isKnown(*items[slot].def);
        bool dim = inventoryAction_ == InventoryAction::Identify && known;
        glyphs_.drawText(line, x, y, dim ? Palette::DIM_TEXT : Palette::TEXT);
        y += row;
    }
}
