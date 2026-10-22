#include "RoomGen.h"
#include <algorithm>   // std::min, for the corridors
#include <cstdlib>     // std::abs, for the corridors

namespace
{
    // How many times the map can be halved, so at most 2 to the power of
    // MAX_CUTS rooms
    constexpr int MAX_CUTS = 4;

    // A piece is only cut if both halves would be at least this big
    constexpr int MIN_PIECE_W = 12;
    constexpr int MIN_PIECE_H = 9;

    // The smallest room, and the solid rock kept around each room, inside
    // its piece, so rooms in neighboring pieces never touch
    constexpr int MIN_ROOM = 4;
    constexpr int PADDING = 1;

    int roll(std::mt19937& rng, int low, int high)
    {
        return std::uniform_int_distribution<int>(low, high)(rng);
    }

    // Makes a room of random size, at a random place inside area, and
    // carves it out of the rock
    SDL_Rect addRoom(Map& map, const SDL_Rect& area, std::mt19937& rng)
    {
        int maxW = area.w - 2 * PADDING;
        int maxH = area.h - 2 * PADDING;
        SDL_Rect room;
        room.w = roll(rng, MIN_ROOM, maxW);
        room.h = roll(rng, MIN_ROOM, maxH);
        room.x = area.x + PADDING + roll(rng, 0, maxW - room.w);
        room.y = area.y + PADDING + roll(rng, 0, maxH - room.h);
        map.carve(room);
        return room;
    }

    // Digs a straight corridor between two points in a line, both ends
    // included
    void carveLine(Map& map, Point a, Point b)
    {
        SDL_Rect line;
        line.x = std::min(a.x, b.x);
        line.y = std::min(a.y, b.y);
        line.w = std::abs(a.x - b.x) + 1;
        line.h = std::abs(a.y - b.y) + 1;
        map.carve(line);
    }

    // Digs an L-shaped corridor between two points: across and then down,
    // or down and then across, at random
    void carveCorridor(Map& map, Point from, Point to, std::mt19937& rng)
    {
        Point corner = { to.x, from.y };
        if (roll(rng, 0, 1) == 1)
        {
            corner = { from.x, to.y };
        }
        carveLine(map, from, corner);
        carveLine(map, corner, to);
    }

    // Cuts area in two, and each half again, until it runs out of cuts or
    // room, and gives every piece that's left a room. Returns the middle of
    // one of area's rooms, so that whoever cut area can join it to the other
    // half
    Point split(Map& map, const SDL_Rect& area, int cutsLeft,
                std::mt19937& rng, std::vector<SDL_Rect>& rooms)
    {
        bool canCutAcross = area.w >= 2 * MIN_PIECE_W;
        bool canCutDown = area.h >= 2 * MIN_PIECE_H;
        if (cutsLeft == 0 || (!canCutAcross && !canCutDown))
        {
            SDL_Rect room = addRoom(map, area, rng);
            rooms.push_back(room);
            return RoomGen::centerOf(room);
        }

        // Cut across the longer side, when either way would do
        bool across = canCutAcross && (!canCutDown || area.w >= area.h);
        SDL_Rect first = area;
        SDL_Rect second = area;
        if (across)
        {
            int cut = roll(rng, MIN_PIECE_W, area.w - MIN_PIECE_W);
            first.w = cut;
            second.x = area.x + cut;
            second.w = area.w - cut;
        }
        else
        {
            int cut = roll(rng, MIN_PIECE_H, area.h - MIN_PIECE_H);
            first.h = cut;
            second.y = area.y + cut;
            second.h = area.h - cut;
        }
        // Every room in each half is joined to the others in it, so one
        // corridor between the halves joins them all
        Point a = split(map, first, cutsLeft - 1, rng, rooms);
        Point b = split(map, second, cutsLeft - 1, rng, rooms);
        carveCorridor(map, a, b, rng);
        if (roll(rng, 0, 1) == 0)
        {
            return a;
        }
        return b;
    }
}

std::vector<SDL_Rect> RoomGen::generate(Map& map, std::mt19937& rng)
{
    map.fill(Terrain::Wall);
    std::vector<SDL_Rect> rooms;

    // Leave the outermost cells as rock, so the walls have somewhere to be
    SDL_Rect inside = { 1, 1, MAP_W - 2, MAP_H - 2 };
    split(map, inside, MAX_CUTS, rng, rooms);
    return rooms;
}

Point RoomGen::centerOf(const SDL_Rect& room)
{
    return { room.x + room.w / 2, room.y + room.h / 2 };
}
