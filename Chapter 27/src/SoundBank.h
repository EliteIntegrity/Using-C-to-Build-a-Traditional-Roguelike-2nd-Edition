#pragma once
#include <SDL3/SDL.h>
#include <vector>   // std::vector, for the sounds and their samples
#include "Data.h"
#include "SoundEvent.h"

// The game's sounds: every file sounds.txt names, loaded once, and
// converted to one format, then played on the audio device. An event with
// several files plays one of them, chosen at random, so a hit doesn't
// sound the same every time
class SoundBank
{
public:
    // Opens the audio device, and loads the files in assets/sounds. A file
    // that can't be loaded is logged, and a tone plays in its place.
    // Without an audio device, the game plays on, in silence
    explicit SoundBank(const GameData& data);
    ~SoundBank();

    SoundBank(const SoundBank&) = delete;
    SoundBank& operator=(const SoundBank&) = delete;

    // Plays one of the event's sounds, if it has any, alongside any others
    // playing
    void play(SoundEvent event);

    // How loud every sound is, from 0 to 100, unless they're muted
    void setVolume(int percent, bool muted);

private:
    // How many sounds can play at once
    static constexpr int STREAMS = 8;

    // One sound's samples, in the bank's one format
    using Samples = std::vector<Uint8>;

    // Each event's sounds, in SoundEvent's order
    std::vector<std::vector<Samples>> sounds_;

    // The audio device, and the streams bound to it, one for each sound
    // playing, which the device mixes. Each stream plays one sound at a
    // time. next_ is the stream to cut short if they're all busy
    SDL_AudioDeviceID device_ = 0;
    std::vector<SDL_AudioStream*> streams_;
    size_t next_ = 0;
};
