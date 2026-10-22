#include "AStar.h"
#include <algorithm>    // std::reverse, to put the way in order
#include <cstdlib>      // std::abs, for distances
#include <functional>   // std::greater, to make the queue smallest first
#include <queue>        // std::priority_queue, for the cells still to visit
#include <tuple>        // std::tuple, for what the queue holds of a cell
#include "DijkstraMap.h"

namespace
{
    // A way's cost is counted in hundredths of a step: STEP for a step, and
    // DIAGONAL_EXTRA more for a diagonal one. Between ways with as many
    // steps as each other, the extra picks the one with fewer diagonal
    // steps. A way with a step more would need over a hundred fewer
    // diagonal steps to cost less
    constexpr int STEP = 100;
    constexpr int DIAGONAL_EXTRA = 1;

    // The least the way from one cell to another could cost, if nothing
    // were in the way: a step for each cell of the longer of the distances
    // across and down, since a diagonal step covers one of each, and the
    // extra for the diagonal steps that takes. A* needs a guess at the
    // cost still to come that's never too much, and this is one
    int leastCost(Point a, Point b)
    {
        int across = std::abs(a.x - b.x);
        int down = std::abs(a.y - b.y);
        return SDL_max(across, down) * STEP +
               SDL_min(across, down) * DIAGONAL_EXTRA;
    }

    // How far apart two cells are as the crow flies, squared. Of two cells
    // that look just as promising, the one nearer the goal this way is
    // tried first, so the way found makes for the goal, rather than
    // wandering among the many ways that are just as short
    int crowFlies(Point a, Point b)
    {
        int dx = a.x - b.x;
        int dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    int indexOf(Point cell)
    {
        return cell.y * MAP_W + cell.x;
    }
}

// A* is Dijkstra's algorithm with a sense of direction. The queue hands
// back the cell whose way so far, plus the least the rest could cost, is
// the least, so the search heads for the goal, and stops when it gets
// there. Each cell remembers the cell it was reached from, and following
// those back from the goal gives the way
std::vector<Point> AStar::findPath(const Map& map, Point from, Point to)
{
    std::vector<Point> path;
    if (from == to || !map.isPassable(to.x, to.y))
    {
        return path;
    }

    // The cost of the cheapest way found so far to each cell, and the cell
    // it came from
    std::vector<int> cost(MAP_W * MAP_H, DijkstraMap::UNREACHED);
    std::vector<int> cameFrom(MAP_W * MAP_H, -1);

    // Each entry is a cell's guess at the whole way's cost, its distance
    // from the goal as the crow flies, and its index. std::greater puts
    // the least first, comparing the guesses, and only if they're equal,
    // the crow's distances
    using Entry = std::tuple<int, int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    cost[indexOf(from)] = 0;
    open.push({ leastCost(from, to), crowFlies(from, to), indexOf(from) });

    while (!open.empty())
    {
        // A structured binding gives the entry's three values names
        auto [guess, crow, index] = open.top();
        open.pop();
        Point cell = { index % MAP_W, index / MAP_W };
        if (cell == to)
        {
            break;
        }

        // A cell can be in the queue more than once, if a cheaper way to it
        // was found after it joined. Only the cheapest counts
        if (guess > cost[index] + leastCost(cell, to))
        {
            continue;
        }

        for (Point step : DIRECTIONS)
        {
            Point next = { cell.x + step.x, cell.y + step.y };
            int stepCost = STEP;
            if (map.at(next.x, next.y) == Terrain::ClosedDoor)
            {
                stepCost = DijkstraMap::DOOR_STEPS * STEP;
            }
            else if (!map.isWalkable(next.x, next.y))
            {
                continue;
            }
            if (step.x != 0 && step.y != 0)
            {
                stepCost += DIAGONAL_EXTRA;
            }
            int nextCost = cost[index] + stepCost;
            int nextIndex = indexOf(next);
            if (nextCost < cost[nextIndex])
            {
                cost[nextIndex] = nextCost;
                cameFrom[nextIndex] = index;
                open.push({ nextCost + leastCost(next, to),
                            crowFlies(next, to), nextIndex });
            }
        }
    }

    // No way there, or the way, followed back from the goal, then turned
    // around
    if (cost[indexOf(to)] == DijkstraMap::UNREACHED)
    {
        return path;
    }
    for (int index = indexOf(to); index != indexOf(from);
         index = cameFrom[index])
    {
        path.push_back({ index % MAP_W, index / MAP_W });
    }
    std::reverse(path.begin(), path.end());
    return path;
}
