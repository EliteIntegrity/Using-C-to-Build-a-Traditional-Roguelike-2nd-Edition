#include "Fov.h"
#include "Map.h"

namespace
{
    // One of the four quarters around the origin: north, east, south or
    // west. Each is a triangle that widens away from the origin, one row
    // at a time. A cell in it is given by its depth, the number of rows
    // out, and its column, left or right of the quarter's middle line
    struct Quarter
    {
        Point origin;
        Point forward;   // one row further out
        Point across;    // one column across

        Point cellAt(int depth, int col) const
        {
            return { origin.x + forward.x * depth + across.x * col,
                     origin.y + forward.y * depth + across.y * col };
        }
    };

    // A slope from the origin: num columns across for every den rows out.
    // Kept as a fraction of whole numbers, so it's exact, and a line that
    // passes exactly through a corner is decided the same way every time
    struct Slope
    {
        int num;
        int den;   // always more than 0
    };

    // Division that rounds down, toward minus infinity. C++'s own division
    // rounds toward zero, so -3 / 2 is -1, where this gives -2
    int floorDiv(int a, int b)
    {
        int quotient = a / b;
        if (a % b != 0 && a < 0)
        {
            --quotient;
        }
        return quotient;
    }

    // The column that a slope crosses at a depth, rounded to the nearest
    // whole column. A slope that crosses exactly halfway between two
    // columns rounds up for the start of a row, and down for its end
    int roundTiesUp(int depth, Slope slope)
    {
        return floorDiv(2 * depth * slope.num + slope.den, 2 * slope.den);
    }

    int roundTiesDown(int depth, Slope slope)
    {
        return -floorDiv(slope.den - 2 * depth * slope.num, 2 * slope.den);
    }

    // The slope to the left edge of a cell, from the origin's center
    Slope leftEdge(int depth, int col)
    {
        return { 2 * col - 1, 2 * depth };
    }

    // True if the cell's center lies between the two slopes. Floor is only
    // lit when it is, which is what makes the field of view symmetric
    bool centerIsLit(int depth, int col, Slope start, Slope end)
    {
        return col * start.den >= depth * start.num &&
               col * end.den <= depth * end.num;
    }

    // Lights one row of a quarter: the cells from slope start to slope
    // end, at a depth. Light that gets past the row goes on to the next
    // one. Where a wall splits it, the part before the wall goes on by
    // itself, and the part after it carries on along this row
    void scanRow(Map& map, const Quarter& quarter, int radius, int depth,
                 Slope start, Slope end)
    {
        if (depth > radius)
        {
            return;
        }

        bool isFirst = true;
        bool previousBlocks = false;
        int firstCol = roundTiesUp(depth, start);
        int lastCol = roundTiesDown(depth, end);
        for (int col = firstCol; col <= lastCol; ++col)
        {
            Point cell = quarter.cellAt(depth, col);
            bool blocks = map.blocksSight(cell.x, cell.y);

            // Walls are lit if any of them is. Floor is lit only if its
            // center is. Either way, only within the radius
            bool inRange = depth * depth + col * col <= radius * (radius + 1);
            bool lit = blocks || centerIsLit(depth, col, start, end);
            if (inRange && lit)
            {
                map.setVisible(cell.x, cell.y);
            }

            if (!isFirst && previousBlocks && !blocks)
            {
                // A wall has ended: the light starts again at this cell
                start = leftEdge(depth, col);
            }
            if (!isFirst && !previousBlocks && blocks)
            {
                // A wall has begun: the light before it goes on to the
                // next row, as far as the wall's left edge
                Slope wallEdge = leftEdge(depth, col);
                scanRow(map, quarter, radius, depth + 1, start, wallEdge);
            }
            isFirst = false;
            previousBlocks = blocks;
        }

        // Unless the row ended in a wall, the light goes on to the next row
        if (!isFirst && !previousBlocks)
        {
            scanRow(map, quarter, radius, depth + 1, start, end);
        }
    }
}

void Fov::compute(Map& map, Point origin, int radius)
{
    map.clearVisible();
    map.setVisible(origin.x, origin.y);

    // Each quarter's forward and across steps, as x and y on the map
    const Quarter quarters[] = {
        { origin, { 0, -1 }, { 1, 0 } },    // north
        { origin, { 1, 0 }, { 0, 1 } },     // east
        { origin, { 0, 1 }, { 1, 0 } },     // south
        { origin, { -1, 0 }, { 0, 1 } },    // west
    };
    for (const Quarter& quarter : quarters)
    {
        // The first row is one cell out, and spans the whole quarter: from
        // one column left for each row out, to one column right
        scanRow(map, quarter, radius, 1, { -1, 1 }, { 1, 1 });
    }
}
