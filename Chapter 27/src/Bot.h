#pragma once
#include <SDL3/SDL.h>
#include <deque>      // std::deque, for the bot's last few decisions
#include <optional>   // std::optional, for where a monster was last seen
#include <set>        // std::set, for the items the bot has given up on
#include <string>     // std::string, for a run's results
#include <vector>     // std::vector, for the last decisions, handed over
#include "Common.h"
#include "Data.h"
#include "Game.h"
#include "GlyphCache.h"
#include "SoundBank.h"

// How a run ended: the class, the seed, how it ended (won, died, or stuck,
// still going after MAX_TURNS), the floor and the turn it ended on, what
// killed the player, if anything did, and, for a stuck run, the bot's last
// decisions, to show what it was trying
struct RunResult
{
    std::string className;
    unsigned int seed = 0;
    std::string ending;
    int floor = 1;
    int turn = 0;
    std::string killedBy;
    std::vector<std::string> lastDecisions;
};

// A player that's a program. It plays a run the way someone might who has
// played a few times: it fights what comes to it, drinks when it's badly
// hurt, picks everything up and wears the best of it, explores each floor
// with the game's own explore, and goes down. It plays through the
// functions the keys call, and nothing is drawn, so a run takes a second
// or two
class Bot
{
public:
    explicit Bot(Game& game);

    // Plays a run as a class, to its end, or until MAX_TURNS have passed
    void play(const ClassDef& def);

    // How the run went
    RunResult result() const;

private:
    // One decision, which mostly takes a turn
    void act();

    // The parts of a decision. Each returns true if it did something, and
    // says what in why_
    bool answer();
    bool drinkIfHurt();
    bool goForLastAmulet();
    bool fight(const Monster* foe);
    bool chase(const Monster* foe);
    bool castByHeartAt(const Monster& foe);
    bool shootAt(const Monster& foe);
    bool readAttackAt(const Monster& foe);
    bool pickUpHere();
    bool whileSafe();
    bool goToItem();
    bool explore();
    bool goDown();
    void wander();

    // Helpers
    void arrive();
    void countChaseStep();
    void remember(int turn, Point from);
    void wearTheBest();
    const Monster* nearestMonster() const;
    int monstersInView() const;
    bool walkTo(Point goal);
    void step(Point to);
    bool use(ItemEffect effect);
    bool useUnknown(ItemClass itemClass);
    void press(SDL_Keycode key);

    Game& game_;
    std::optional<Point> lastSeen_;   // where the monster it chased was
    std::set<int> skipped_;           // cells of items it has given up on
    bool explored_ = false;           // nothing left to explore here
    bool lost_ = false;               // no way on that it knows of
    int idle_ = 0;                    // decisions in a row with no turn
    int chaseSteps_ = 0;              // chasing, since its last blow
    int chaseAgainOn_ = 0;            // the turn it will chase again
    std::string why_;                 // the decision, in a few words
    std::deque<std::string> decisions_;   // the last LAST_DECISIONS
};

// One line about a run, and for a stuck run, its last decisions
std::string describe(const RunResult& run);

// Has the bot play runs seeds, from firstSeed, as each class in turn, or
// as one class only, if its id is given. It prints a line for each run,
// and at the end, a report of them all
void playBotRuns(SDL_Renderer* renderer, GlyphCache& glyphs,
                 const GameData& data, SoundBank& sounds, int runs,
                 unsigned int firstSeed, const std::string& onlyClass);
