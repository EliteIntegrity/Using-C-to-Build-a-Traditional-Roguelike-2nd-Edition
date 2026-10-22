#include "Bot.h"
#include <algorithm>   // std::find, for a monster on a line of fire
#include <cstdio>      // std::printf, for the results
#include <vector>      // std::vector, for paths and lines of fire
#include "AStar.h"
#include "BotReport.h"
#include "DijkstraMap.h"

namespace
{
    // A run still going after this many turns has gone wrong: the bot is
    // stuck. Most wins take 4,000 to 9,000, the rests included
    constexpr int MAX_TURNS = 30000;

    // The bot drinks below this share of its health, in percent
    constexpr int DRINK_BELOW = 40;

    // A monster further off than this, in steps, isn't worth chasing
    constexpr int CHASE_STEPS = 8;

    // Decisions in a row that take no turn before the bot gives up on what
    // it's trying, and waits a turn
    constexpr int MOST_IDLE = 10;

    // How many of its last decisions the bot keeps, to show what it was
    // trying if it gets stuck
    constexpr size_t LAST_DECISIONS = 12;

    // Steps of chasing with no blow struck before the bot gives up on a
    // monster, and the turns it then lets monsters be (countChaseStep)
    constexpr int CHASE_LIMIT = 40;
    constexpr int CHASE_REST = 100;

    int stepsApart(Point a, Point b)
    {
        return SDL_max(SDL_abs(a.x - b.x), SDL_abs(a.y - b.y));
    }

    // A cell as a number, for keeping in a set
    int cellKey(Point cell)
    {
        return cell.y * MAP_W + cell.x;
    }

    std::string cellText(Point cell)
    {
        return "(" + std::to_string(cell.x) + "," + std::to_string(cell.y) +
               ")";
    }
}

Bot::Bot(Game& game)
    : game_(game)
{
}

void Bot::play(const ClassDef& def)
{
    game_.startAs(def);
    while (game_.state_ != GameState::Dead &&
           game_.state_ != GameState::Won && game_.turn_ < MAX_TURNS)
    {
        int turn = game_.turn_;
        int depth = game_.level_.depth;
        Point from = game_.player_.pos;
        why_.clear();
        act();
        remember(turn, from);

        // Nothing is drawn, so the turn's flashes and sounds are let go
        game_.effects_.finish();

        if (game_.level_.depth != depth)
        {
            arrive();
        }
        if (game_.turn_ != turn)
        {
            idle_ = 0;
        }
        else if (++idle_ > MOST_IDLE)
        {
            // Whatever the bot was trying, it can't: it gives up on the
            // item under it, if any, and waits a turn
            skipped_.insert(cellKey(game_.player_.pos));
            game_.playerStep({ 0, 0 });
            idle_ = 0;
        }
    }
}

RunResult Bot::result() const
{
    RunResult run;
    run.className = game_.player_.classDef->name;
    run.seed = game_.seed_;
    run.ending = game_.state_ == GameState::Won    ? "won"
                 : game_.state_ == GameState::Dead ? "died"
                                                   : "stuck";
    run.floor = game_.level_.depth;
    run.turn = game_.turn_;
    run.killedBy = game_.player_.killedBy;
    if (run.ending == "stuck")
    {
        run.lastDecisions.assign(decisions_.begin(), decisions_.end());
    }
    return run;
}

// A decision: the first of these that does something is the turn's
void Bot::act()
{
    if (answer() || drinkIfHurt() || goForLastAmulet())
    {
        return;
    }
    const Monster* foe = nearestMonster();
    if (fight(foe) || chase(foe) || pickUpHere())
    {
        return;
    }
    if (foe == nullptr && whileSafe())
    {
        return;
    }
    if (goToItem() || explore() || goDown())
    {
        return;
    }
    wander();
}

// The game is asking something, or waiting for a choice
bool Bot::answer()
{
    Game& g = game_;
    if (g.state_ == GameState::Confirming)
    {
        // Going down without the floor's Amulet: by then, the bot has
        // looked everywhere it can. A known trap: only when it's lost
        bool descending = g.confirmPurpose_ == ConfirmPurpose::Descend;
        bool yes = descending || lost_;
        why_ = yes ? "says yes to the game's question"
                   : "says no to the game's question";
        press(yes ? SDLK_Y : SDLK_N);
        return true;
    }
    if (g.state_ == GameState::Inventory || g.state_ == GameState::Targeting)
    {
        why_ = "puts the pack away, or stops aiming";
        press(SDLK_ESCAPE);
        return true;
    }
    return false;
}

