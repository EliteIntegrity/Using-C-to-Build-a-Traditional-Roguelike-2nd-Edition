#include "Game.h"
#include <iterator>   // std::size, to count the kinds of monster
#include <random>     // std::random_device, for a random seed
#include "Fov.h"
#include "RoomGen.h"

namespace
{
    // How far the player can see, in cells
    constexpr int SIGHT_RADIUS = 9;

    // How many monsters a new dungeon starts with
    constexpr int MONSTER_COUNT = 8;

    // The player's health at the start of a run
    constexpr int PLAYER_HP = 30;

    // The player heals 1 health every HEAL_EVERY turns, while hurt
    constexpr int HEAL_EVERY = 10;

    // How tall the health bar in the side panel is, in pixels
    constexpr float HEALTH_BAR_H = 8.0f;

    // The tiles' picture and index. Like the font's, the paths are
    // relative to the working directory
    const char* const TILES_PATH = "assets/tiles.png";
    const char* const TILES_INDEX_PATH = "assets/tiles.txt";

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

    // The damage one blow does: a roll from minDamage to maxDamage, less
    // the defender's defense, but never less than 1, so every hit hurts
    int rollDamage(Rng& rng, int minDamage, int maxDamage, int defense)
    {
        int damage = rng.roll(minDamage, maxDamage) - defense;
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
    placeMonsters(rooms);
    player_.pos = RoomGen::centerOf(rooms.front());
    player_.hp = PLAYER_HP;
    player_.maxHp = PLAYER_HP;
    Fov::compute(map_, player_.pos, SIGHT_RADIUS);

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
            // The view's camera keeps the player in sight
            view_->camera().follow(player_.pos);
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
                monsters_.push_back(monster);
                break;
            }
        }
    }
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

void Game::handleKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_ESCAPE)
    {
        running_ = false;
        return;
    }
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

    // The keys above work in every state. What the others do depends on
    // the state the game is in
    switch (state_)
    {
    case GameState::Playing:
        handlePlayingKey(key);
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

// The moving keys move the player
void Game::handlePlayingKey(const SDL_KeyboardEvent& key)
{
    Point step;
    if (stepFor(key, step))
    {
        playerStep(step);
        redraw_ = true;
    }
}

// The death screen is up. R ends this run, and asks for another. The other
// keys do nothing
void Game::handleDeadKey(const SDL_KeyboardEvent& key)
{
    if (key.key == SDLK_R)
    {
        playAgain_ = true;
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
        Fov::compute(map_, player_.pos, SIGHT_RADIUS);
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
    int damage = rollDamage(rng_, player_.minDamage, player_.maxDamage,
                            monster.defense);
    monster.hp -= damage;
    std::string name = monster.def->name;
    if (monster.isAlive())
    {
        log_.add("You hit the " + name + " for " + std::to_string(damage) +
                 ".");
    }
    else
    {
        log_.add("You kill the " + name + "!");
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

// The monsters' turn. A monster that can see the player attacks if it's
// next to them, and otherwise comes closer. The rest wander. Once the
// player is dead, no monster acts
void Game::monstersAct()
{
    for (Monster& monster : monsters_)
    {
        if (!player_.isAlive())
        {
            return;
        }
        if (!seesPlayer(monster))
        {
            monster.pos = wanderStep(monster);
        }
        else if (isNextTo(monster.pos, player_.pos))
        {
            monsterAttacks(monster);
        }
        else
        {
            monster.pos = chaseStep(monster);
        }
    }
}

// A monster strikes the player. If the blow kills, the player remembers
// what dealt it, for the death screen
void Game::monsterAttacks(const Monster& monster)
{
    int damage = rollDamage(rng_, monster.def->minDamage,
                            monster.def->maxDamage, player_.defense);
    player_.hp -= damage;
    std::string name = monster.def->name;
    log_.add("The " + name + " hits you for " + std::to_string(damage) +
             ".");
    if (!player_.isAlive())
    {
        player_.killedBy = withArticle(name);
    }
}

// Where a monster that sees the player steps: to the free neighbor nearest
// the player, if that's nearer than where it stands
Point Game::chaseStep(const Monster& monster)
{
    Point best = monster.pos;
    int bestDistance = distanceSquared(monster.pos, player_.pos);
    for (Point step : DIRECTIONS)
    {
        Point to = { monster.pos.x + step.x, monster.pos.y + step.y };
        int distance = distanceSquared(to, player_.pos);
        if (distance < bestDistance && isFree(to))
        {
            best = to;
            bestDistance = distance;
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
    drawMonsters();
    view_->drawPlayer(player_.pos);
    SDL_SetRenderClipRect(renderer_, nullptr);

    drawPanels();
    drawSidePanel();
    drawLog();
    if (state_ == GameState::Dead)
    {
        drawDeathScreen();
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
    glyphs_.drawText("Turn " + std::to_string(turn_), x, y, Palette::TEXT);
    y += row;
    glyphs_.drawText("Seed " + std::to_string(seed_), x, y,
                     Palette::DIM_TEXT);
    if (revealMap_)
    {
        y += 2 * row;
        glyphs_.drawText("DEBUG: map revealed (F1)", x, y, Palette::DEBUG);
    }
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

// The death screen. The dungeon fades, and a box in the middle of it says
// what killed the player, and when
void Game::drawDeathScreen() const
{
    // The shade is see-through, so it's blended with what's under it
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    setColor(Palette::SHADE);
    SDL_FRect mapView = { 0.0f, 0.0f, static_cast<float>(MAP_VIEW_W),
                          static_cast<float>(MAP_VIEW_H) };
    SDL_RenderFillRect(renderer_, &mapView);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

    float centerX = MAP_VIEW_W / 2.0f;
    float row = glyphs_.lineHeight();
    SDL_FRect box = { centerX - 300.0f, MAP_VIEW_H / 2.0f - 3 * row, 600.0f,
                      6 * row };
    setColor(Palette::PANEL);
    SDL_RenderFillRect(renderer_, &box);
    setColor(Palette::FRAME);
    SDL_RenderRect(renderer_, &box);

    float y = box.y + row;
    drawCentered("YOU HAVE DIED", centerX, y, Palette::DANGER);
    y += 2 * row;
    drawCentered("Killed by " + player_.killedBy + ", on turn " +
                 std::to_string(turn_) + ".", centerX, y, Palette::TEXT);
    y += row;
    drawCentered("Press R to play again, or Esc to quit.", centerX, y,
                 Palette::DIM_TEXT);
}
