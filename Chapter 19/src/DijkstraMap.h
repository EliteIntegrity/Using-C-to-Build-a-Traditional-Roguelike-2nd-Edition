#pragma once
#include <vector>   // std::vector, for the distances
#include "Common.h"
#include "Map.h"

// For every cell of the map, how many steps it is to the nearest of some
// goal cells, walking in eight directions over the ground, and through
// closed doors, which count as more than one step. A monster that steps
// to its lowest neighbor, again and again, reaches a goal by the shortest
// way around the walls. Roguelike programmers call this a Dijkstra map,
// after the computer scientist whose algorithm fills it in
class DijkstraMap
{
public:
    // The distance of a cell that no goal can be reached from
    static constexpr int UNREACHED = 1000000;

    // What a closed door counts as, unless asked otherwise: two steps, one
    // to open it and one to step in
    static constexpr int DOOR_STEPS = 2;

    DijkstraMap();

    // Works out every cell's distance from the nearest of the goals. A
    // closed door counts as doorCost steps
    void compute(const Map& map, const std::vector<Point>& goals,
                 int doorCost = DOOR_STEPS);

    // A map for running away from this one's goals. Walking downhill on it
    // leads to the places farthest from them that can be reached, and
    // around the goals if that's the way to somewhere far
    DijkstraMap fleeMap(const Map& map) const;

    // A cell's distance. Off the map, it's UNREACHED
    int at(Point cell) const;

private:
    void spread(const Map& map);

    std::vector<int> distance_;
    int doorCost_ = DOOR_STEPS;
};