// Badly hurt, or about to die of poison: a healing potion it knows, or
// else one it doesn't, on the chance that it heals
bool Bot::drinkIfHurt()
{
    const Player& player = game_.player_;
    bool low = player.hp * 100 < player.maxHp * DRINK_BELOW;
    bool poisonKills = player.poisoned >= player.hp;
    if (!low && !poisonKills)
    {
        return false;
    }
    why_ = "drinks, badly hurt";
    return use(ItemEffect::HealFully) || use(ItemEffect::Heal) ||
           useUnknown(ItemClass::Potion);
}

// The third Amulet wins the run at once, so with two, the bot makes for
// the third when it's seen, whatever's in the way
bool Bot::goForLastAmulet()
{
    Game& g = game_;
    if (g.player_.amulets != AMULETS - 1)
    {
        return false;
    }
    for (const FloorItem& lying : g.level_.items)
    {
        if (lying.item.def->itemClass != ItemClass::Amulet || !lying.seen)
        {
            continue;
        }
        why_ = "makes for the last Amulet";
        if (lying.pos == g.player_.pos)
        {
            g.pickUp();
            return true;
        }
        return walkTo(lying.pos);
    }
    return false;
}

// The Mage casts lightning by heart at a monster, near or far, whenever
// it can: it hits far harder than the Mage's dagger. (The bot used to
// stab with the dagger at a monster beside it, and the Mage won far
// more often once it cast first.) A monster beside the bot is
// otherwise attacked; further off, a bow shoots it, and a scroll is read
// at it if the fight is going badly
bool Bot::fight(const Monster* foe)
{
    if (foe == nullptr)
    {
        return false;
    }
    if (castByHeartAt(*foe))
    {
        chaseSteps_ = 0;
        return true;
    }
    if (stepsApart(game_.player_.pos, foe->pos) == 1)
    {
        why_ = "attacks the " + foe->def->name;
        step(foe->pos);
        lastSeen_.reset();
        chaseSteps_ = 0;
        return true;
    }
    if (shootAt(*foe) || readAttackAt(*foe))
    {
        chaseSteps_ = 0;
        return true;
    }
    return false;
}

// A scroll of lightning, or of fireball, that the bot knows, at the foe,
// when the fight is going badly: the bot below half its health, or more
// than one monster in view. The line of fire is checked first, as for the
// spell and the bow, but before the scroll is read, since a read scroll
// let go is wasted. Lightning needs the foe on the line. A fireball needs
// the foe to be the first thing the line meets, three steps or more away,
// so its burst doesn't catch the bot
bool Bot::readAttackAt(const Monster& foe)
{
    Game& g = game_;
    const Player& player = g.player_;
    bool badly = player.hp * 2 < player.maxHp || monstersInView() > 1;
    if (!badly)
    {
        return false;
    }
    Point at = foe.pos;
    std::string name = foe.def->name;
    g.lookCursor_ = at;
    g.targetPurpose_ = TargetPurpose::Lightning;
    std::vector<Point> path = g.lineOfFire();
    bool onLine = std::find(path.begin(), path.end(), at) != path.end();
    g.targetPurpose_ = TargetPurpose::Fireball;
    path = g.lineOfFire();
    bool first = !path.empty() && path.back() == at &&
                 stepsApart(player.pos, at) >= 3;
    if (!(onLine && use(ItemEffect::Lightning)) &&
        !(first && use(ItemEffect::Fireball)))
    {
        return false;
    }
    why_ = "reads an attack scroll at the " + name;
    g.lookCursor_ = at;
    g.fireAtCursor();
    return true;
}

// How many monsters are in view
int Bot::monstersInView() const
{
    int count = 0;
    for (const Monster& monster : game_.level_.monsters)
    {
        bool seen = game_.level_.map.isVisible(monster.pos.x, monster.pos.y);
        if (seen && monster.isAlive())
        {
            ++count;
        }
    }
    return count;
}

