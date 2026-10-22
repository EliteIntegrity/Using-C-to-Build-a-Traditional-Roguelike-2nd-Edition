#include "DijkstraMap.h"
#include <functional>   // std::greater, to make the queue smallest first
#include <queue>        // std::priority_queue, for the cells still to visit
#include <utility>      // std::pair, for a distance and a cell together

DijkstraMap::DijkstraMap()
    : distance_(MAP_W * MAP_H, UNREACHED)
{
}

void DijkstraMap::compute(const Map& map, const std::vector<Point>& goals)
{
    distance_.assign(distance_.size(), UNREACHED);
    for (Point goal : goals)
    {
        if (map.inBounds(goal.x, goal.y))
        {
            distance_[goal.y * MAP_W + goal.x] = 0;
        }
    }
    spread(map);
}

// Brogue's trick. Every distance is multiplied by -1.2, so the farthest
// cells become the lowest. Downhill on that alone, a monster would run
// into the nearest dead end that's a little farther away. So the map then
// spreads again: a cell next to a very low one can be no more than one
// step higher. Now a cell is low if it's near somewhere far away, and a
// fleeing monster heads for open ground, even if the way passes close to
// the goals. The 1.2 makes running past them worth it if the far side is
// far enough
DijkstraMap DijkstraMap::fleeMap(const Map& map) const
{
    DijkstraMap flee = *this;
    for (int& distance : flee.distance_)
    {
        if (distance < UNREACHED)
        {
            distance = -(distance * 6) / 5;
        }
    }
    flee.spread(map);
    return flee;
}

int DijkstraMap::at(Point cell) const
{
    if (cell.x < 0 || cell.x >= MAP_W || cell.y < 0 || cell.y >= MAP_H)
    {
        return UNREACHED;
    }
    return distance_[cell.y * MAP_W + cell.x];
}

// Dijkstra's algorithm. Every cell that has a distance waits in a queue,
// which always hands back the one with the smallest distance next. Each
// cell handed back offers its neighbors its own distance plus one step,
// and a neighbor that had a longer distance (or none) takes the offer, and
// joins the queue. When the queue is empty, every distance is the shortest
void DijkstraMap::spread(const Map& map)
{
    // Each entry is a distance and a cell's index. std::greater turns the
    // queue's usual order around, so the smallest distance comes out first
    using Entry = std::pair<int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    for (int i = 0; i < static_cast<int>(distance_.size()); ++i)
    {
        if (distance_[i] < UNREACHED)
        {
            queue.push({ distance_[i], i });
        }
    }

    while (!queue.empty())
    {
        int distance = queue.top().first;
        int index = queue.top().second;
        queue.pop();

        // A cell can be in the queue more than once, if it was offered a
        // shorter distance after joining it. Only its shortest counts
        if (distance > distance_[index])
        {
            continue;
        }

        Point cell = { index % MAP_W, index / MAP_W };
        for (Point step : DIRECTIONS)
        {
            Point next = { cell.x + step.x, cell.y + step.y };
            if (map.at(next.x, next.y) != Terrain::Floor)
            {
                continue;
            }
            int nextIndex = next.y * MAP_W + next.x;
            if (distance + 1 < distance_[nextIndex])
            {
                distance_[nextIndex] = distance + 1;
                queue.push({ distance + 1, nextIndex });
            }
        }
    }
}
