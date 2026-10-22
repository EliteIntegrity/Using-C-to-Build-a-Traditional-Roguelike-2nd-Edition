#pragma once
#include <SDL3/SDL.h>
#include <string>   // std::string, for text on the screen
#include <vector>   // std::vector, for the monsters and the rooms
#include "Common.h"
#include "GlyphCache.h"
#include "GlyphView.h"
#include "Map.h"
#include "MessageLog.h"
#include "Monster.h"
#include "Player.h"
#include "Rng.h"
#include "TileView.h"

// What the game is doing. It decides what the keys do, and what's drawn
enum class GameState
{
    Playing,
    Dead,
};

// One run of the game, from a new dungeon to the player's death: the
// dungeon and everyone in it, the rules that play out each turn, and the
// screen that shows it all
class Game
{
public:
    // A seed of 0 means a random one. With useTiles, the run starts in
    // the tile view
    Game(SDL_Renderer* renderer, GlyphCache& glyphs, unsigned int seed,
         bool useTiles);

    // Plays the run. Returns true if the player asked to play again
    bool run();

    // True if the tile view is the one in use, so the next run can start
    // in the same view
    bool usesTiles() const;

private:
    // Setting up
    void placeMonsters(const std::vector<SDL_Rect>& rooms);

    // Input
    void handleEvent(const SDL_Event& event);
    void handleKey(const SDL_KeyboardEvent& key);
    void handlePlayingKey(const SDL_KeyboardEvent& key);
    void handleDeadKey(const SDL_KeyboardEvent& key);
    void switchView();
    void playerStep(Point step);

    // The turn
    void playerAttacks(Monster& monster);
    void removeDeadMonsters();
    void finishTurn();
    void monstersAct();
    void monsterAttacks(const Monster& monster);
    Point chaseStep(const Monster& monster);
    Point wanderStep(const Monster& monster);
    bool seesPlayer(const Monster& monster) const;
    bool isFree(Point cell);
    Monster* monsterAt(Point pos);

    // Drawing
    void draw() const;
    void setColor(SDL_Color color) const;
    void drawMap() const;
    void drawMonsters() const;
    void drawPanels() const;
    void drawHealthBar(float x, float y, float width,
                       const Entity& entity) const;
    void drawSidePanel() const;
    void drawLog() const;
    void drawCentered(const std::string& text, float centerX, float y,
                      SDL_Color color) const;
    void drawDeathScreen() const;

    SDL_Renderer* renderer_;
    GlyphCache& glyphs_;

    // The map is drawn through view_, which points at the view in use
    GlyphView glyphView_;
    TileView tileView_;
    View* view_;

    // The run: its seed and dice, the dungeon, who's in it, and what's
    // been said
    unsigned int seed_;
    Rng rng_;
    Map map_;
    std::vector<Monster> monsters_;
    Player player_;
    MessageLog log_;
    int turn_ = 0;   // a turn passes each time the player acts

    GameState state_ = GameState::Playing;
    bool revealMap_ = false;   // the debugging aid on F1
    bool running_ = true;
    bool playAgain_ = false;
    bool redraw_ = true;
};
