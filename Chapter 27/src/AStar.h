#pragma once
#include <vector>   // std::vector, for the way's cells
#include "Common.h"
#include "Map.h"

// A*: the shortest way from one cell to another, for travel
namespace AStar
{
    // The cells of the shortest way from one cell to another, after the
    // first and ending with the second, or none if there's no way. Only
    // the ground and doors can be stepped on, and a step onto a closed
    // door counts as DijkstraMap::DOOR_STEPS steps: one to open it, and
    // one to step in. Of the ways with the fewest steps, it's one with the
    // fewest diagonal steps, so it walks straight where it can
    std::vector<Point> findPath(const Map& map, Point from, Point to);
}
