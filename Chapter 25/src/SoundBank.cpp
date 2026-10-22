#include "SoundBank.h"
#include <string>   // std::string, for the files' paths
#include "Files.h"

namespace
{
    // Every sound is converted to this as it's loaded: 16-bit samples, one
    // channel, 48,000 a second, which most of the files are already. Then
    // any sound can go into any stream
    constexpr SDL_AudioSpec FORMAT = { SDL_AUDIO_S16, 1, 48000 };

    // The folder the sound files are in
    const std::string SOUNDS_FOLDER = "assets/sounds/";

    // The tone a sound whose file can't be loaded plays instead: a short
    // note, A above middle C, fading out, so the game still says that
    // something happened, and the missing file is easy to notice. It's
    // built a sample at a time, in the bank's format: a sine wave, as loud
    // as LOUDNESS of the loudest a 16-bit sample can be, getting quieter to
    // the end
    std::vector<Uint8> makeTone()
    {
        constexpr float PITCH = 440.0f;
        constexpr float SECONDS = 0.15f;
        constexpr float LOUDNESS = 0.3f;
        int count = static_cast<int>(FORMAT.freq * SECONDS);
        std::vector<Uint8> samples(count * sizeof(Sint16));
        for (int i = 0; i < count; ++i)
        {
            float time = static_cast<float>(i) / FORMAT.freq;
            float fade = 1.0f - static_cast<float>(i) / count;
            float wave = SDL_sinf(2.0f * SDL_PI_F * PITCH * time);
            Sint16 sample = static_cast<Sint16>(wave * fade * LOUDNESS *
                                                SDL_MAX_SINT16);
            SDL_memcpy(&samples[i * sizeof(Sint16)], &sample, sizeof(sample));
        }
        return samples;
    }
}

SoundBank::SoundBank(const GameData& data)
    : sounds_(SOUND_EVENTS)
{
    // The default device, and STREAMS streams in the bank's format, bound
    // to it. Binding sets each stream's output to whatever the device
    // wants, and SDL converts
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0)
    {
        SDL_Log("No sound: %s", SDL_GetError());
        return;
    }
    for (int i = 0; i < STREAMS; ++i)
    {
        SDL_AudioStream* stream = SDL_CreateAudioStream(&FORMAT, &FORMAT);
        if (stream != nullptr && SDL_BindAudioStream(device_, stream))
        {
            streams_.push_back(stream);
        }
        else if (stream != nullptr)
        {
            SDL_DestroyAudioStream(stream);
        }
    }

    for (const SoundDef& def : data.sounds())
    {
        for (const std::string& file : def.files)
        {
            std::string path = resourcePath(SOUNDS_FOLDER + file);
            SDL_AudioSpec spec;
            Uint8* buffer = nullptr;
            Uint32 length = 0;
            if (!SDL_LoadWAV(path.c_str(), &spec, &buffer, &length))
            {
                SDL_Log("A tone plays instead of %s, which couldn't be "
                        "loaded: %s", path.c_str(), SDL_GetError());
                sounds_[static_cast<int>(def.event)].push_back(makeTone());
                continue;
            }
            Uint8* converted = nullptr;
            int convertedLength = 0;
            bool done = SDL_ConvertAudioSamples(&spec, buffer,
                                                static_cast<int>(length),
                                                &FORMAT, &converted,
                                                &convertedLength);
            SDL_free(buffer);
            if (!done)
            {
                SDL_Log("A tone plays instead of %s, which couldn't be "
                        "converted: %s", path.c_str(), SDL_GetError());
                sounds_[static_cast<int>(def.event)].push_back(makeTone());
                continue;
            }
            sounds_[static_cast<int>(def.event)].emplace_back(
                converted, converted + convertedLength);
            SDL_free(converted);
        }
    }
}

// The streams go first, which unbinds them, and then the device
SoundBank::~SoundBank()
{
    for (SDL_AudioStream* stream : streams_)
    {
        SDL_DestroyAudioStream(stream);
    }
    if (device_ != 0)
    {
        SDL_CloseAudioDevice(device_);
    }
}

void SoundBank::play(SoundEvent event)
{
    const std::vector<Samples>& variants = sounds_[static_cast<int>(event)];
    if (streams_.empty() || variants.empty())
    {
        return;
    }

    // SDL's own dice choose the variant, not the game's: the game's dice
    // decide what happens, and a sound mustn't change that
    Sint32 choice = SDL_rand(static_cast<Sint32>(variants.size()));
    const Samples& samples = variants[choice];

    // A stream with nothing left to play takes the sound, so it plays
    // alongside the others. With every stream busy, one is cut short,
    // each in turn
    SDL_AudioStream* stream = nullptr;
    for (SDL_AudioStream* each : streams_)
    {
        if (SDL_GetAudioStreamQueued(each) == 0)
        {
            stream = each;
            break;
        }
    }
    if (stream == nullptr)
    {
        stream = streams_[next_];
        next_ = (next_ + 1) % streams_.size();
        SDL_ClearAudioStream(stream);
    }
    SDL_PutAudioStreamData(stream, samples.data(),
                           static_cast<int>(samples.size()));
}

// The device's gain scales every stream bound to it: 1 is as loud as the
// sounds were made, and 0 is silence
void SoundBank::setVolume(int percent, bool muted)
{
    if (device_ == 0)
    {
        return;
    }
    float gain = muted ? 0.0f : percent / 100.0f;
    SDL_SetAudioDeviceGain(device_, gain);
}