// After a monster in view, unless it's running or far off; and with none
// worth chasing in view, on to where the one it was after was last seen,
// as a player would go, rather than forget it the moment it's out of
// sight. (A monster it won't chase doesn't make it forget the one it
// will: the trace of a stuck run showed the bot torn between a grizzly
// bear, seen from one cell, and a shrieker too far off to chase, seen
// from the next)
bool Bot::chase(const Monster* foe)
{
    Point here = game_.player_.pos;
    bool rested = game_.turn_ >= chaseAgainOn_;
    bool worthIt = foe != nullptr && rested &&
                   foe->state != MonsterState::Fleeing &&
                   stepsApart(here, foe->pos) <= CHASE_STEPS;
    if (worthIt)
    {
        // What the bot needs to know of the monster, it reads before the
        // step. The step plays out a turn, which may kill the monster, and
        // take it out of the floor's monsters, or drop the bot through a
        // trapdoor to a new floor: either way, foe would point at nothing
        Point at = foe->pos;
        why_ = "chases the " + foe->def->name + " at " + cellText(at);
        if (walkTo(at))
        {
            lastSeen_ = at;
            countChaseStep();
            return true;
        }
    }
    if (rested && lastSeen_ && here != *lastSeen_ && walkTo(*lastSeen_))
    {
        why_ = "goes where it last saw a monster, " + cellText(*lastSeen_);
        countChaseStep();
        return true;
    }
    lastSeen_.reset();
    return false;
}

// A step of chasing with no blow struck. After CHASE_LIMIT of them, the
// monster is one that keeps its distance, as archers do, and the bot lets
// it be for CHASE_REST turns. (Found in the trace of a stuck run: the bot
// following an archer that kept its distance, back and forth,
// forever)
void Bot::countChaseStep()
{
    if (++chaseSteps_ >= CHASE_LIMIT)
    {
        chaseAgainOn_ = game_.turn_ + CHASE_REST;
        chaseSteps_ = 0;
        lastSeen_.reset();
    }
}

// Lightning by heart, the Mage's gift, whenever it's ready, at the foe.
// It costs nothing, so the bot doesn't save it
bool Bot::castByHeartAt(const Monster& foe)
{
    Game& g = game_;
    bool knows = g.player_.classDef->has(Gift::LIGHTNING_BY_HEART);
    if (!knows || g.player_.spellTurns > 0)
    {
        return false;
    }
    g.castByHeart();
    g.lookCursor_ = foe.pos;
    std::vector<Point> path = g.lineOfFire();
    if (std::find(path.begin(), path.end(), foe.pos) == path.end())
    {
        press(SDLK_ESCAPE);   // out of reach: letting it go costs nothing
        return false;
    }
    why_ = "casts lightning at the " + foe.def->name;
    g.fireAtCursor();
    return true;
}

// An arrow, with a bow ready, arrows in the quiver, and a clear line
bool Bot::shootAt(const Monster& foe)
{
    Game& g = game_;
    if (!g.player_.bow || !g.player_.quiver)
    {
        return false;
    }
    g.fire();
    g.lookCursor_ = foe.pos;
    std::vector<Point> path = g.lineOfFire();
    if (path.empty() || path.back() != foe.pos)
    {
        press(SDLK_ESCAPE);
        return false;
    }
    why_ = "shoots at the " + foe.def->name;
    g.fireAtCursor();
    return true;
}

// Whatever lies here is picked up, unless the pack was full for it
// before. An Amulet always fits
bool Bot::pickUpHere()
{
    Game& g = game_;
    const FloorItem* here = g.floorItemAt(g.player_.pos);
    if (here == nullptr)
    {
        return false;
    }
    bool amulet = here->item.def->itemClass == ItemClass::Amulet;
    if (!amulet && skipped_.contains(cellKey(g.player_.pos)))
    {
        return false;
    }
    int turn = g.turn_;
    g.pickUp();
    if (g.turn_ == turn)
    {
        skipped_.insert(cellKey(g.player_.pos));   // the pack is full
        return false;
    }
    why_ = "picks up what's here";
    // Picking up takes a turn, which can end the run, if a monster strikes
    // or the third Amulet wins it. Wearing is only for a run still going
    if (g.state_ == GameState::Playing)
    {
        wearTheBest();
    }
    return true;
}

