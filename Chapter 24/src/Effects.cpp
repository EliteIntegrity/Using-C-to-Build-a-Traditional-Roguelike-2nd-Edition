#include "Effects.h"
#include <vector>   // std::erase_if, to forget the effects that have played

void Effects::flash(const std::vector<Point>& cells, SDL_Color color,
                    Uint64 now)
{
    Effect effect;
    effect.cells = cells;
    effect.color = color;
    effect.start = nextStart(now);
    effect.end = effect.start + FLASH_MS;
    effects_.push_back(effect);
}

void Effects::missile(const std::vector<Point>& path, const ItemLook& look,
                      Uint64 now)
{
    Effect effect;
    effect.kind = EffectKind::Missile;
    effect.cells = path;
    effect.look = look;
    effect.start = nextStart(now);
    effect.end = effect.start + MISSILE_MS * path.size();
    effects_.push_back(effect);
}

void Effects::ghost(Point cell, const MonsterDef& def, Uint64 now)
{
    Effect effect;
    effect.kind = EffectKind::Ghost;
    effect.cells = { cell };
    effect.monster = &def;
    effect.start = now;
    effect.end = nextStart(now);
    effects_.push_back(effect);
}

void Effects::shake(Uint64 now)
{
    Effect effect;
    effect.kind = EffectKind::Shake;
    effect.start = effects_.empty() ? now : effects_.back().start;
    effect.end = effect.start + SHAKE_MS;
    effects_.push_back(effect);
}

// A shake moves the view back and forth, across and up and down at
// different rates, so it jitters rather than circles, and less and less
// as it goes on. A swing across takes about 50 milliseconds, and one up
// and down about 70, long enough for a few frames to show each one. Its
// moves come from the time, not the game's dice, which are kept for the
// game, so a seed still replays the same dungeon
SDL_FPoint Effects::shakeOffset(Uint64 now) const
{
    SDL_FPoint offset = { 0.0f, 0.0f };
    for (const Effect& effect : effects_)
    {
        bool shaking = effect.kind == EffectKind::Shake &&
                       effect.start <= now && now < effect.end;
        if (!shaking)
        {
            continue;
        }
        float time = static_cast<float>(now - effect.start);
        float left = 1.0f - time / static_cast<float>(SHAKE_MS);
        offset.x += SHAKE_PX * left * SDL_sinf(time * 0.12f);
        offset.y += SHAKE_PX * left * SDL_cosf(time * 0.09f);
    }
    return offset;
}

void Effects::update(Uint64 now)
{
    std::erase_if(effects_, [now](const Effect& effect)
    {
        return effect.end <= now;
    });
}

bool Effects::playing() const
{
    return !effects_.empty();
}

const std::vector<Effect>& Effects::all() const
{
    return effects_;
}

void Effects::finish()
{
    effects_.clear();
}

Uint64 Effects::nextStart(Uint64 now) const
{
    Uint64 start = now;
    for (const Effect& effect : effects_)
    {
        start = SDL_max(start, effect.end);
    }
    return start;
}
