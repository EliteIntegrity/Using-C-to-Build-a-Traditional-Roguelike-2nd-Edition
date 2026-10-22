#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for the mouse, which may not be there
#include <string>     // std::string, for text on the screen
#include <vector>     // std::vector, for the monsters and the rooms
#include "Common.h"
#include "DijkstraMap.h"
#include "GlyphCache.h"
#include "GlyphView.h"
#include "Identification.h"
#include "Item.h"
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
    Look,
    Inventory,
    Dead,
};

// What choosing an item in the inventory does with it
enum class InventoryAction
{
    Use,
    Drop,
    Identify,
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
    void placeItems(const std::vector<SDL_Rect>& rooms);

    // Input
    void handleEvent(const SDL_Event& event);
    void handleKey(const SDL_KeyboardEvent& key);
    void handlePlayingKey(const SDL_KeyboardEvent& key);
    void handleLookKey(const SDL_KeyboardEvent& key);
    void handleInventoryKey(const SDL_KeyboardEvent& key);
    void handleDeadKey(const SDL_KeyboardEvent& key);
    void switchView();
    void trackMouse(SDL_Event event);
    std::optional<Point> hoveredCell() const;
    std::optional<Point> lookTarget() const;
    void playerStep(Point step);
    void pickUp();
    void openInventory(InventoryAction action);
    void useItem(int slot);
    void wield(int slot);
    void wear(int slot);
    void dropItem(int slot);
    void identifyItem(int slot);
    void applyEffect(const ItemDef& def);
    void teleportPlayer();
    void updateSight();

    // The turn
    void playerAttacks(Monster& monster);
    void removeDeadMonsters();
    void finishTurn();
    void monstersAct();
    void actAsleep(Monster& monster);
    void actWandering(Monster& monster);
    void actHunting(Monster& monster);
    void actFleeing(Monster& monster);
    bool walkToward(Monster& monster, Point goal);
    bool movesErratically(Monster& monster);
    void giveUp(Monster& monster);
    std::optional<Point> randomFloorCell();
    void announce(const Monster& monster, const std::string& what);
    void actStationary(Monster& monster);
    void makeNoise(Point origin, int loudness);
    void monsterAttacks(const Monster& monster);
    Point stepDown(const Monster& monster, const DijkstraMap& distances);
    Point wanderStep(const Monster& monster);
    bool seesPlayer(const Monster& monster) const;
    bool isFree(Point cell);
    Monster* monsterAt(Point pos);

    // Items
    const FloorItem* floorItemAt(Point pos) const;
    std::string itemName(const Item& item) const;
    ItemLook itemLook(const Item& item) const;

    // Drawing
    void draw() const;
    void setColor(SDL_Color color) const;
    void drawMap() const;
    void drawMonsters() const;
    void drawItems() const;
    void drawHighlight(Point cell) const;
    void drawDistances() const;
    void drawMinds() const;
    void drawPanels() const;
    void drawHealthBar(float x, float y, float width,
                       const Entity& entity) const;
    void drawSidePanel() const;
    void drawLookPanel() const;
    float drawWrapped(const std::string& text, float y, SDL_Color color) const;
    const Monster* visibleMonsterAt(Point cell) const;
    void drawLog() const;
    void drawCentered(const std::string& text, float centerX, float y,
                      SDL_Color color) const;
    SDL_FRect drawDialog(float width, float height) const;
    void drawDeathScreen() const;
    void drawInventory() const;
    std::string wieldingText() const;
    std::string wearingText() const;

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
    std::vector<FloorItem> floorItems_;
    Identification identification_;
    Player player_;
    MessageLog log_;

    // Every cell's distance from the player, and the map for running away
    // from them, worked out once a turn
    DijkstraMap toPlayer_;
    DijkstraMap fromPlayer_;
    int turn_ = 0;   // a turn passes each time the player acts

    GameState state_ = GameState::Playing;
    InventoryAction inventoryAction_ = InventoryAction::Use;

    // Where the mouse is, in the game's picture, while it's over the map
    // view. Empty while it's anywhere else
    std::optional<SDL_FPoint> mouse_;

    // The cell the look cursor is on, in the Look state
    Point lookCursor_;

    bool revealMap_ = false;       // the debugging aid on F1
    bool showDistances_ = false;   // and the one on F3
    bool showMinds_ = false;       // and on F4
    bool running_ = true;
    bool playAgain_ = false;
    bool redraw_ = true;
};
