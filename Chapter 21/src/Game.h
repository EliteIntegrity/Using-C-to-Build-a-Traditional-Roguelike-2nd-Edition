#pragma once
#include <SDL3/SDL.h>
#include <optional>   // std::optional, for the mouse, which may not be there
#include <string>     // std::string, for text on the screen
#include <vector>     // std::vector, for the areas
#include "Common.h"
#include "Data.h"
#include "DijkstraMap.h"
#include "GlyphCache.h"
#include "GlyphView.h"
#include "Identification.h"
#include "Item.h"
#include "Level.h"
#include "Map.h"
#include "MessageLog.h"
#include "Monster.h"
#include "Player.h"
#include "Rng.h"
#include "TileView.h"
#include "VaultStamp.h"

// What the game is doing. It decides what the keys do, and what's drawn
enum class GameState
{
    Title,       // a saved run is waiting, to be continued or not
    Playing,
    Look,
    Inventory,
    Menu,        // Esc's menu, to save the run and quit
    Closing,     // asking which way to close a door
    Confirming,  // asking a yes or no, as ConfirmPurpose says
    Targeting,   // aiming, with the cursor, at a cell
    Dead,
};

// What the player is aiming for
enum class TargetPurpose
{
    Lightning,   // a scroll of lightning's bolt
    Fire,        // an arrow from the bow
    Throw,       // something made to be thrown: a dagger, or a dart
    Fireball,    // a scroll of fireball's burst
};

// What a yes in the Confirming state would do
enum class ConfirmPurpose
{
    Step,      // step onto a trap the player knows of
    Descend,   // go down the stairs, leaving this floor's Amulet behind
};

// What came of loading the save
enum class LoadResult
{
    Loaded,
    OtherVersion,   // it's from another version of the game
    Damaged,        // it can't be used, for the reasons in errors
};

// What choosing an item in the inventory does with it
enum class InventoryAction
{
    Use,
    Drop,
    Identify,
    Throw,
};

// One run of the game, from a new dungeon to the player's death: the
// dungeon and everyone in it, the rules that play out each turn, and the
// screen that shows it all
class Game
{
public:
    // The kinds of monster and item come from data, which must outlive
    // the Game. A seed of 0 means a random one. With useTiles, the run
    // starts in the tile view
    Game(SDL_Renderer* renderer, GlyphCache& glyphs, const GameData& data,
         unsigned int seed, bool useTiles);

    // Plays the run. Returns true if the player asked to play again
    bool run();

    // True if the tile view is the one in use, so the next run can start
    // in the same view
    bool usesTiles() const;

private:
    // Setting up
    void newLevel(int depth);
    void placeMonsters(const std::vector<SDL_Rect>& areas);
    const MonsterDef* chooseMonsterKind();
    std::optional<Point> freeCellIn(const SDL_Rect& area);
    void addMonster(const MonsterDef& kind, Point cell, int packId,
                    bool awake);
    const ItemDef* chooseItemKind();
    void placeItems(const std::vector<SDL_Rect>& areas);
    void placeStairs();
    void placeGold(const std::vector<SDL_Rect>& areas);
    void placeSecretDoors(const std::vector<SDL_Rect>& rooms);
    void placeTraps(const std::vector<SDL_Rect>& areas);
    TrapKind chooseTrapKind();
    std::optional<VaultStamp::Contents> buildVault(
        std::vector<SDL_Rect>& areas, bool amulet);
    std::optional<Point> roomForVault(const VaultDef& vault,
                                      const std::vector<SDL_Rect>& rooms);
    std::optional<Point> caveForVault(const VaultDef& vault);
    void fillVault(const VaultStamp::Contents& contents);
    bool inVault(Point cell) const;

    // Input
    void handleEvent(const SDL_Event& event);
    void handleKey(const SDL_KeyboardEvent& key);
    void handlePlayingKey(const SDL_KeyboardEvent& key);
    void handleLookKey(const SDL_KeyboardEvent& key);
    void handleInventoryKey(const SDL_KeyboardEvent& key);
    void handleDeadKey(const SDL_KeyboardEvent& key);
    void handleTitleKey(const SDL_KeyboardEvent& key);
    void handleMenuKey(const SDL_KeyboardEvent& key);
    void handleClosingKey(const SDL_KeyboardEvent& key);
    void openDoor(Point door);
    void closeDoor(Point door);
    void search();
    void continueRun();
    void saveAndQuit();
    void putBackUnfinishedScroll();
    void switchView();
    void stepCave();
    void stepVault();
    void trackMouse(SDL_Event event);
    std::optional<Point> hoveredCell() const;
    std::optional<Point> lookTarget() const;
    void playerStep(Point step, bool confirmed = false);
    void handleConfirmKey(const SDL_KeyboardEvent& key);
    void handleClick(SDL_Event event);
    void startTargeting(TargetPurpose purpose);
    void handleTargetingKey(const SDL_KeyboardEvent& key);
    void targetNext();
    void stopTargeting();
    void fire();
    void chooseThrow(int slot);
    void fireAtCursor();
    void shootMissile(Item missile, const std::vector<Point>& path,
                      int minDamage, int maxDamage);
    void dropNear(Item item, Point cell);
    std::vector<Point> lineOfFire() const;
    void castLightning(const std::vector<Point>& path);
    void castFireball(Point center);
    std::vector<Point> burstAt(Point center, int radius) const;
    void hurtMonster(Monster& monster, int damage, const std::string& by);
    bool springTrap(Trap& trap);
    void arriveOnFloor(const std::string& how);
    void pickUp();
    void descend(bool confirmed = false);
    bool amuletStillHere() const;
    void openInventory(InventoryAction action);
    void useItem(int slot);
    void wield(int slot);
    void wear(int slot);
    void readyBow(int slot);
    void fillQuiver(int slot);
    void dropItem(int slot);
    void identifyItem(int slot);
    void applyEffect(const ItemDef& def);
    void tickStatuses(Entity& entity);
    void frightenMonsters();
    void teleportPlayer();
    void updateSight();

