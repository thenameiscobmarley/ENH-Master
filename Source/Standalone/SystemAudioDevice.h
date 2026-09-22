#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>

namespace pad
{
    /** "Windows system audio": an audio device type for the standalone app whose *inputs* are the
        computer's playback endpoints, captured with WASAPI loopback - everything the PC is playing,
        rather than a microphone - and whose outputs are the ordinary playback endpoints.

        So: the game (or anything else) plays to one endpoint, ENH Master captures that endpoint and
        plays the processed result to the headset.

        The one rule the hardware imposes: the endpoint being captured cannot be the endpoint being
        played to. Loopback captures everything rendered to an endpoint, which would include our own
        output - a feedback loop that gets louder every time round. open() refuses that combination
        and says so rather than deafening anyone. In practice, either set Windows' playback device to
        something you are not listening to (an HDMI or monitor output with nothing plugged into it
        works, and is free) and capture that, or install a virtual cable and capture that.

        Returns nullptr on platforms other than Windows. */
    std::unique_ptr<juce::AudioIODeviceType> createSystemAudioDeviceType();
}
