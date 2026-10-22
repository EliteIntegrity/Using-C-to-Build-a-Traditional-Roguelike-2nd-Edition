#pragma once
#include <SDL3/SDL.h>
#include <vector>   // std::vector, for the effects and their cells
#include "Common.h"
#include "SoundEvent.h"
#include "View.h"

// What an effect shows
enum class EffectKind
{
    Flash,     // cells flashing a color, which fades
    Missile,   // something flying along a path, a cell at a time
    Ghost,     // a monster already killed, shown until the blow lands
    Shake,     // the map view shaking, less and less
};

// An effect, shown over the map for a moment, so the player sees what
// happened. Its start and end are times in milliseconds, as SDL_GetTicks
// gives them
struct Effect
{
    EffectKind kind = EffectKind::Flash;
    std::vector<Point> cells;              // a flash's, or a missile's path
    SDL_Color color = {};                  // a flash's
    ItemLook look = {};                    // a missile's
    const MonsterDef* monster = nullptr;   // a ghost's, on its one cell
    Uint64 start = 0;
    Uint64 end = 0;
};

// The effects playing, and those waiting their turn. The game works out a
// whole turn at once, in an instant, but its effects play one after
// another, in the order they're asked for, so the player sees the turn in
// the order it happened
class Effects
{
public:
    // How long a flash lasts
    static constexpr Uint64 FLASH_MS = 150;

    // How long a missile takes to fly a cell
    static constexpr Uint64 MISSILE_MS = 25;

    // How long a shake lasts, and how far it moves the view at first, in
    // pixels
    static constexpr Uint64 SHAKE_MS = 200;
    static constexpr float SHAKE_PX = 6.0f;

    // Cells flash a color, once the effects before it have played
    void flash(const std::vector<Point>& cells, SDL_Color color,
               Uint64 now);

    // Something flies along a path, once the effects before it have
    // played. Returns when it lands
    Uint64 missile(const std::vector<Point>& path, const ItemLook& look,
                   Uint64 now);

    // A monster that's just been killed stays on its cell, from now until
    // the effects asked for so far have played: the blow that killed it
    void ghost(Point cell, const MonsterDef& def, Uint64 now);

    // The view shakes, from the moment the last flash or missile asked for
    // starts: the hit, or the blast, that shakes it
    void shake(Uint64 now);

    // How far the shaking moves the view at a moment: nowhere, unless it's
    // shaking
    SDL_FPoint shakeOffset(Uint64 now) const;

    // A sound, played as the last flash or missile asked for starts: the
    // hit, the shot or the blast it belongs to. With nothing waiting, it
    // plays at once
    void sound(SoundEvent event, Uint64 now);

    // A sound at a moment given: a miss, heard as the arrow lands, though
    // there's nothing there to see
    void soundAt(SoundEvent event, Uint64 when);

    // The sounds whose moment has come, taken out to be played
    std::vector<SoundEvent> soundsDue(Uint64 now);

    // Forgets the effects that have played
    void update(Uint64 now);

    // True while an effect is playing or waiting, or a sound is waiting
    bool playing() const;

    // Every effect, for drawing, which leaves out those that haven't
    // started yet, or have ended
    const std::vector<Effect>& all() const;

    // Ends them all at once, as a key does: the player wants to get on
    void finish();

private:
    // When the next effect can start: now, or when the last one ends
    Uint64 nextStart(Uint64 now) const;

    std::vector<Effect> effects_;

    // A sound, and when it's to play
    struct Sound
    {
        SoundEvent event = SoundEvent::Hit;
        Uint64 start = 0;
    };
    std::vector<Sound> sounds_;

    // When the last flash or missile asked for starts
    Uint64 lastStart_ = 0;
};