// With no monster in view: a potion of strength it knows is drunk at
// once; it rests to full health, a turn at a time, which costs nothing
// but turns; then it finds out what its unknown potions and scrolls are
// by trying them, as players do
bool Bot::whileSafe()
{
    Game& g = game_;
    if (use(ItemEffect::Strengthen))
    {
        why_ = "drinks a potion of strength";
        return true;
    }
    if (g.player_.hp < g.player_.maxHp)
    {
        why_ = "rests";
        g.playerStep({ 0, 0 });
        return true;
    }
    why_ = "tries something it doesn't know";
    return useUnknown(ItemClass::Potion) || useUnknown(ItemClass::Scroll);
}

// To the nearest item it has seen and not given up on: nearest by the way
// there, not as the crow flies, so that a step toward it leaves it the
// nearest, and the bot doesn't turn back and forth between two
bool Bot::goToItem()
{
    Game& g = game_;
    DijkstraMap fromBot;
    fromBot.compute(g.knownMap(), { g.player_.pos });
    const FloorItem* best = nullptr;
    int bestDistance = DijkstraMap::UNREACHED;
    for (const FloorItem& lying : g.level_.items)
    {
        if (!lying.seen || skipped_.contains(cellKey(lying.pos)))
        {
            continue;
        }
        int distance = fromBot.at(lying.pos);
        if (distance < bestDistance)
        {
            best = &lying;
            bestDistance = distance;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    Point goal = best->pos;   // read before the step, as in chase
    if (!walkTo(goal))
    {
        skipped_.insert(cellKey(goal));
        return false;
    }
    why_ = "goes to an item at " + cellText(goal);
    return true;
}

// A step of the game's own explore, until there's nothing left
bool Bot::explore()
{
    if (explored_)
    {
        return false;
    }
    int turn = game_.turn_;
    explored_ = game_.exploreStep();
    why_ = "explores";
    return game_.turn_ != turn;
}

// Down the stairs, or on toward them
bool Bot::goDown()
{
    Game& g = game_;
    Point here = g.player_.pos;
    if (g.level_.map.at(here.x, here.y) == Terrain::StairsDown)
    {
        why_ = "goes down the stairs";
        g.descend();
        return true;
    }
    std::optional<Point> stairs = g.knownStairs();
    why_ = "walks to the stairs";
    return stairs && walkTo(*stairs);
}

// Lost: no stairs it knows of, and nothing left to explore that it can
// reach, as when a scroll of teleportation has put it in a corner behind
// a secret door. It wanders, and searches, as a lost player would, and
// steps onto known traps if it has to. Then it explores again, in case a
// way has opened
void Bot::wander()
{
    lost_ = true;
    explored_ = false;
    if (SDL_rand(2) == 0)
    {
        why_ = "is lost, and searches";
        game_.search();
    }
    else
    {
        why_ = "is lost, and wanders";
        game_.playerStep(DIRECTIONS[SDL_rand(8)]);
    }
}

// A new floor: what the bot knew of the last one is no use
void Bot::arrive()
{
    lastSeen_.reset();
    skipped_.clear();
    explored_ = false;
    lost_ = false;
    chaseSteps_ = 0;
}

// Keeps the last LAST_DECISIONS decisions, as in "turn 1520, floor 4,
// (23,17): explores"
void Bot::remember(int turn, Point from)
{
    decisions_.push_back("turn " + std::to_string(turn) + ", floor " +
                         std::to_string(game_.level_.depth) + ", " +
                         cellText(from) + ": " + why_);
    if (decisions_.size() > LAST_DECISIONS)
    {
        decisions_.pop_front();
    }
}

// Wields a weapon that hits harder, wears better armor, readies a bow
// with none ready, and fills the quiver
void Bot::wearTheBest()
{
    Game& g = game_;
    const Player& player = g.player_;
    const std::vector<Item>& items = player.inventory.items();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const ItemDef& def = *items[i].def;
        bool better = false;
        if (def.itemClass == ItemClass::Weapon)
        {
            better = def.minDamage + def.maxDamage >
                     player.minDamage() + player.maxDamage();
        }
        else if (def.itemClass == ItemClass::Armor)
        {
            better = def.defense >
                     (player.armor ? player.armor->def->defense : 0);
        }
        else if (def.itemClass == ItemClass::Bow)
        {
            better = !player.bow ||
                     def.maxDamage > player.bow->def->maxDamage;
        }
        else if (def.itemClass == ItemClass::Missile)
        {
            better = !def.thrown;   // arrows, for the quiver
        }
        if (better)
        {
            g.useItem(static_cast<int>(i));
            return;
        }
    }
}

// The nearest monster in view, or nullptr
const Monster* Bot::nearestMonster() const
{
    const Monster* best = nullptr;
    int bestSteps = MAP_W;
    for (const Monster& monster : game_.level_.monsters)
    {
        bool seen = game_.level_.map.isVisible(monster.pos.x, monster.pos.y);
        int steps = stepsApart(game_.player_.pos, monster.pos);
        if (seen && monster.isAlive() && steps < bestSteps)
        {
            best = &monster;
            bestSteps = steps;
        }
    }
    return best;
}

// A step along the shortest way the bot knows to a cell, around the traps
// it knows of. Returns false if it knows no way there
bool Bot::walkTo(Point goal)
{
    std::vector<Point> path = AStar::findPath(game_.knownMap(),
                                              game_.player_.pos, goal);
    if (path.empty())
    {
        return false;
    }
    step(path.front());
    return true;
}

// A step toward a cell next to the bot, or into a monster there
void Bot::step(Point to)
{
    Point from = game_.player_.pos;
    game_.playerStep({ SDL_clamp(to.x - from.x, -1, 1),
                       SDL_clamp(to.y - from.y, -1, 1) });
}

// Uses the first potion or scroll in the pack known to have an effect
bool Bot::use(ItemEffect effect)
{
    Game& g = game_;
    const std::vector<Item>& items = g.player_.inventory.items();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const ItemDef& def = *items[i].def;
        if (def.effect == effect && g.identification_.isKnown(def))
        {
            g.useItem(static_cast<int>(i));
            return true;
        }
    }
    return false;
}