    // Saving and loading, in SaveGame.cpp
    bool saveExists() const;
    bool writeSave();
    LoadResult loadSave(std::vector<std::string>& errors);
    void removeSave();
    bool setSaveAside();

    // The turn
    void playerAttacks(Monster& monster);
    void alarm(Monster& monster);
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
    void wakePack(const Monster& woken);
    void actStationary(Monster& monster);
    void makeNoise(Point origin, int loudness);
    void monsterAttacks(Monster& monster);
    bool attacksFromAfar(Monster& monster);
    bool clearShot(Point from, Point to);
    void monsterShoots(Monster& monster);
    void monsterCastsLightning(Monster& monster);
    void monsterBreathes(Monster& monster);
    bool backsAway(Monster& monster);
    void keepDistance(Monster& monster);
    Point stepDown(const Monster& monster, const DijkstraMap& distances);
    Point wanderStep(const Monster& monster);
    bool seesPlayer(const Monster& monster) const;
    bool isFree(Point cell);
    bool canEnter(const Monster& monster, Point cell);
    void moveMonster(Monster& monster, Point to);
    Monster* monsterAt(Point pos);
    Trap* trapAt(Point pos);

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
    void drawCaveSteps() const;
    void drawVaultShown() const;
    void drawPanels() const;
    void drawHealthBar(float x, float y, float width,
                       const Entity& entity) const;
    void drawSidePanel() const;
    void drawLookPanel() const;
    float drawWrapped(const std::string& text, float y, SDL_Color color) const;
    const Monster* visibleMonsterAt(Point cell) const;
    const Trap* knownTrapAt(Point cell) const;
    void drawTraps() const;
    void drawLineOfFire() const;
    void drawLog() const;
    void drawCentered(const std::string& text, float centerX, float y,
                      SDL_Color color) const;
    SDL_FRect drawDialog(float width, float height) const;
    void drawDeathScreen() const;
    void drawTitle() const;
    void drawMenu() const;
    void drawInventory() const;
    std::string wieldingText() const;
    std::string wearingText() const;
    std::string bowText() const;

    SDL_Renderer* renderer_;
    GlyphCache& glyphs_;
    const GameData& data_;

    // The map is drawn through view_, which points at the view in use
    GlyphView glyphView_;
    TileView tileView_;
    View* view_;

    // The run: its seed and dice, the floor the player is on, what the
    // player knows of the items, the player, and what's been said
    unsigned int seed_;
    Rng rng_;
    Level level_;
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
    GameState keyState_ = GameState::Playing;   // where the last key went down

    // Why the saved run couldn't be loaded, for the title screen, if it
    // couldn't, whether that's because it's from another version, and
    // whether a damaged one could be set aside
    std::vector<std::string> loadErrors_;
    bool saveIsOtherVersion_ = false;
    bool saveSetAside_ = false;

    // Where the mouse is, in the game's picture, while it's over the map
    // view. Empty while it's anywhere else
    std::optional<SDL_FPoint> mouse_;

    // The cell the cursor is on, in the Look state, and the Targeting
    // state, where it's what the player aims at, and what for
    Point lookCursor_;
    TargetPurpose targetPurpose_ = TargetPurpose::Lightning;
    int throwSlot_ = 0;   // the pack's slot of what's being thrown

    // What the Confirming state is asking about, and the step waiting for
    // a yes, if it's about a step
    ConfirmPurpose confirmPurpose_ = ConfirmPurpose::Step;
    Point pendingStep_;

    bool revealMap_ = false;       // the debugging aid on F1
    bool showDistances_ = false;   // and the one on F3
    bool showMinds_ = false;       // and on F4

    // The debugging aid on F6: a cave being made, while it's showing, and
    // the step it's at: 0 for the random fill, then each smoothing pass,
    // then the biggest cave kept, with its size, and where a player would
    // start in it. cavesShown_ counts the caves, for their dice
    struct CaveSteps
    {
        Map map;
        int step = 0;
        int cells = 0;
        std::optional<Point> start;
    };
    std::optional<CaveSteps> caveSteps_;
    int cavesShown_ = 0;

    // The debugging aid on F7: which vault in vaults.txt is showing, if any
    std::optional<int> vaultShown_;
    bool running_ = true;
    bool playAgain_ = false;
    bool redraw_ = true;
};
