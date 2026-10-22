#pragma once
#include "Common.h"

class Map;

// The field of view: which cells can be seen from a cell. It's worked out
// by symmetric shadowcasting, as Albert Ford described it in 2020, so if
// one cell can see another, the other can see it too
namespace Fov
{
    // Marks every cell that can be seen from origin, up to radius cells
    // away, as visible, once it has cleared what was visible before
    void compute(Map& map, Point origin, int radius);
}