// Uses the first potion, or scroll, whose kind isn't known yet
bool Bot::useUnknown(ItemClass itemClass)
{
    Game& g = game_;
    const std::vector<Item>& items = g.player_.inventory.items();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const ItemDef& def = *items[i].def;
        if (def.itemClass == itemClass && !g.identification_.isKnown(def))
        {
            g.useItem(static_cast<int>(i));
            return true;
        }
    }
    return false;
}

// A key, as if pressed, for the game's questions and menus
void Bot::press(SDL_Keycode key)
{
    SDL_KeyboardEvent event{};
    event.key = key;
    game_.handleKey(event);
}

std::string describe(const RunResult& run)
{
    char line[160];
    SDL_snprintf(line, sizeof(line), "%-8s %10u  %-5s floor %d, turn %5d  %s",
                 run.className.c_str(), run.seed, run.ending.c_str(),
                 run.floor, run.turn, run.killedBy.c_str());
    std::string text = line;
    for (const std::string& decision : run.lastDecisions)
    {
        text += "\n      " + decision;
    }
    return text;
}

void playBotRuns(SDL_Renderer* renderer, GlyphCache& glyphs,
                 const GameData& data, SoundBank& sounds, int runs,
                 unsigned int firstSeed, const std::string& onlyClass)
{
    BotReport report;
    for (int i = 0; i < runs; ++i)
    {
        unsigned int seed = firstSeed + i;
        for (const ClassDef& def : data.classes())
        {
            if (!onlyClass.empty() && def.id != onlyClass)
            {
                continue;
            }

            // SDL's own dice, which the bot wanders with, start from the
            // seed too, so a run plays out the same every time
            SDL_srand(seed);
            Game game(renderer, glyphs, data, sounds, seed, false, false);
            Bot bot(game);
            bot.play(def);
            RunResult run = bot.result();
            std::printf("%s\n", describe(run).c_str());
            std::fflush(stdout);
            report.add(run);
        }
    }
    report.print();
}
